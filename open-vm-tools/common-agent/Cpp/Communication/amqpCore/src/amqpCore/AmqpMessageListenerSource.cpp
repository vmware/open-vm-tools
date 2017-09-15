/*
 *  Created on: Aug 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Core/CIntMessage.h"
#include "Integration/Dependencies/CPollerMetadata.h"
#include "Integration/IIntMessage.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpCore/AmqpMessageListenerSource.h"
#include "Exception/CCafException.h"

using namespace Caf::AmqpIntegration;

AmqpMessageListenerSource::AmqpMessageListenerSource() :
	_isInitialized(false),
	_messageQueue(NULL),
	CAF_CM_INIT("AmqpMessageListenerSource") {
}

AmqpMessageListenerSource::~AmqpMessageListenerSource() {
}

void AmqpMessageListenerSource::init(
		const SmartPtrAmqpHeaderMapper& headerMapper,
		const SmartPtrCPollerMetadata& pollerMetadata) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_PTR(pollerMetadata);
	// headerMapper is optional

	_headerMapper = headerMapper;
	_messageQueue = g_async_queue_new_full(QueueItemDestroyFunc);
	setPollerMetadata(pollerMetadata);
	_isInitialized = true;
}
void AmqpMessageListenerSource::onMessage(const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("onMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	SmartPtrIIntMessage queuedMessage = message;
	if (_headerMapper) {
		SmartPtrCIntMessage filteredMessage;
		filteredMessage.CreateInstance();
		filteredMessage->initialize(
				queuedMessage->getPayload(),
				_headerMapper->filterHeaders(queuedMessage->getHeaders()),
				NULL);
		queuedMessage = filteredMessage;
	}
	g_async_queue_push(
			_messageQueue,
			queuedMessage.GetAddRefedInterface());
}

bool AmqpMessageListenerSource::doSend(
		const SmartPtrIIntMessage& message,
		int32 timeout) {
	CAF_CM_FUNCNAME("doSend");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_EXCEPTIONEX_VA1(
			UnsupportedOperationException,
			E_NOTIMPL,
			"This is not a sending channel: %s", _id.c_str());
	return false;
}

SmartPtrIIntMessage AmqpMessageListenerSource::doReceive(const int32 timeout) {
	CAF_CM_FUNCNAME("doReceive");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	gpointer data = NULL;
	if (timeout < 0) {
		// blocking
		CAF_CM_EXCEPTIONEX_VA1(UnsupportedOperationException, E_INVALIDARG,
			"Infinite blocking is not supported for a polled channel: %s", _id.c_str());
		//data = g_async_queue_pop(_messageQueue);
	} else if (timeout == 0) {
		// immediate
		data = g_async_queue_try_pop(_messageQueue);
	} else {
		// timed
		guint64 microTimeout = static_cast<guint64>(timeout) * 1000;
		data = g_async_queue_timeout_pop(_messageQueue, microTimeout);
	}

	SmartPtrIIntMessage message;
	if (data) {
		IIntMessage *messagePtr = reinterpret_cast<IIntMessage*>(data);
		message = messagePtr;
		messagePtr->Release();
	}
	return message;
}

void AmqpMessageListenerSource::QueueItemDestroyFunc(gpointer data) {
	reinterpret_cast<IIntMessage*>(data)->Release();
}
