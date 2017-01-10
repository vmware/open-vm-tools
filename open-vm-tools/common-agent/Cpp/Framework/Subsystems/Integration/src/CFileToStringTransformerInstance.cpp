/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/Core/CIntMessageHeaders.h"
#include "CFileToStringTransformerInstance.h"
#include "Integration/Core/FileHeaders.h"

using namespace Caf;

CFileToStringTransformerInstance::CFileToStringTransformerInstance() :
	_isInitialized(false),
	_deleteFiles(false),
	CAF_CM_INIT_LOG("CFileToStringTransformerInstance") {
}

CFileToStringTransformerInstance::~CFileToStringTransformerInstance() {
}

void CFileToStringTransformerInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_id = configSection->findRequiredAttribute("id");

		const std::string deleteFilesStr = configSection->findOptionalAttribute("delete-files");
		_deleteFiles = (deleteFilesStr.empty() || deleteFilesStr.compare("true") == 0) ? true : false;

		CAF_CM_LOG_DEBUG_VA2("deleteFilesStr: %s, deleteFiles: %s", deleteFilesStr.c_str(), _deleteFiles ? "true" : "false");

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CFileToStringTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CFileToStringTransformerInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(appContext);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);

	}
	CAF_CM_EXIT;
}

SmartPtrIIntMessage CFileToStringTransformerInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");

	SmartPtrIIntMessage newMessage;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		const std::string filename = message->getPayloadStr();
		const std::string fileContents = FileSystemUtils::loadTextFile(filename);

		CIntMessageHeaders messageHeaders;
		if (_deleteFiles) {
			CAF_CM_LOG_INFO_VA1("Removing file - %s", filename.c_str());
			FileSystemUtils::removeFile(filename);
		} else {
			messageHeaders.insertString(FileHeaders::_sORIGINAL_FILE, filename);
		}

		SmartPtrCIntMessage messageImpl;
		messageImpl.CreateInstance();
		messageImpl->initializeStr(fileContents, messageHeaders.getHeaders(), message->getHeaders());
		newMessage = messageImpl;
	}
	CAF_CM_EXIT;

	return newMessage;
}
