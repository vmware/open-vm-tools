/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/Core/CUnicastingDispatcher.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IMessageHandler.h"
#include "Exception/CCafException.h"
#include "CDirectChannelInstance.h"

using namespace Caf;

CDirectChannelInstance::CDirectChannelInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CDirectChannelInstance"){
}

CDirectChannelInstance::~CDirectChannelInstance() {
}

void CDirectChannelInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_configSection = configSection;
	_id = _configSection->findRequiredAttribute("id");

	_isInitialized = true;
}

std::string CDirectChannelInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CDirectChannelInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	const SmartPtrIMessageChannel errorMessageChannel =
		channelResolver->resolveChannelName("errorChannel");

	SmartPtrCErrorHandler errorHandler;
	errorHandler.CreateInstance();
	errorHandler->initialize(channelResolver, errorMessageChannel);

	SmartPtrCUnicastingDispatcher unicastingDispatcher;
	unicastingDispatcher.CreateInstance();
	unicastingDispatcher->initialize(errorHandler);
	_messageDispatcher = unicastingDispatcher;
}

void CDirectChannelInstance::subscribe(
	const SmartPtrIMessageHandler& messageHandler) {
	CAF_CM_FUNCNAME_VALIDATE("subscribe");

	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(messageHandler);

	_messageDispatcher->addHandler(messageHandler);
}

void CDirectChannelInstance::unsubscribe(
	const SmartPtrIMessageHandler& messageHandler) {
	CAF_CM_FUNCNAME_VALIDATE("unsubscribe");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(messageHandler);

	_messageDispatcher->removeHandler(messageHandler);
}

bool CDirectChannelInstance::doSend(
		const SmartPtrIIntMessage& message,
		int32 timeout) {
	CAF_CM_FUNCNAME("doSend");

	bool sent = false;
	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(message);

		if (timeout > 0) {
			CAF_CM_EXCEPTIONEX_VA1(UnsupportedOperationException, E_INVALIDARG,
				"Timeout not currently supported: %s", _id.c_str());
		} else {
			CAF_CM_LOG_DEBUG_VA1("Dispatching message - %s", _id.c_str());
			sent = _messageDispatcher->dispatch(message);
			if (!sent) {
				CAF_CM_LOG_WARN_VA1("Nothing handled the message - channel: %s", _id.c_str());
			}
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
	return sent;
}
