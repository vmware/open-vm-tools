/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Integration/Core/CBroadcastingDispatcher.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IMessageHandler.h"
#include "Exception/CCafException.h"
#include "CPublishSubscribeChannelInstance.h"

using namespace Caf;

CPublishSubscribeChannelInstance::CPublishSubscribeChannelInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CPublishSubscribeChannelInstance") {
}

CPublishSubscribeChannelInstance::~CPublishSubscribeChannelInstance() {
}

void CPublishSubscribeChannelInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_configSection = configSection;
		_id = _configSection->findRequiredAttribute("id");

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CPublishSubscribeChannelInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CPublishSubscribeChannelInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(appContext);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);

		const SmartPtrIMessageChannel errorMessageChannel =
			channelResolver->resolveChannelName("errorChannel");

		SmartPtrCErrorHandler errorHandler;
		errorHandler.CreateInstance();
		errorHandler->initialize(channelResolver, errorMessageChannel);

		SmartPtrCBroadcastingDispatcher broadcastingDispatcher;
		broadcastingDispatcher.CreateInstance();
		broadcastingDispatcher->initialize(errorHandler);
		_messageDispatcher = broadcastingDispatcher;
	}
	CAF_CM_EXIT;
}

void CPublishSubscribeChannelInstance::subscribe(
	const SmartPtrIMessageHandler& messageHandler) {
	CAF_CM_FUNCNAME_VALIDATE("subscribe");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(messageHandler);

		_messageDispatcher->addHandler(messageHandler);
	}
	CAF_CM_EXIT;
}

void CPublishSubscribeChannelInstance::unsubscribe(
	const SmartPtrIMessageHandler& messageHandler) {
	CAF_CM_FUNCNAME_VALIDATE("unsubscribe");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(messageHandler);

		_messageDispatcher->removeHandler(messageHandler);
	}
	CAF_CM_EXIT;
}

bool CPublishSubscribeChannelInstance::doSend(
		const SmartPtrIIntMessage& message,
		int32 timeout) {
	CAF_CM_FUNCNAME("doSend");

	bool sent = false;
	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(message);

		if (timeout > 0) {
			CAF_CM_EXCEPTIONEX_VA1(UnsupportedOperationException, E_INVALIDARG,
				"Timeout not currently supported: %s", _id.c_str());
		}

		CAF_CM_LOG_DEBUG_VA1("Dispatching message - %s", _id.c_str());
		sent = _messageDispatcher->dispatch(message);
		if (!sent) {
			CAF_CM_LOG_ERROR_VA1("Nothing handled the message - channel: %s", _id.c_str());
		}
	}
	CAF_CM_EXIT;
	return sent;
}
