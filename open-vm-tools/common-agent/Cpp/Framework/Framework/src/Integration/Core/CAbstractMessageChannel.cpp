/*
 *  Created on: Jan 26, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IChannelInterceptor.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/Core/CAbstractMessageChannel.h"

CAbstractMessageChannel::CAbstractMessageChannel() :
	CAF_CM_INIT("CAbstractMessageChannel") {
}

CAbstractMessageChannel::~CAbstractMessageChannel() {
}

bool CAbstractMessageChannel::send(
		const SmartPtrIIntMessage& message) {
	return send(message, -1);
}

bool CAbstractMessageChannel::send(
		const SmartPtrIIntMessage& message,
		const int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_VALIDATE_INTERFACE(message);

	// Call all interceptors' preSend()
	SmartPtrIIntMessage intMessage = message;
	SmartPtrIMessageChannel channel(this);
	for (TSmartIterator<std::list<SmartPtrIChannelInterceptor> > interceptor(_interceptors);
			interceptor;
			interceptor++) {
		intMessage = interceptor->preSend(intMessage, channel);
		if (!intMessage) {
			break;
		}
	}

	// If an interceptor returned NULL then we're done
	bool sent = false;
	if (intMessage) {
		// Send the message through the subclass doSend()
		sent = doSend(intMessage, timeout);

		// Call the interceptors' postSend()
		for (TSmartIterator<std::list<SmartPtrIChannelInterceptor> > interceptor(_interceptors);
				interceptor;
				interceptor++) {
			interceptor->postSend(intMessage, channel, sent);
		}
	}

	return sent;
}

void CAbstractMessageChannel::setInterceptors(
		const IChannelInterceptorSupport::InterceptorCollection& interceptors) {
	_interceptors = interceptors;
}

std::list<SmartPtrIChannelInterceptor> CAbstractMessageChannel::getInterceptors() const {
	return _interceptors;
}
