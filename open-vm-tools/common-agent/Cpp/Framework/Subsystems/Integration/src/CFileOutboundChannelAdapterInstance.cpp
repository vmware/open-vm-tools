/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Exception/CCafException.h"
#include "CFileOutboundChannelAdapterInstance.h"
#include "CDirectChannelInstance.h"
#include "Integration/Core/FileHeaders.h"

using namespace Caf;

CFileOutboundChannelAdapterInstance::CFileOutboundChannelAdapterInstance() :
	_isInitialized(false),
	_autoCreateDirectory(false),
	_deleteSourceFiles(false),
	CAF_CM_INIT_LOG("CFileOutboundChannelAdapterInstance") {
}

CFileOutboundChannelAdapterInstance::~CFileOutboundChannelAdapterInstance() {
}

void CFileOutboundChannelAdapterInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");

	const std::string directoryStr = configSection->findRequiredAttribute("directory");
	const std::string autoCreateDirectoryStr = configSection->findOptionalAttribute(
		"auto-create-directory");
	const std::string deleteSourceFilesStr = configSection->findOptionalAttribute(
		"delete-source-files");
	const std::string temporaryFileSuffixStr = configSection->findOptionalAttribute(
		"temporary-file-suffix");
	_modeStr = configSection->findOptionalAttribute("mode");

	_directory = CStringUtils::expandEnv(directoryStr);
	_deleteSourceFiles =
		(deleteSourceFilesStr.empty() || deleteSourceFilesStr.compare("false") == 0) ?
			false : true;
	_autoCreateDirectory =
		(autoCreateDirectoryStr.empty() || autoCreateDirectoryStr.compare("true") == 0) ?
			true : false;
	_temporaryFileSuffix =
		temporaryFileSuffixStr.empty() ? ".writing" : temporaryFileSuffixStr;

	_isInitialized = true;
}

std::string CFileOutboundChannelAdapterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CFileOutboundChannelAdapterInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
}

void CFileOutboundChannelAdapterInstance::handleMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("handleMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	_savedMessage = message;
	savePayloadToFile(message);
	deleteSourceFiles(message);
}

SmartPtrIIntMessage CFileOutboundChannelAdapterInstance::getSavedMessage() const {
	CAF_CM_FUNCNAME_VALIDATE("getSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _savedMessage;
}

void CFileOutboundChannelAdapterInstance::clearSavedMessage() {
	CAF_CM_FUNCNAME_VALIDATE("clearSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_savedMessage = NULL;
}

SmartPtrIIntMessage CFileOutboundChannelAdapterInstance::processErrorMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("processErrorMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	_savedMessage = message;
	savePayloadToFile(message);
	deleteSourceFiles(message);

	return SmartPtrIIntMessage();
}

void CFileOutboundChannelAdapterInstance::savePayloadToFile(
	const SmartPtrIIntMessage& message) const {
	CAF_CM_FUNCNAME("savePayloadToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	const std::string relFilename = generateFilename(message);
	const std::string filePath = FileSystemUtils::buildPath(_directory, relFilename);
	const std::string fileDir = FileSystemUtils::getDirname(filePath);

	if (! _autoCreateDirectory && ! FileSystemUtils::doesDirectoryExist(fileDir)) {
		CAF_CM_EXCEPTION_VA1(ERROR_PATH_NOT_FOUND,
			"Directory does not exist - %s", fileDir.c_str());
	}

	const SmartPtrCDynamicByteArray payload = message->getPayload();
	FileSystemUtils::saveByteFile(filePath, payload->getPtr(), payload->getByteCount(),
		translateMode(_modeStr), _temporaryFileSuffix);
}

std::string CFileOutboundChannelAdapterInstance::generateFilename(
	const SmartPtrIIntMessage& message) const {
	CAF_CM_FUNCNAME_VALIDATE("generateFilename");
	CAF_CM_VALIDATE_INTERFACE(message);

	std::string relFilename = message->findOptionalHeaderAsString(
		FileHeaders::_sFILENAME);
	if (relFilename.empty()) {
		relFilename = CStringUtils::createRandomUuid() + ".msg";
	}

	return relFilename;
}

FileSystemUtils::FILE_MODE_TYPE CFileOutboundChannelAdapterInstance::translateMode(
	const std::string modeStr) const {
	CAF_CM_FUNCNAME("translateMode");

	FileSystemUtils::FILE_MODE_TYPE modeType;
	if (modeStr.empty() || (modeStr.compare("REPLACE") == 0)) {
		modeType = FileSystemUtils::FILE_MODE_REPLACE;
	} else if (modeStr.compare("FAIL") == 0) {
		modeType = FileSystemUtils::FILE_MODE_FAIL;
	} else if (modeStr.compare("IGNORE") == 0) {
		modeType = FileSystemUtils::FILE_MODE_IGNORE;
	} else if (modeStr.compare("APPEND") == 0) {
		CAF_CM_EXCEPTION_VA0(ERROR_INVALID_DATA,
			"Invalid mode - APPEND not currently supported");
	} else {
		CAF_CM_EXCEPTION_VA1(ERROR_INVALID_DATA, "Invalid mode - %s", modeStr.c_str());
	}

	return modeType;
}

void CFileOutboundChannelAdapterInstance::deleteSourceFiles(
	const SmartPtrIIntMessage& message) const {
	CAF_CM_FUNCNAME_VALIDATE("deleteSourceFiles");
	CAF_CM_VALIDATE_INTERFACE(message);

	if (_deleteSourceFiles) {
		const std::string originalFile = message->findOptionalHeaderAsString(
			FileHeaders::_sORIGINAL_FILE);
		if (!originalFile.empty()) {
			if (FileSystemUtils::doesFileExist(originalFile)) {
				CAF_CM_LOG_INFO_VA1("Removing original file - %s", originalFile.c_str());
				FileSystemUtils::removeFile(originalFile);
			}
		}
	}
}
