/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/ConsumerWorkService.h"
#include "amqpClient/api/Consumer.h"
#include "amqpClient/api/Envelope.h"
#include "amqpClient/ConsumerDispatcher.h"
#include "Exception/CCafException.h"

using namespace Caf::AmqpClient;

ConsumerDispatcher::ConsumerDispatcher() :
	_isInitialized(false),
	_isShuttingDown(false),
	CAF_CM_INIT_LOG("ConsumerDispatcher") {
	CAF_CM_INIT_THREADSAFE;
}

ConsumerDispatcher::~ConsumerDispatcher() {
}

void ConsumerDispatcher::init(const SmartPtrConsumerWorkService& workService) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(workService);
	_workService = workService;
	_isInitialized = true;
}

void ConsumerDispatcher::quiesce() {
	CAF_CM_LOCK_UNLOCK;
	_isShuttingDown = true;
}

void ConsumerDispatcher::lock() {
	CAF_CM_LOCK;
}

void ConsumerDispatcher::unlock() {
	CAF_CM_UNLOCK;
}

void ConsumerDispatcher::addConsumer(
		const std::string& consumerTag,
		const SmartPtrConsumer& consumer) {
	CAF_CM_FUNCNAME("addConsumer");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(consumerTag);
	CAF_CM_VALIDATE_SMARTPTR(consumer);

	SmartPtrDispatcherTask consumerTask;
	consumerTask.CreateInstance();
	consumerTask->init(consumerTag, consumer);
	if (_consumers.insert(
			ConsumerMap::value_type(
					consumerTag,
					ConsumerItem(consumer, consumerTask))).second) {
		_workService->addWork(consumerTask);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				DuplicateElementException,
				0,
				"A consumer with consumer tag '%s' is already registered",
				consumerTag.c_str());
	}
}

void ConsumerDispatcher::removeConsumer(
		const std::string& consumerTag) {
	CAF_CM_FUNCNAME_VALIDATE("removeConsumer");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(consumerTag);

	ConsumerMap::iterator consumer = _consumers.find(consumerTag);
	if (consumer != _consumers.end()) {
		(consumer->second).second->term();
		_consumers.erase(consumer);
	}
}

SmartPtrConsumer ConsumerDispatcher::getConsumer(
		const std::string& consumerTag) {
	CAF_CM_FUNCNAME_VALIDATE("getConsumer");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(consumerTag);
	SmartPtrConsumer result;
	ConsumerMap::iterator consumer = _consumers.find(consumerTag);
	if (consumer != _consumers.end()) {
		result = (consumer->second).first;
	}
	return result;
}

void ConsumerDispatcher::handleShutdown(SmartPtrCCafException exception) {
	CAF_CM_FUNCNAME("handleShutdown");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	for (TMapIterator<ConsumerMap> consumerItem(_consumers);
			consumerItem;
			consumerItem++) {
		try {
			consumerItem->second->term();
			consumerItem->first->handleShutdown(consumerItem.getKey(), exception);
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}
	_consumers.clear();
}

void ConsumerDispatcher::handleConsumeOk(const std::string& consumerTag) {
	CAF_CM_FUNCNAME("handleConsumeOk");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	try {
		if (!_isShuttingDown) {
			SmartPtrDispatcherWorkItem workItem;
			workItem.CreateInstance();
			workItem->init(DISPATCH_ITEM_METHOD_HANDLE_CONSUME_OK);
			getConsumerItem(consumerTag).second->addWorkItem(workItem);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_THROWEXCEPTION;
}

void ConsumerDispatcher::handleCancelOk(const std::string& consumerTag) {
	CAF_CM_FUNCNAME("handleCancelOk");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	try {
		if (!_isShuttingDown) {
			SmartPtrDispatcherWorkItem workItem;
			workItem.CreateInstance();
			workItem->init(DISPATCH_ITEM_METHOD_HANDLE_CANCEL_OK);
			getConsumerItem(consumerTag).second->addWorkItem(workItem);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_THROWEXCEPTION;
}

void ConsumerDispatcher::handleRecoverOk() {
	CAF_CM_FUNCNAME("handleRecoverOk");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	try {
		if (!_isShuttingDown) {
			for (TMapIterator<ConsumerMap> consumerItem(_consumers);
					consumerItem;
					consumerItem++) {
				if (!_isShuttingDown) {
					SmartPtrDispatcherWorkItem workItem;
					workItem.CreateInstance();
					workItem->init(DISPATCH_ITEM_METHOD_HANDLE_RECOVER_OK);
					consumerItem->second->addWorkItem(workItem);
				}
			}
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_THROWEXCEPTION;
}

void ConsumerDispatcher::handleDelivery(
		const std::string& consumerTag,
		const SmartPtrEnvelope& envelope,
		const AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body) {
	CAF_CM_FUNCNAME("handleDelivery");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	try {
		if (!_isShuttingDown) {
			SmartPtrDispatcherWorkItem workItem;
			workItem.CreateInstance();
			workItem->init(
					DISPATCH_ITEM_METHOD_HANDLE_DELIVERY,
					envelope,
					properties,
					body);
			getConsumerItem(consumerTag).second->addWorkItem(workItem);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_THROWEXCEPTION;
}

ConsumerDispatcher::ConsumerItem ConsumerDispatcher::getConsumerItem(const std::string& consumerTag) {
	CAF_CM_FUNCNAME("getConsumerItem");
	ConsumerMap::const_iterator consumerItem = _consumers.find(consumerTag);
	if (consumerItem == _consumers.end()) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"Consumer '%s' is not in the collection",
				consumerTag.c_str());
	}
	return consumerItem->second;
}

#if (1) // DispatcherWorkItem
ConsumerDispatcher::DispatcherWorkItem::DispatcherWorkItem() :
	_method(DISPATCH_ITEM_METHOD_TERMINATE) {
}

void ConsumerDispatcher::DispatcherWorkItem::init(
		const DispatchItemMethod method) {
	_method = method;
}

void ConsumerDispatcher::DispatcherWorkItem::init(
		const DispatchItemMethod method,
		const SmartPtrEnvelope& envelope,
		const AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body) {
	_method = method;
	_envelope = envelope;
	_properties = properties;
	_body = body;
}

ConsumerDispatcher::DispatchItemMethod ConsumerDispatcher::DispatcherWorkItem::getMethod() const {
	return _method;
}

SmartPtrEnvelope ConsumerDispatcher::DispatcherWorkItem::getEnvelope() const {
	return _envelope;
}

AmqpContentHeaders::SmartPtrBasicProperties ConsumerDispatcher::DispatcherWorkItem::getProperties() const {
	return _properties;
}

SmartPtrCDynamicByteArray ConsumerDispatcher::DispatcherWorkItem::getBody() const {
	return _body;
}
#endif

#if (1) // DispatcherTask
ConsumerDispatcher::DispatcherTask::DispatcherTask() :
	_workItemQueue(NULL) {
}

ConsumerDispatcher::DispatcherTask::~DispatcherTask() {
	if (_workItemQueue) {
		g_async_queue_unref(_workItemQueue);
	}
}

void ConsumerDispatcher::DispatcherTask::init(
		const std::string& consumerTag,
		const SmartPtrConsumer& consumer) {
	_consumerTag = consumerTag;
	_consumer = consumer;
	_workItemQueue = g_async_queue_new_full(FreeWorkItem);
}

void ConsumerDispatcher::DispatcherTask::term() {
	SmartPtrDispatcherWorkItem workItem;
	workItem.CreateInstance();
	workItem->init(DISPATCH_ITEM_METHOD_TERMINATE);
	addWorkItem(workItem);
}

void ConsumerDispatcher::DispatcherTask::addWorkItem(
		const ConsumerDispatcher::SmartPtrDispatcherWorkItem& workItem) {
	g_async_queue_push(_workItemQueue, workItem.GetAddRefedInterface());
}

bool ConsumerDispatcher::DispatcherTask::run() {
	uint32 itemsProcessed = 0;
	bool isTerminated = false;

	gpointer data = g_async_queue_try_pop(_workItemQueue);
	while (data) {
		DispatcherWorkItem *workItemPtr =
				reinterpret_cast<DispatcherWorkItem*>(data);
		SmartPtrDispatcherWorkItem workItem(workItemPtr);
		workItemPtr->Release();
		data = NULL;

		switch (workItem->getMethod()) {
		case DISPATCH_ITEM_METHOD_HANDLE_CONSUME_OK:
			_consumer->handleConsumeOk(_consumerTag);
			break;

		case DISPATCH_ITEM_METHOD_HANDLE_CANCEL_OK:
			_consumer->handleCancelOk(_consumerTag);
			break;

		case DISPATCH_ITEM_METHOD_HANDLE_RECOVER_OK:
			_consumer->handleRecoverOk(_consumerTag);
			break;

		case DISPATCH_ITEM_METHOD_HANDLE_DELIVERY:
			_consumer->handleDelivery(
					_consumerTag,
					workItemPtr->getEnvelope(),
					workItemPtr->getProperties(),
					workItemPtr->getBody());
			break;

		case DISPATCH_ITEM_METHOD_TERMINATE:
			isTerminated = true;
			break;
		}

		if (!isTerminated && (++itemsProcessed < DEFAULT_CONSUMER_THREAD_MAX_DELIVERY_COUNT)) {
			data = g_async_queue_try_pop(_workItemQueue);
		}
	}

	return isTerminated;
}

void ConsumerDispatcher::DispatcherTask::FreeWorkItem(gpointer data) {
	CAF_CM_STATIC_FUNC_LOG_ONLY("ConsumerDispatcher::DispatcherTask","FreeWorkItem");
	DispatcherWorkItem *workItemPtr =
			reinterpret_cast<DispatcherWorkItem*>(data);
	CAF_CM_LOG_DEBUG_VA1("Clearing workItem [method=%d]", workItemPtr->getMethod());
	workItemPtr->Release();
}
#endif
