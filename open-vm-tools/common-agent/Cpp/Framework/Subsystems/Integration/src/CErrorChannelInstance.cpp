/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CErrorChannelInstance.h"

using namespace Caf;

CErrorChannelInstance::CErrorChannelInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CErrorChannelInstance") {
}

CErrorChannelInstance::~CErrorChannelInstance() {
}

void CErrorChannelInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		// configSection is optional

		_id = "errorChannel";

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CErrorChannelInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CErrorChannelInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");

	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(appContext);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);

		const SmartPtrIMessageChannel nullMessageChannel =
			channelResolver->resolveChannelName("nullChannel");

		SmartPtrCErrorHandler errorHandler;
		errorHandler.CreateInstance();
		errorHandler->initialize(channelResolver, nullMessageChannel);

		SmartPtrCUnicastingDispatcher unicastingDispatcher;
		unicastingDispatcher.CreateInstance();
		unicastingDispatcher->initialize(errorHandler);
		_messageDispatcher = unicastingDispatcher;
	}
	CAF_CM_EXIT;
}

void CErrorChannelInstance::subscribe(const SmartPtrIMessageHandler& messageHandler) {
	CAF_CM_FUNCNAME_VALIDATE("subscribe");

	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(messageHandler);

		_messageDispatcher->addHandler(messageHandler);
	}
	CAF_CM_EXIT;
}

void CErrorChannelInstance::unsubscribe(const SmartPtrIMessageHandler& messageHandler) {
	CAF_CM_FUNCNAME_VALIDATE("unsubscribe");

	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(messageHandler);

		_messageDispatcher->removeHandler(messageHandler);
	}
	CAF_CM_EXIT;
}

bool CErrorChannelInstance::doSend(
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
