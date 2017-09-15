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
#include "Exception/CCafException.h"
#include "CQueueChannelInstance.h"

using namespace Caf;

CQueueChannelInstance::CQueueChannelInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CQueueChannelInstance") {
	CAF_CM_INIT_THREADSAFE;
}

CQueueChannelInstance::~CQueueChannelInstance() {
}

void CQueueChannelInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_configSection = configSection;
		_id = _configSection->findRequiredAttribute("id");

		setPollerMetadata(_configSection->findOptionalChild("poller"));

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CQueueChannelInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string id;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		id = _id;
	}
	CAF_CM_EXIT;

	return id;
}

void CQueueChannelInstance::wire(
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

bool CQueueChannelInstance::doSend(
	const SmartPtrIIntMessage& message,
	int32 timeout) {
	CAF_CM_FUNCNAME("doSend");

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(message);

		if (timeout > 0) {
			CAF_CM_EXCEPTIONEX_VA1(UnsupportedOperationException, E_INVALIDARG,
				"Queue channel with timeout not currently supported: %s", _id.c_str());
		}

		CAF_CM_LOG_DEBUG_VA2("Queueing message %d - %s", _messageQueue.size(), _id.c_str());
		_messageQueue.push_front(message);
	}
	CAF_CM_UNLOCK_AND_EXIT;
	return true;
}

SmartPtrIIntMessage CQueueChannelInstance::doReceive(const int32 timeout) {
	CAF_CM_FUNCNAME("doReceive");

	SmartPtrIIntMessage message;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		if (timeout > 0) {
			CAF_CM_EXCEPTIONEX_VA1(UnsupportedOperationException, E_INVALIDARG,
				"Queue channel with timeout not currently supported: %s", _id.c_str());
		}

		if (! _messageQueue.empty()) {
			CAF_CM_LOG_DEBUG_VA2("Receiving message %d - %s", _messageQueue.size(), _id.c_str());

			message = _messageQueue.back();
			_messageQueue.pop_back();
		}
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return message;
}
