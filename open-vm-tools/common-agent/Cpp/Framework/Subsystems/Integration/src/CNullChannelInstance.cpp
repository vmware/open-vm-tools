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
#include "CNullChannelInstance.h"

using namespace Caf;

CNullChannelInstance::CNullChannelInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CNullChannelInstance") {
}

CNullChannelInstance::~CNullChannelInstance() {
}

void CNullChannelInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		// configSection is optional

		_id = "nullChannel";

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CNullChannelInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CNullChannelInstance::wire(
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

bool CNullChannelInstance::doSend(
	const SmartPtrIIntMessage& message,
	int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("doSend");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(message);

		CAF_CM_LOG_DEBUG_VA1("Received message - %s", message->getPayloadStr().c_str());
	}
	CAF_CM_EXIT;
	return true;
}
