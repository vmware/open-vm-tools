/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CSourcePollingChannelAdapter.h"

using namespace Caf;

CSourcePollingChannelAdapter::CSourcePollingChannelAdapter() :
	_isInitialized(false),
	_isCancelled(false),
	_isTimeoutSet(false),
	_timeout(0),
	CAF_CM_INIT_LOG("CSourcePollingChannelAdapter") {
	CAF_CM_INIT_THREADSAFE;
}

CSourcePollingChannelAdapter::~CSourcePollingChannelAdapter() {
}

void CSourcePollingChannelAdapter::initialize(
	const SmartPtrIMessageHandler& messageHandler,
	const SmartPtrIPollableChannel& inputPollableChannel,
	const SmartPtrIErrorHandler& errorHandler) {

	initialize(
			messageHandler,
			inputPollableChannel,
			errorHandler,
			0);
	_isTimeoutSet = false;
}

void CSourcePollingChannelAdapter::initialize(
	const SmartPtrIMessageHandler& messageHandler,
	const SmartPtrIPollableChannel& inputPollableChannel,
	const SmartPtrIErrorHandler& errorHandler,
	const int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(messageHandler);
	CAF_CM_VALIDATE_INTERFACE(inputPollableChannel);
	CAF_CM_VALIDATE_INTERFACE(errorHandler);

	_messageHandler = messageHandler;
	_inputPollableChannel = inputPollableChannel;
	_pollerMetadata = inputPollableChannel->getPollerMetadata();
	_errorHandler = errorHandler;
	_timeout = timeout;
	_isTimeoutSet = true;

	_isInitialized = true;
}

void CSourcePollingChannelAdapter::run() {
	CAF_CM_FUNCNAME("run");

	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	uint32 messageCount = 0;
	SmartPtrIIntMessage message;
	while (! getIsCancelled()) {
		try {
			if (_isTimeoutSet) {
				message = _inputPollableChannel->receive(_timeout);
			} else {
				message = _inputPollableChannel->receive();
			}
			if (! message.IsNull()) {
				messageCount++;
				_messageHandler->handleMessage(message);
			}
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;

		if (CAF_CM_ISEXCEPTION) {
			SmartPtrIIntMessage savedMessage = _messageHandler->getSavedMessage();
			if (savedMessage.IsNull()) {
				savedMessage = message;
			}

			SmartPtrCIntException intException;
			intException.CreateInstance();
			intException->initialize(CAF_CM_GETEXCEPTION);
			_errorHandler->handleError(intException, savedMessage);

			CAF_CM_CLEAREXCEPTION;
		}

		if (message.IsNull()
			|| (messageCount >= _pollerMetadata->getMaxMessagesPerPoll())) {
			CThreadUtils::sleep(_pollerMetadata->getFixedRate());
			messageCount = 0;
		}
	}

	CAF_CM_LOG_DEBUG_VA0("Finished");
}

void CSourcePollingChannelAdapter::cancel() {
	CAF_CM_FUNCNAME_VALIDATE("cancel");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("Canceling");
	setIsCancelled(true);
}

bool CSourcePollingChannelAdapter::getIsCancelled() const {
	CAF_CM_FUNCNAME_VALIDATE("getIsCancelled");

	bool isCancelled = false;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		isCancelled = _isCancelled;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return isCancelled;
}

void CSourcePollingChannelAdapter::setIsCancelled(
	const bool isCancelled) {
	CAF_CM_FUNCNAME_VALIDATE("setIsCancelled");

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		_isCancelled = isCancelled;
	}
	CAF_CM_UNLOCK_AND_EXIT;
}
