/*
 *	 Author: brets
 *  Created: Nov 20, 2015
 *
 *	Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CProviderExecutorRequestHandler.h"

using namespace Caf;

CProviderExecutorRequestHandler::CProviderExecutorRequestHandler() :
		_isInitialized(false),
		_isCancelled(false),
		CAF_CM_INIT_LOG("CProviderExecutorRequestHandler") {
	_taskExecutor = NULL;
}

CProviderExecutorRequestHandler::~CProviderExecutorRequestHandler() {
}

void CProviderExecutorRequestHandler::initialize(const std::string& providerUri,
		const SmartPtrITransformer beginImpersonationTransformer,
		const SmartPtrITransformer endImpersonationTransformer,
		const SmartPtrIErrorHandler errorHandler) {
	CAF_CM_FUNCNAME("initialize");

	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(providerUri);

	_providerUri = providerUri;
	UriUtils::SUriRecord providerUriRecord;
	UriUtils::parseUriString(providerUri, providerUriRecord);

	if (providerUriRecord.protocol.compare("file") != 0) {
		CAF_CM_EXCEPTIONEX_VA2(Caf::NoSuchElementException, ERROR_NOT_FOUND,
				"Unrecognized provider URI protocol - %s, %s",
				providerUriRecord.protocol.c_str(), providerUri.c_str());
	}

	UriUtils::SFileUriRecord fileUriRecord;
	UriUtils::parseFileAddress(providerUriRecord.address, fileUriRecord);
	_providerPath = fileUriRecord.path;

	if (!FileSystemUtils::doesFileExist(_providerPath)) {
		CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
				"Provider path not found - %s", _providerPath.c_str());
	}

	SmartPtrCAutoMutex mutex;
	mutex.CreateInstance();
	mutex->initialize();
	_mutex = mutex;

	_beginImpersonationTransformer = beginImpersonationTransformer;
	_endImpersonationTransformer = endImpersonationTransformer;
	_errorHandler = errorHandler;

	_isInitialized = true;
}

void CProviderExecutorRequestHandler::handleRequest(const SmartPtrCProviderExecutorRequest request) {
	CAF_CM_FUNCNAME("handleRequest");

	CAF_CM_VALIDATE_SMARTPTR(request);
	if (_providerUri.compare(request->getProviderUri()) != 0) {
		CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, ERROR_INVALID_PARAMETER,
				"Provider request not for current provider - %s", _providerUri.c_str());
	}

	CAutoMutexLockUnlock lock(_mutex);
	if (_isCancelled) {
		CAF_CM_EXCEPTIONEX_VA1(IllegalStateException, ERROR_INVALID_STATE,
				"Provider canceled: %s", _providerUri.c_str());
	}
	_pendingRequests.push_back(request);

	if (_taskExecutor == NULL) {
		SmartPtrCSimpleAsyncTaskExecutor simpleAsyncTaskExecutor;
		simpleAsyncTaskExecutor.CreateInstance();
		simpleAsyncTaskExecutor->initialize(this, _errorHandler);
		_taskExecutor = simpleAsyncTaskExecutor;
		_taskExecutor->execute(0);
	}
}

SmartPtrCProviderExecutorRequest CProviderExecutorRequestHandler::getNextPendingRequest() {
	CAutoMutexLockUnlock lock(_mutex);

	SmartPtrCProviderExecutorRequest request;
	if (_isCancelled || _pendingRequests.empty()) {
		_taskExecutor = NULL;
		return request;
	}
	request = _pendingRequests.front();
	_pendingRequests.pop_front();
	return request;
}

void CProviderExecutorRequestHandler::run() {
	CAF_CM_FUNCNAME("run");

	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrIIntMessage message;
	SmartPtrCProviderExecutorRequest request = getNextPendingRequest();
	while (!request.IsNull()) {
		try {
			processRequest(request);
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;

		if (CAF_CM_ISEXCEPTION) {
			SmartPtrCIntException intException;
			intException.CreateInstance();
			intException->initialize(CAF_CM_GETEXCEPTION);
			_errorHandler->handleError(intException, request->getInternalRequest());

			CAF_CM_CLEAREXCEPTION;
		}

		request = getNextPendingRequest();
	}
	CAF_CM_LOG_DEBUG_VA0("Finished");
}

void CProviderExecutorRequestHandler::cancel() {
	CAF_CM_FUNCNAME_VALIDATE("cancel");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("Canceling");

	CAutoMutexLockUnlock lock(_mutex);
	_isCancelled = true;
}

void CProviderExecutorRequestHandler::processRequest(
		const SmartPtrCProviderExecutorRequest& request) const {
	CAF_CM_FUNCNAME_VALIDATE("processRequest");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(request);

	const std::string& outputDir = request->getOutputDirectory();
	if (AppConfigUtils::getRequiredBoolean("managementAgent", "remap_logging_location")) {
		SmartPtrCLoggingSetter loggingSetter;
		loggingSetter.CreateInstance();
		loggingSetter->initialize(outputDir);
	}

	SmartPtrIIntMessage message = request->getInternalRequest();

	const std::string providerRequestPath = FileSystemUtils::buildPath(outputDir, _sProviderRequestFilename);
	const std::string stdoutPath = FileSystemUtils::buildPath(outputDir, _sStdoutFilename);
	const std::string stderrPath = FileSystemUtils::buildPath(outputDir, _sStderrFilename);

	std::string newProviderRequestPath = FileSystemUtils::normalizePathWithForward(
			providerRequestPath);

	// Create temporary request file for use by the provider
	CCafMessagePayload::saveToFile(message->getPayload(), newProviderRequestPath);

	Cdeqstr argv;
	argv.push_back(_providerPath);
	argv.push_back("-r");
	argv.push_back(newProviderRequestPath);

	CAF_CM_LOG_INFO_VA2("Running command - %s -r %s", _providerPath.c_str(),
			newProviderRequestPath.c_str());

	ProcessUtils::Priority priority = ProcessUtils::NORMAL;
	std::string appConfigPriority = AppConfigUtils::getOptionalString(_sManagementAgentArea, "provider_process_priority");
	if (!appConfigPriority.empty()) {
		if (CStringUtils::isEqualIgnoreCase("LOW", appConfigPriority)) {
			priority = ProcessUtils::LOW;
		} else if (CStringUtils::isEqualIgnoreCase("IDLE", appConfigPriority)) {
			priority = ProcessUtils::IDLE;
		}
	}

	// Begin impersonation
	if (!_beginImpersonationTransformer.IsNull()) {
		message = _beginImpersonationTransformer->transformMessage(message);
		if (message.IsNull()) {
			CAF_CM_LOG_WARN_VA0("Begin impersonation transform did not return a message");
		}
	}

	ProcessUtils::runSyncToFiles(argv, stdoutPath, stderrPath, priority);

	// End impersonation
	if (!_endImpersonationTransformer.IsNull()) {
		message = _endImpersonationTransformer->transformMessage(message);
		if (message.IsNull()) {
			CAF_CM_LOG_WARN_VA0("End impersonation transform did not return a message");
		}
	}

	// Delete temporary request file used by the provider
	if (FileSystemUtils::doesFileExist(newProviderRequestPath)) {
		CAF_CM_LOG_INFO_VA1("Removing handler produced request file - %s", newProviderRequestPath.c_str());
		FileSystemUtils::removeFile(newProviderRequestPath);
	}

	// Delete original request
	const std::string originalFile = message->findOptionalHeaderAsString(FileHeaders::_sORIGINAL_FILE);
	if (!originalFile.empty()) {
		if (FileSystemUtils::doesFileExist(originalFile)) {
			CAF_CM_LOG_INFO_VA1("Removing original file - %s", originalFile.c_str());
			FileSystemUtils::removeFile(originalFile);
		}
	}

	// Package response in envelope and write to global response location
	const SmartPtrCProviderRequestDoc providerRequest =
			CCafMessagePayloadParser::getProviderRequest(message->getPayload());
	const SmartPtrCResponseDoc response = CResponseFactory::createResponse(providerRequest, outputDir);

	const std::string relFilename = CStringUtils::createRandomUuid() + "_" + _sResponseFilename;

	SmartPtrIIntMessage responseMessage = CCafMessageCreator::createPayloadEnvelope(
			response, relFilename, message->getHeaders());

	const std::string directory = AppConfigUtils::getRequiredString("response_dir");
	const std::string filePath = FileSystemUtils::buildPath(directory, relFilename);

	const SmartPtrCDynamicByteArray payload = responseMessage->getPayload();
	FileSystemUtils::saveByteFile(filePath, payload->getPtr(), payload->getByteCount(),
			FileSystemUtils::FILE_MODE_REPLACE, ".writing");
}
