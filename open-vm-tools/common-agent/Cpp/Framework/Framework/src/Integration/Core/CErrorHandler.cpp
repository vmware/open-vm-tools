/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Core/CIntMessage.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IThrowable.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/Core/CIntMessageHeaders.h"
#include "Integration/Core/MessageHeaders.h"

using namespace Caf;

CErrorHandler::CErrorHandler() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CErrorHandler") {
}

CErrorHandler::~CErrorHandler() {
}

void CErrorHandler::initialize(
	const SmartPtrIChannelResolver& channelResolver,
	const SmartPtrIMessageChannel& errorMessageChannel) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);
		CAF_CM_VALIDATE_INTERFACE(errorMessageChannel);

		_channelResolver = channelResolver;
		_errorMessageChannel = errorMessageChannel;

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

void CErrorHandler::handleError(
	const SmartPtrIThrowable& throwable,
	const SmartPtrIIntMessage& message) const {
	CAF_CM_FUNCNAME_VALIDATE("handleError");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(throwable);
		// message is optional

		bool isThrowable = false;
		if (! message.IsNull()) {
			const std::string isThrowableStr =
				message->findOptionalHeaderAsString(MessageHeaders::_sIS_THROWABLE);
			isThrowable =
				(isThrowableStr.empty() || (isThrowableStr.compare("false") == 0)) ? false : true;
		}

		if (isThrowable) {
			CAF_CM_LOG_ERROR_VA2("Error already handled - MsgErr: %s, Thrown: %s",
				message->getPayloadStr().c_str(), throwable->getFullMsg().c_str());
		} else {
			CIntMessageHeaders messageHeaders;
			messageHeaders.insertString(MessageHeaders::_sIS_THROWABLE, "true");

			const IIntMessage::SmartPtrCHeaders origHeaders =
				message.IsNull() ? IIntMessage::SmartPtrCHeaders() : message->getHeaders();

			SmartPtrCIntMessage messageImpl;
			messageImpl.CreateInstance();
			messageImpl->initializeStr(throwable->getFullMsg(),
				messageHeaders.getHeaders(), origHeaders);

			const SmartPtrIIntMessage newMessage = messageImpl;

			const std::string errorChannelRef =
				newMessage->findOptionalHeaderAsString(MessageHeaders::_sERROR_CHANNEL);

			SmartPtrIMessageChannel errorMessageChannel = _errorMessageChannel;
			if (! errorChannelRef.empty()) {
				CAF_CM_LOG_INFO_VA1("Using alternate error channel - %s", errorChannelRef.c_str());
				errorMessageChannel = _channelResolver->resolveChannelName(errorChannelRef);
			}

			errorMessageChannel->send(newMessage);
		}
	}
	CAF_CM_EXIT;
}
