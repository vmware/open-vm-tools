/*
 *  Created on: Jan 26, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Dependencies/CPollerMetadata.h"
#include "Integration/IChannelInterceptor.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/Core/CAbstractPollableChannel.h"

CAbstractPollableChannel::CAbstractPollableChannel() :
	CAF_CM_INIT("CAbstractPollableChannel") {
}

CAbstractPollableChannel::~CAbstractPollableChannel() {
}

SmartPtrIIntMessage CAbstractPollableChannel::receive() {
	return receive(0);
}

SmartPtrIIntMessage CAbstractPollableChannel::receive(const int32 timeout) {
	SmartPtrIIntMessage message;
	CAF_CM_ENTER {
		std::list<SmartPtrIChannelInterceptor> interceptors = getInterceptors();
		SmartPtrIMessageChannel channel(this);
		bool preReceiveOk = true;
		for (TSmartIterator<std::list<SmartPtrIChannelInterceptor> > interceptor(interceptors);
				interceptor && preReceiveOk;
				interceptor++) {
			preReceiveOk = interceptor->preReceive(channel);
		}

		if (preReceiveOk) {
			message = doReceive(timeout);
			for (TSmartIterator<std::list<SmartPtrIChannelInterceptor> > interceptor(interceptors);
					interceptor && message;
					interceptor++) {
				message = interceptor->postReceive(message, channel);
			}
		}
	}
	CAF_CM_EXIT;
	return message;
}

SmartPtrCPollerMetadata CAbstractPollableChannel::getPollerMetadata() const {
	CAF_CM_FUNCNAME_VALIDATE("getPollerMetadata");
	CAF_CM_VALIDATE_SMARTPTR(_pollerMetadata);
	return _pollerMetadata;
}

void CAbstractPollableChannel::setPollerMetadata(const SmartPtrCPollerMetadata& pollerMetadata) {
	CAF_CM_FUNCNAME_VALIDATE("setPollerMetadata");
	CAF_CM_VALIDATE_SMARTPTR(pollerMetadata);
	_pollerMetadata = pollerMetadata;
}

void CAbstractPollableChannel::setPollerMetadata(const SmartPtrIDocument& pollerDoc) {
	// pollerDoc is optional

	CAF_CM_ENTER {
		_pollerMetadata.CreateInstance();
		_pollerMetadata->putFixedRate(1000);
		_pollerMetadata->putMaxMessagesPerPoll(1);

		if (pollerDoc) {
			const std::string maxMessagesPerPollStr = pollerDoc->findOptionalAttribute("max-messages-per-poll");
			if (!maxMessagesPerPollStr.empty()) {
				const uint32 maxMessagesPerPoll = CStringConv::fromString<uint32>(maxMessagesPerPollStr);
				_pollerMetadata->putMaxMessagesPerPoll(maxMessagesPerPoll);
			}

			const std::string fixedRateStr = pollerDoc->findOptionalAttribute("fixed-rate");
			if (!fixedRateStr.empty()) {
				const uint32 fixedRate = CStringConv::fromString<uint32>(fixedRateStr);
				_pollerMetadata->putFixedRate(fixedRate);
			}
		}
	}
	CAF_CM_EXIT;
}
