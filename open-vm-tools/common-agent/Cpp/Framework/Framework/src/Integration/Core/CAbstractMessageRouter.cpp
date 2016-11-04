/*
 *  Created on: Aug 9, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/Core/CIntException.h"
#include "Exception/CCafException.h"
#include "Integration/Core/CAbstractMessageRouter.h"

using namespace Caf;

CAbstractMessageRouter::CAbstractMessageRouter() :
	_ignoreSendFailures(false),
	_sendTimeout(-1),
	_isInitialized(false),
	CAF_CM_INIT_LOG("CAbstractMessageRouter") {
}

CAbstractMessageRouter::~CAbstractMessageRouter() {
}

void CAbstractMessageRouter::init() {
	init(SmartPtrIMessageChannel(), false, -1);
}

void CAbstractMessageRouter::init(
		const SmartPtrIMessageChannel& defaultOutputChannel,
		const bool ignoreSendFailures,
		const int32 sendTimeout) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	// defaultOutputChannel may be NULL

	_defaultOutputChannel = defaultOutputChannel;
	_ignoreSendFailures = ignoreSendFailures;
	_sendTimeout = sendTimeout;
	_isInitialized = true;
}

void CAbstractMessageRouter::routeMessage(const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("routeMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	bool isSent = false;

	ChannelCollection channels;
	try {
		channels = getTargetChannels(message);
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	for (TSmartConstIterator<ChannelCollection> channelIter(channels);
			channelIter;
			channelIter++) {
		try {
			SmartPtrIMessageChannel channel(*channelIter);
			CAF_CM_VALIDATE_INTERFACE(channel);
			channel->send(message, _sendTimeout);
			isSent = true;
		}
		CAF_CM_CATCH_ALL;
		if (CAF_CM_ISEXCEPTION && _ignoreSendFailures) {
			CAF_CM_LOG_DEBUG_CAFEXCEPTION;
			CAF_CM_CLEAREXCEPTION;
		}
		CAF_CM_THROWEXCEPTION;
	}

	if (!isSent) {
		if (_defaultOutputChannel) {
			_defaultOutputChannel->send(message, _sendTimeout);
		} else {
			if (channels.size()) {
				CAF_CM_EXCEPTIONEX_VA0(
						MessageDeliveryException,
						0,
						"failed to send message to resolved channel(s) "
						"and no default output channel defined");
			} else {
				CAF_CM_EXCEPTIONEX_VA0(
						MessageDeliveryException,
						0,
						"no channel resolved by router and no default "
						"output channel defined");
			}
		}
	}
}
