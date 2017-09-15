/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/ProviderRequestXml/ProviderRequestXmlRoots.h"
#include "Doc/DocXml/ProviderResultsXml/ProviderResultsXmlRoots.h"
#include "Doc/DocXml/ResponseXml/ResponseXmlRoots.h"

#include "Doc/DocUtils/EnumConvertersXml.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CInlineAttachmentDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectInstancesCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectInstancesDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"
#include "Doc/ProviderResultsDoc/CRequestIdentifierDoc.h"
#include "Doc/ResponseDoc/CProviderResponseDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassDoc.h"
#include "Doc/SchemaTypesDoc/CCollectMethodDoc.h"
#include "Doc/SchemaTypesDoc/CMethodDoc.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#include "CProviderDriver.h"
#include "CProviderCdifFormatter.h"
#include "CProviderRequest.h"
#include "IInvokedProvider.h"
#include "Integration/Caf/CCafMessagePayload.h"

using namespace Caf;

CProviderDriver::CProviderDriver(IInvokedProvider& provider) :
	_provider(provider),
	_schema(provider.getSchema()),
	_providerNamespace(provider.getProviderNamespace()),
	_providerName(provider.getProviderName()),
	_providerVersion(provider.getProviderVersion()),
	CAF_CM_INIT_LOG("CProviderDriver") {
}

CProviderDriver::~CProviderDriver() {
}

int CProviderDriver::processProviderCommandline(IInvokedProvider& provider, int argc, char* argv[]) {
	SmartPtrIAppConfig appConfig;
	try {
		if (g_getenv("CAF_APPCONFIG") == NULL) {
			Cdeqstr deqstr;
			deqstr.push_back("cafenv-appconfig");
			deqstr.push_back("persistence-appconfig");
			deqstr.push_back("providerFx-appconfig");
			deqstr.push_back("custom-appconfig");
			appConfig = getAppConfig(deqstr);
		} else {
			appConfig = getAppConfig();
		}
	} catch(CCafException *ex) {
		std::cerr << "CProviderDriver::processProviderCommandline() failed to initialize AppConfig:  " << ex->getFullMsg().c_str();
		ex->Release();
		return 1;
	} catch (std::exception ex) {
		std::cerr << "CProviderDriver::getAppConfig() failed to initialize AppConfig:  " << ex.what();
		return 1;
	} catch (...) {
		std::cerr << "CProviderDriver::getAppConfig() failed to initialize AppConfig:  unknown exception";
		return 1;
	}

	// NOTE:  Want to define parameters as constants
	if (argc == 0) {
		// Error
		std::cerr << "Invalid command line:  no options provided";
		return 1;
	}

	CProviderDriver driver(provider);
	return driver.processProviderCommandline(argc, argv);
}

int CProviderDriver::processProviderCommandline(int argc, char* argv[]) {
	CAF_CM_FUNCNAME("processProviderCommandline");

	_commandLineArgs.clear();
	_commandLineArgs.reserve(argc);
	for (int32 i = 0; i < argc; i++) {
		_commandLineArgs.push_back(argv[i]);
	}

	for (std::vector<std::string>::const_iterator itr = _commandLineArgs.begin(); itr != _commandLineArgs.end(); itr++) {
		if ((*itr).compare("--schema") == 0) {
			itr++;
			if (itr == _commandLineArgs.end()) {
				std::cerr << "Invalid command line:  no schema output directory provided";
				return 1;
			} else if ((*itr).compare("-o") != 0) {
				std::cerr << "Invalid command line:  unexpected option: " << (*itr).c_str();
				return 1;
			}
			itr++;
			if (itr == _commandLineArgs.end()) {
				std::cerr << "Invalid command line:  no schema output directory provided";
				return 1;
			}

			std::string outputDir = *itr;
			while (++itr != _commandLineArgs.end()) {
				outputDir += " " + *itr;
			}

			collectSchema(outputDir);
			return 0;

		} else if ((*itr).compare("-r") == 0) {
			itr++;
			if (itr == _commandLineArgs.end()) {
				std::cerr << "Invalid command line:  no request location provided";
				return 1;
			}

			std::string requestPath = *itr;
			while (++itr != _commandLineArgs.end()) {
				requestPath += " " + *itr;
			}

			try {
				executeRequest(requestPath);
				return 0;
			}
			CAF_CM_CATCH_ALL;
			CAF_CM_LOG_CRIT_CAFEXCEPTION;
			std::cerr << "Error executing request:  " << _cm_exception_->getFullMsg().c_str() << std::endl;
			CAF_CM_CLEAREXCEPTION
			return 1;
		}
	}
	std::cerr << "Invalid command line:  unknown options";
	return 1;
}

void CProviderDriver::collectSchema(const std::string& outputDir) const {
	CAF_CM_FUNCNAME_VALIDATE("collectSchema");
	CAF_CM_VALIDATE_STRING(outputDir);

	const std::string schemaStr = XmlRoots::saveSchemaToString(_schema);
	const std::string schemaFilename = _providerName + "-collectSchema-Rnd.provider-data.xml";
	const std::string schemaPath = FileSystemUtils::buildPath(outputDir, schemaFilename);
	FileSystemUtils::saveTextFile(schemaPath, schemaStr);
	CAF_CM_LOG_DEBUG_VA1("Saved schema file - %s", schemaPath.c_str());

	saveProviderResponse(schemaPath);
}

void CProviderDriver::executeRequest(const std::string& requestPath) const {
	CAF_CM_FUNCNAME("executeRequest");
	CAF_CM_VALIDATE_STRING(requestPath);

	// TODO:  Verify file exists
	SmartPtrCDynamicByteArray fileContents = CCafMessagePayload::createBufferFromFile(requestPath);
	std::string providerRequestXml(reinterpret_cast<const char*>(fileContents->getPtr()));

	const SmartPtrCProviderRequestDoc request = XmlRoots::parseProviderRequestFromString(providerRequestXml);

	bool isProviderCalled = false;
	const SmartPtrCProviderCollectInstancesCollectionDoc collectInstancesCollection = request->getBatch()->getCollectInstancesCollection();
	if (! collectInstancesCollection.IsNull()) {
		const std::deque<SmartPtrCProviderCollectInstancesDoc> instances = collectInstancesCollection->getCollectInstances();
		for(TConstIterator<std::deque<SmartPtrCProviderCollectInstancesDoc> > iter(instances); iter; iter++) {
			isProviderCalled = true;
			executeCollectInstances(request, *iter);
		}
	}

	const SmartPtrCProviderInvokeOperationCollectionDoc invokeOperationCollection = request->getBatch()->getInvokeOperationCollection();
	if (! invokeOperationCollection.IsNull()) {
		const std::deque<SmartPtrCProviderInvokeOperationDoc> operations = invokeOperationCollection->getInvokeOperation();
		for(TConstIterator<std::deque<SmartPtrCProviderInvokeOperationDoc> > iter(operations); iter; iter++) {
			isProviderCalled = true;
			executeInvokeOperation(request, *iter);
		}
	}

	if (! isProviderCalled) {
		CAF_CM_EXCEPTIONEX_VA3(NoSuchElementException, ERROR_NOT_FOUND,
			"Did not call anything on the provider - %s::%s::%s",
			_providerNamespace.c_str(), _providerName.c_str(), _providerVersion.c_str());
	}
}

void CProviderDriver::executeCollectInstances(
	const SmartPtrCProviderRequestDoc request,
	const SmartPtrCProviderCollectInstancesDoc doc) const {
	CAF_CM_FUNCNAME_VALIDATE("executeCollectInstances");
	CAF_CM_VALIDATE_SMARTPTR(request);
	CAF_CM_VALIDATE_SMARTPTR(doc);

	const SmartPtrCActionClassDoc actionClass = findActionClass(doc->getClassNamespace(),
			doc->getClassName(), doc->getClassVersion(), "collectInstances");

	const SmartPtrCRequestIdentifierDoc requestId = createRequestId(request, actionClass, doc->getJobId());

	const std::string outputFilename = _providerName + std::string("-collectInstances.provider-data.xml");
	const std::string outputFilePath = FileSystemUtils::buildPath(doc->getOutputDir(), outputFilename);

	CProviderCdifFormatter formatter;
	CAF_CM_LOG_DEBUG_VA1("Initializing formatter with path - %s", outputFilePath.c_str());
	formatter.initialize(requestId, _schema, outputFilePath);

	CAF_CM_LOG_DEBUG_VA1("Calling collect on the provider - %s", _providerName.c_str());
	CProviderRequest providerRequest(request, _commandLineArgs);
	providerRequest.setCollectInstances(doc);
	_provider.collect(providerRequest, formatter);
	formatter.finished();
}

void CProviderDriver::executeInvokeOperation(
	const SmartPtrCProviderRequestDoc request,
	const SmartPtrCProviderInvokeOperationDoc doc) const {
	CAF_CM_FUNCNAME_VALIDATE("executeInvokeOperation");
	CAF_CM_VALIDATE_SMARTPTR(request);
	CAF_CM_VALIDATE_SMARTPTR(doc);

	const SmartPtrCActionClassDoc actionClass = findActionClass(doc->getClassNamespace(),
			doc->getClassName(), doc->getClassVersion(), doc->getOperation()->getName());

	const SmartPtrCRequestIdentifierDoc requestId = createRequestId(request, actionClass, doc->getJobId());

	const std::string outputFilename = _providerName + std::string("-invokeOperation.provider-data.xml");
	const std::string outputFilePath = FileSystemUtils::buildPath(doc->getOutputDir(), outputFilename);

	CProviderCdifFormatter formatter;
	CAF_CM_LOG_DEBUG_VA1("Initializing formatter with path - %s", outputFilePath.c_str());
	formatter.initialize(requestId, _schema, outputFilePath);

	CAF_CM_LOG_DEBUG_VA1("Calling invoke on the provider - %s", _providerName.c_str());
	CProviderRequest providerRequest(request, _commandLineArgs);
	providerRequest.setInvokeOperations(doc);
	_provider.invoke(providerRequest, formatter);
	formatter.finished();
}

SmartPtrCRequestIdentifierDoc CProviderDriver::createRequestId(
	const SmartPtrCProviderRequestDoc request,
	const SmartPtrCActionClassDoc actionClass,
	const UUID jobId) const {
	CAF_CM_FUNCNAME_VALIDATE("createRequestId");
	CAF_CM_VALIDATE_SMARTPTR(request);
	CAF_CM_VALIDATE_SMARTPTR(actionClass);
	CAF_CM_VALIDATE_GUID(jobId);

	SmartPtrCRequestIdentifierDoc requestIdentifier;
	requestIdentifier.CreateInstance();
	requestIdentifier->initialize(
		request->getClientId(),
		request->getRequestId(),
		request->getPmeId(),
		jobId,
		actionClass,
		CAFCOMMON_GUID_NULL);

	return requestIdentifier;
}

SmartPtrCActionClassDoc CProviderDriver::findActionClass(
	const std::string srchClassNamespace,
	const std::string srchClassName,
	const std::string srchClassVersion,
	const std::string srchOperationName) const {
	CAF_CM_FUNCNAME("findActionClass");
	CAF_CM_VALIDATE_STRING(srchClassNamespace);
	CAF_CM_VALIDATE_STRING(srchClassName);
	CAF_CM_VALIDATE_STRING(srchClassVersion);
	CAF_CM_VALIDATE_STRING(srchOperationName);

	const std::deque<SmartPtrCActionClassDoc> actionClassCollection = _schema->getActionClassCollection();
	CAF_CM_VALIDATE_STL(actionClassCollection);

	SmartPtrCActionClassDoc actionClassRc;
	for(TConstIterator<std::deque<SmartPtrCActionClassDoc> > actionClassIter(actionClassCollection);
		actionClassIter; actionClassIter++) {
		const SmartPtrCActionClassDoc actionClass = *actionClassIter;

		const std::string actionClassNamespace = actionClass->getNamespaceVal();
		const std::string actionClassName = actionClass->getName();
		const std::string actionClassVersion = actionClass->getVersion();

		if ((srchClassNamespace.compare(actionClassNamespace) == 0) &&
			(srchClassName.compare(actionClassName) == 0) &&
			(srchClassVersion.compare(actionClassVersion) == 0)) {

			const SmartPtrCCollectMethodDoc actionCollectMethod = actionClass->getCollectMethod();
			if (! actionCollectMethod.IsNull() && (srchOperationName.compare(actionCollectMethod->getName()) == 0)) {
				actionClassRc.CreateInstance();
				actionClassRc->initialize(
					actionClassNamespace,
					actionClassName,
					actionClassVersion,
					actionCollectMethod,
					std::deque<SmartPtrCMethodDoc>(),
					actionClass->getDisplayName(),
					actionClass->getDescription());
				break;
			} else {
				const std::deque<SmartPtrCMethodDoc> actionMethodCollection = actionClass->getMethodCollection();
				for(TConstIterator<std::deque<SmartPtrCMethodDoc> > actionMethodIter(actionMethodCollection);
					actionMethodIter; actionMethodIter++) {
					const SmartPtrCMethodDoc actionMethod = *actionMethodIter;

					if (srchOperationName.compare(actionMethod->getName()) == 0) {
						std::deque<SmartPtrCMethodDoc> actionMethodCollection;
						actionMethodCollection.push_back(actionMethod);

						actionClassRc.CreateInstance();
						actionClassRc->initialize(
							actionClassNamespace,
							actionClassName,
							actionClassVersion,
							SmartPtrCCollectMethodDoc(),
							actionMethodCollection,
							actionClass->getDisplayName(),
							actionClass->getDescription());
						break;
					}
				}
			}
		}
	}

	if (actionClassRc.IsNull()) {
		CAF_CM_EXCEPTIONEX_VA4(NoSuchElementException, ERROR_NOT_FOUND,
			"Action Class not found - %s::%s::%s::%s",
			srchClassNamespace.c_str(), srchClassName.c_str(), srchClassVersion.c_str(), srchOperationName.c_str());
	}

	return actionClassRc;
}

void CProviderDriver::saveProviderResponse(const std::string attachmentFilePath) const {
	CAF_CM_FUNCNAME_VALIDATE("saveProviderResponse");
	CAF_CM_VALIDATE_STRING(attachmentFilePath);

	const std::string attachmentName = FileSystemUtils::getBasename(attachmentFilePath);
	const std::string attachmentDirPath = FileSystemUtils::getDirname(attachmentFilePath);
	const std::string attachmentFilePathTmp =
		FileSystemUtils::normalizePathWithForward(attachmentFilePath);

	const std::string cmsPolicyStr = AppConfigUtils::getRequiredString(
			"security", "cms_policy");;

	SmartPtrCAttachmentDoc attachment;
	attachment.CreateInstance();
	attachment->initialize(attachmentName, "cdif",
		"file:///" + attachmentFilePathTmp, false,
		EnumConvertersXml::convertStringToCmsPolicy(cmsPolicyStr));

	std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInner;
	attachmentCollectionInner.push_back(attachment);

	SmartPtrCAttachmentCollectionDoc attachmentCollection;
	attachmentCollection.CreateInstance();
	attachmentCollection->initialize(attachmentCollectionInner,
		std::deque<SmartPtrCInlineAttachmentDoc>());

	const UUID clientId = CAFCOMMON_GUID_NULL;
	const UUID requestId = CAFCOMMON_GUID_NULL;
	const std::string pmeId = BasePlatform::UuidToString(CAFCOMMON_GUID_NULL);

	SmartPtrCProviderResponseDoc providerResponse;
	providerResponse.CreateInstance();
	providerResponse->initialize(
		clientId,
		requestId,
		pmeId,
		SmartPtrCResponseHeaderDoc(),
		SmartPtrCManifestDoc(),
		attachmentCollection,
		SmartPtrCStatisticsDoc());

	const std::string providerResponseStr = XmlRoots::saveProviderResponseToString(providerResponse);
	const std::string providerResponsePath = FileSystemUtils::buildPath(attachmentDirPath, _sProviderResponseFilename);
	FileSystemUtils::saveTextFile(providerResponsePath, providerResponseStr);
	CAF_CM_LOG_DEBUG_VA1("Saved provider response file - %s", providerResponsePath.c_str());
}
