/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Core/CIntException.h"
#include "Integration/IErrorHandler.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageHandler.h"
#include "Integration/IPollableChannel.h"
#include "Integration/Core/CSourcePollingChannelAdapter.h"

using namespace Caf;

CSourcePollingChannelAdapter::CSourcePollingChannelAdapter() :
	_isInitialized(false),
	_isCancelled(false),
	_isTimeoutSet(false),
	_timeout(0),
	CAF_CM_INIT_LOG("CSourcePollingChannelAdapter") {
	CAF_CM_INIT_THREADSAFE;
	CAF_THREADSIGNAL_INIT;
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

	_threadSignalCancel.initialize("Cancel");

	_isInitialized = true;
}

void CSourcePollingChannelAdapter::run() {
	CAF_CM_FUNCNAME("run");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	uint32 messageCount = 0;
	SmartPtrIIntMessage message;
	while (! getIsCancelled()) {
		try {
			message = NULL;
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
			{
				CAF_THREADSIGNAL_LOCK_UNLOCK;
//				CAF_CM_LOG_DEBUG_VA2("Wait (%s) - waitMs: %d",
//						_threadSignalCancel.getName().c_str(),
//						_pollerMetadata->getFixedRate());
				_threadSignalCancel.waitOrTimeout(
						CAF_THREADSIGNAL_MUTEX, _pollerMetadata->getFixedRate());
			}

			messageCount = 0;
		}
	}

	CAF_CM_LOG_DEBUG_VA0("Finished");
}

void CSourcePollingChannelAdapter::cancel() {
	CAF_CM_FUNCNAME_VALIDATE("cancel");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA1("Signal (%s)", _threadSignalCancel.getName().c_str());
	_isCancelled = true;
	_threadSignalCancel.signal();
}

bool CSourcePollingChannelAdapter::getIsCancelled() const {
	CAF_CM_FUNCNAME_VALIDATE("getIsCancelled");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _isCancelled;
}
