/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "ICafObject.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/Core/CMessageHandler.h"
#include "Integration/Core/CSimpleAsyncTaskExecutor.h"
#include "Integration/Core/CSourcePollingChannelAdapter.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IPollableChannel.h"
#include "Integration/ITaskExecutor.h"
#include "Integration/Core/CMessagingTemplate.h"
#include "Exception/CCafException.h"

using namespace Caf;

CMessagingTemplate::CMessagingTemplate() :
	_isInitialized(false),
	_isRunning(false),
	CAF_CM_INIT_LOG("CMessagingTemplate") {
}

CMessagingTemplate::~CMessagingTemplate() {
}

void CMessagingTemplate::initialize(
	const SmartPtrIChannelResolver& channelResolver,
	const SmartPtrIIntegrationObject& inputIntegrationObject,
	const SmartPtrIMessageChannel& errorMessageChannel,
	const SmartPtrIMessageChannel& outputMessageChannel,
	const SmartPtrICafObject& messageHandlerObj) {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
	CAF_CM_VALIDATE_INTERFACE(inputIntegrationObject);
	CAF_CM_VALIDATE_INTERFACE(errorMessageChannel);
	// outputMessageChannel is optional
	//
	// messageHandlerObj optional but if provided must be one of
	//		ITransformer
	//		IMessageProcessor
	//		IMessageSplitter
	//		IMessageRouter
	//		IMessageHandler
	//
	// messageHandlerObj may also support IErrorProcessor

	_inputId = inputIntegrationObject->getId();

	SmartPtrCMessageHandler messageHandler;
	messageHandler.CreateInstance();
	messageHandler->initialize(
		_inputId,
		outputMessageChannel,
		messageHandlerObj);

	SmartPtrIPollableChannel inputPollableChannel;
	inputPollableChannel.QueryInterface(inputIntegrationObject, false);
	_inputSubscribableChannel.QueryInterface(inputIntegrationObject, false);

	if (! _inputSubscribableChannel.IsNull()) {
		_messagingTemplateHandler.CreateInstance();
		_messagingTemplateHandler->initialize(messageHandler);
	} else if (! inputPollableChannel.IsNull()) {
		_taskExecutor = createTaskExecutor(
			channelResolver, messageHandler, inputPollableChannel, errorMessageChannel);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
			"Input object does not support any required interfaces - %s", _inputId.c_str());
	}

	_isInitialized = true;
}

void CMessagingTemplate::start(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("start");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (! _taskExecutor.IsNull()) {
		CAF_CM_LOG_DEBUG_VA1("Executing task - %s", _inputId.c_str());
		_taskExecutor->execute(timeoutMs);
	} else if (! _inputSubscribableChannel.IsNull()) {
		CAF_CM_LOG_DEBUG_VA1("Subscribing handler - %s", _inputId.c_str());
		_inputSubscribableChannel->subscribe(_messagingTemplateHandler);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(IllegalStateException, E_UNEXPECTED,
			"Nothing to start: %s", _inputId.c_str());
	}
}

void CMessagingTemplate::stop(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("stop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("Stopping");

	if (! _taskExecutor.IsNull()) {
		CAF_CM_LOG_DEBUG_VA1("Canceling task - %s", _inputId.c_str());
		_taskExecutor->cancel(timeoutMs);
	} else if (! _inputSubscribableChannel.IsNull()) {
		CAF_CM_LOG_DEBUG_VA1("Unsubscribing handler - %s", _inputId.c_str());
		_inputSubscribableChannel->unsubscribe(_messagingTemplateHandler);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(IllegalStateException, E_UNEXPECTED,
			"Nothing to stop: %s", _inputId.c_str());
	}
}

bool CMessagingTemplate::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	bool isRunning = true;
	if (! _taskExecutor.IsNull()) {
		isRunning = (_taskExecutor->getState() == ITaskExecutor::ETaskStateStarted);
	}

	return isRunning;
}

SmartPtrITaskExecutor CMessagingTemplate::createTaskExecutor(
	const SmartPtrIChannelResolver& channelResolver,
	const SmartPtrCMessageHandler& messageHandler,
	const SmartPtrIPollableChannel& inputPollableChannel,
	const SmartPtrIMessageChannel& errorMessageChannel) const {
	CAF_CM_FUNCNAME_VALIDATE("createTaskExecutor");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
	CAF_CM_VALIDATE_SMARTPTR(messageHandler);
	CAF_CM_VALIDATE_INTERFACE(inputPollableChannel);
	CAF_CM_VALIDATE_INTERFACE(errorMessageChannel);

	SmartPtrCErrorHandler errorHandler;
	errorHandler.CreateInstance();
	errorHandler->initialize(channelResolver, errorMessageChannel);

	SmartPtrCSourcePollingChannelAdapter sourcePollingChannelAdapter;
	sourcePollingChannelAdapter.CreateInstance();
	sourcePollingChannelAdapter->initialize(messageHandler, inputPollableChannel, errorHandler);

	SmartPtrCSimpleAsyncTaskExecutor simpleAsyncTaskExecutor;
	simpleAsyncTaskExecutor.CreateInstance();
	simpleAsyncTaskExecutor->initialize(sourcePollingChannelAdapter, errorHandler);

	return simpleAsyncTaskExecutor;
}
