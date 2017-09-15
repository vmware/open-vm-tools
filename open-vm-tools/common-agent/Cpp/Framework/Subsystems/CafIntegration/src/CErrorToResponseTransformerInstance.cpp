/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Caf/CCafMessageHeaders.h"
#include "Common/IAppContext.h"
#include "Doc/ResponseDoc/CErrorResponseDoc.h"
#include "Doc/ResponseDoc/CResponseHeaderDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "CErrorToResponseTransformerInstance.h"
#include "Integration/Caf/CCafMessageCreator.h"

using namespace Caf;

CErrorToResponseTransformerInstance::CErrorToResponseTransformerInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CErrorToResponseTransformerInstance") {
}

CErrorToResponseTransformerInstance::~CErrorToResponseTransformerInstance() {
}

void CErrorToResponseTransformerInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_id = configSection->findRequiredAttribute("id");

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CErrorToResponseTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CErrorToResponseTransformerInstance::wire(
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

SmartPtrIIntMessage CErrorToResponseTransformerInstance::processErrorMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("processErrorMessage");

	SmartPtrIIntMessage newMessage;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		CAF_CM_LOG_DEBUG_VA1("Called - %s", _id.c_str());

		const SmartPtrCCafMessageHeaders cafMessageHeaders =
				CCafMessageHeaders::create(message->getHeaders());

		const std::string errorMessage = message->getPayloadStr();
		const UUID clientId = cafMessageHeaders->getClientIdOpt();
		const UUID requestId = cafMessageHeaders->getRequestIdOpt();
		const std::string pmeIdStr = cafMessageHeaders->getPmeIdOpt();
		const UUID sessionId = cafMessageHeaders->getSessionIdOpt();

		const std::string version = "1.0";
		const std::string createdDateTime = CDateTimeUtils::getCurrentDateTime();
		const uint32 sequenceNumber = 0;
		const bool isFinalResponse = true;

		SmartPtrCResponseHeaderDoc responseHeader;
		responseHeader.CreateInstance();
		responseHeader->initialize(
				version,
				createdDateTime,
				sequenceNumber,
				isFinalResponse,
				sessionId);

		SmartPtrCErrorResponseDoc errorResponse;
		errorResponse.CreateInstance();
		errorResponse->initialize(
				clientId,
				requestId,
				pmeIdStr,
				responseHeader,
				errorMessage);

		const std::string randomUuidStr = CStringUtils::createRandomUuid();
		const std::string relFilename = randomUuidStr + "_" + _sErrorResponseFilename;

		newMessage = CCafMessageCreator::createPayloadEnvelope(
				errorResponse, relFilename, message->getHeaders());

		// Writing the error response for debugging purposes
		const std::string tmpDir = AppConfigUtils::getRequiredString(_sConfigTmpDir);
		FileSystemUtils::saveTextFile(tmpDir, _sErrorResponseFilename,
				newMessage->getPayloadStr());
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	return newMessage;
}
