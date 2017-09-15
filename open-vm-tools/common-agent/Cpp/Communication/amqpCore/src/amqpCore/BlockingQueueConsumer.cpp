/*
 *  Created on: Jul 30, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/Channel.h"
#include "amqpClient/api/Envelope.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpCore/BlockingQueueConsumer.h"
#include "Integration/Core/CIntException.h"
#include "Exception/CCafException.h"

using namespace Caf::AmqpIntegration;

#if (1) // BlockingQueueConsumer
BlockingQueueConsumer::BlockingQueueConsumer() :
			_isInitialized(false),
	_isRunning(false),
	_isCanceled(false),
	_deliveryQueue(NULL),
	_acknowledgeMode(ACKNOWLEDGEMODE_NONE),
	_prefetchCount(0),
	CAF_CM_INIT_LOG("BlockingQueueConsumer") {
	_parentLock.CreateInstance();
	_parentLock->initialize();
}

BlockingQueueConsumer::~BlockingQueueConsumer() {
	if (_deliveryQueue) {
		g_async_queue_unref(_deliveryQueue);
	}
}

void BlockingQueueConsumer::init(
		SmartPtrConnectionFactory connectionFactory,
		SmartPtrAmqpHeaderMapper headerMapper,
		AcknowledgeMode acknowledgeMode,
		uint32 prefetchCount,
		const std::string& queue) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(connectionFactory);
	CAF_CM_VALIDATE_INTERFACE(headerMapper);
	CAF_CM_VALIDATE_STRING(queue);

	CAF_CM_ASSERT(acknowledgeMode != ACKNOWLEDGEMODE_MANUAL);

	_connectionFactory = connectionFactory;
	_headerMapper = headerMapper;
	_acknowledgeMode = acknowledgeMode;
	_prefetchCount = prefetchCount;
	_queue = queue;
	_deliveryQueue = g_async_queue_new_full(destroyQueueItem);
	_isInitialized = true;
}

AmqpClient::SmartPtrChannel BlockingQueueConsumer::getChannel() {
	CAF_CM_FUNCNAME("getChannel");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(_isRunning);
	return _channel;
}

std::string BlockingQueueConsumer::getConsumerTag() {
	CAF_CM_FUNCNAME("getConsumerTag");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(_isRunning);
	return _consumer->getConsumerTag();
}

SmartPtrIIntMessage BlockingQueueConsumer::nextMessage() {
	CAF_CM_FUNCNAME("nextMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(_isRunning);

	// simulate a blocking pop from the queue
	gpointer data = g_async_queue_try_pop(_deliveryQueue);
	while (!data && !_isCanceled) {
		CThreadUtils::sleep(100);
		data = g_async_queue_try_pop(_deliveryQueue);
	}

	checkShutdown();
	SmartPtrIIntMessage message;
	if (data) {
		Delivery *deliveryPtr = reinterpret_cast<Delivery*>(data);
		SmartPtrDelivery delivery(deliveryPtr);
		deliveryPtr->Release();
		message = handle(delivery);
	}
	return message;
}

SmartPtrIIntMessage BlockingQueueConsumer::nextMessage(int32 timeout) {
	CAF_CM_FUNCNAME("nextMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(_isRunning);

	guint64 microTimeout = static_cast<guint64>(timeout) * 1000;
	gpointer data = g_async_queue_timeout_pop(_deliveryQueue, microTimeout);

	checkShutdown();
	SmartPtrIIntMessage message;
	if (data) {
		Delivery *deliveryPtr = reinterpret_cast<Delivery*>(data);
		SmartPtrDelivery delivery(deliveryPtr);
		deliveryPtr->Release();
		message = handle(delivery);
	}
	return message;
}

bool BlockingQueueConsumer::commitIfNecessary() {
	CAF_CM_FUNCNAME("commitIfNecessary");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(_isRunning);

	bool result = false;
	std::set<uint64> deliveryTags;
	{
		CAF_CM_LOCK_UNLOCK1(_parentLock);
		deliveryTags = _deliveryTags;
	}

	try {
		if (deliveryTags.size()) {
			result = true;
			if (_acknowledgeMode == ACKNOWLEDGEMODE_AUTO) {
				std::set<uint64>::const_reverse_iterator tag = deliveryTags.rbegin();
				CAF_CM_ASSERT(tag != deliveryTags.rend());
				CAF_CM_LOG_DEBUG_VA2(
						"basicAck [tag=%Ld][tag count=%d]",
						*tag,
						deliveryTags.size());
				_channel->basicAck(*tag, true);
			}
		}
	}
	CAF_CM_CATCH_ALL;
	{
		CAF_CM_LOCK_UNLOCK1(_parentLock);
		_deliveryTags.clear();
	}
	CAF_CM_THROWEXCEPTION;
	return result;
}

void BlockingQueueConsumer::rollbackOnExceptionIfNecessary(SmartPtrCCafException& ex) {
	CAF_CM_FUNCNAME("rollbackOnExceptionIfNecessary");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(_isRunning);

	if (_acknowledgeMode == ACKNOWLEDGEMODE_AUTO) {
		std::set<uint64> deliveryTags;
		{
			CAF_CM_LOCK_UNLOCK1(_parentLock);
			deliveryTags = _deliveryTags;
		}

		CAF_CM_LOG_DEBUG_VA2(
				"Rejecting %d messages on app exception: %s",
				deliveryTags.size(),
				ex->getMsg().c_str());

		try {
			for (TConstIterator<std::set<uint64> > tag(deliveryTags); tag; tag++) {
#ifdef FIXED
				_channel->basicReject(*tag, true);
#endif
			}
		}
		CAF_CM_CATCH_ALL;
		{
			CAF_CM_LOCK_UNLOCK1(_parentLock);
			_deliveryTags.clear();
		}
		if (CAF_CM_ISEXCEPTION) {
			CAF_CM_LOG_ERROR_VA1(
					"App exception overridden by rollback exception: "
					"%s",
					ex->getFullMsg().c_str());
			ex = NULL;
			CAF_CM_THROWEXCEPTION;
		}
	}
}

void BlockingQueueConsumer::checkShutdown() {
	CAF_CM_LOCK_UNLOCK1(_parentLock);
	if (_shutdownException) {
		_shutdownException->throwAddRefedSelf();
	}
}

void BlockingQueueConsumer::start(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("start");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(!_isRunning);

	CAF_CM_LOG_DEBUG_VA0("Starting consumer");
	_isCanceled = false;
	_connection = _connectionFactory->createConnection();
	_channel = _connection->createChannel();

	try {
		_consumer.CreateInstance();
		_consumer->init(this);

		// Set the prefetchCount if ack mode is not NONE (broker-auto)
		if (_acknowledgeMode != ACKNOWLEDGEMODE_NONE) {
			_channel->basicQos(0, _prefetchCount, false);
		}

		if (_connectionFactory->getProtocol().compare("tunnel") != 0) {
			// Verify that the queue exists
			try {
				_channel->queueDeclarePassive(_queue);
			} catch (AmqpClient::AmqpExceptions::AmqpIoErrorException *ex) {
				std::string exMsg = ex->getMsg();
				ex->Release();
				CAF_CM_EXCEPTIONEX_VA1(
						FatalListenerStartupException,
						0,
						"Cannot prepare queue for listener. "
						"Either the queue does not exist or the broker will not allow us to use it. %s",
						exMsg.c_str());
			}
		}
		CAF_CM_LOG_DEBUG_VA1("Starting on queue '%s'", _queue.c_str());
		_channel->basicConsume(
				_queue,
				_acknowledgeMode == ACKNOWLEDGEMODE_NONE,
				_consumer);
		CAF_CM_LOG_DEBUG_VA1("Started on queue '%s'", _queue.c_str());

		_isRunning = true;
	}
	CAF_CM_CATCH_ALL;
	if (CAF_CM_ISEXCEPTION) {
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		if (_channel) {
			_channel->close();
			_channel = NULL;
		}
		if (_connection) {
			_connection->close();
			_connection = NULL;
		}
	}
	CAF_CM_THROWEXCEPTION;
}

void BlockingQueueConsumer::stop(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("stop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_isCanceled = true;

	try {
		std::string consumerTag = _consumer ? _consumer->getConsumerTag() : std::string();
		if (_channel && consumerTag.length()) {
			if (_channel->isOpen()) {
					CAF_CM_LOG_DEBUG_VA1(
							"Canceling consumer '%s'",
							consumerTag.c_str());
					_channel->basicCancel(consumerTag);

				// If we are not using broker auto-ack then re-queue the messages
				if (_acknowledgeMode != ACKNOWLEDGEMODE_NONE) {
					_channel->basicRecover(true);
				}

				// Wait for the cancelOk response
				CAF_CM_LOG_DEBUG_VA0("Waiting for consumer handler to receive cancel.ok");
				uint64 start = CDateTimeUtils::getTimeMs();
				while (CDateTimeUtils::calcRemainingTime(start, timeoutMs)) {
					gpointer data = g_async_queue_try_pop(_deliveryQueue);
					if (data) {
						Delivery *delivery = reinterpret_cast<Delivery*>(data);
						if (!delivery->envelope) {
							break;
						}
						delivery->Release();
					}
				}
			}
		}
	}
	CAF_CM_CATCH_ALL;

	SmartPtrCCafException savedException;
	if (CAF_CM_ISEXCEPTION) {
		savedException = CAF_CM_GETEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}

	try {
		if (_channel) {
			_channel->close();
			_channel = NULL;
		}
		if (_connection) {
			_connection->close();
			_connection = NULL;
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	_consumer = NULL;
	_isRunning = false;
	_isCanceled = false;

	if (_shutdownException) {
		_shutdownException = NULL;
	}

	{
		CAF_CM_LOCK_UNLOCK1(_parentLock);
		_deliveryTags.clear();
	}

	if (savedException) {
		savedException->throwAddRefedSelf();
	}
}

bool BlockingQueueConsumer::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _isRunning;
}

SmartPtrIIntMessage BlockingQueueConsumer::handle(SmartPtrDelivery delivery) {
	CAF_CM_FUNCNAME_VALIDATE("handle");
	CAF_CM_VALIDATE_INTERFACE(delivery);

	IIntMessage::SmartPtrCHeaders headers = _headerMapper->toHeaders(
			delivery->properties,
			delivery->envelope);
	SmartPtrCIntMessage message;
	message.CreateInstance();
	message->initialize(delivery->body, headers, NULL);
	{
		CAF_CM_LOCK_UNLOCK1(_parentLock);
		_deliveryTags.insert(delivery->envelope->getDeliveryTag());
	}
	return message;
}

void BlockingQueueConsumer::destroyQueueItem(gpointer data) {
	reinterpret_cast<Delivery*>(data)->Release();
}
#endif

#if (1) // BlockingQueueConsumer::InternalConsumer
BlockingQueueConsumer::InternalConsumer::InternalConsumer() :
	_deliveryQueue(NULL),
	CAF_CM_INIT_LOG("InternalConsumer") {
}

BlockingQueueConsumer::InternalConsumer::~InternalConsumer() {
	if (_deliveryQueue) {
		g_async_queue_unref(_deliveryQueue);
	}
}

void BlockingQueueConsumer::InternalConsumer::init(
		BlockingQueueConsumer* parent) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_VALIDATE_PTR(parent);
	_parent = parent;
	_deliveryQueue = _parent->_deliveryQueue;
	g_async_queue_ref(_deliveryQueue);
}

void BlockingQueueConsumer::InternalConsumer::handleConsumeOk(
		const std::string& consumerTag) {
	CAF_CM_FUNCNAME_VALIDATE("handleConsumeOk");
	_consumerTag = consumerTag;
	CAF_CM_LOG_DEBUG_VA1("Received ConsumeOk - consumer='%s'", _consumerTag.c_str());
}

void BlockingQueueConsumer::InternalConsumer::handleCancelOk(
			const std::string& consumerTag) {
	CAF_CM_FUNCNAME_VALIDATE("handleCancelOk");
	CAF_CM_LOG_DEBUG_VA1("Received CancelOk - consumer='%s'", consumerTag.c_str());
	BlockingQueueConsumer::SmartPtrDelivery delivery;
	delivery.CreateInstance();
	g_async_queue_push(
			_deliveryQueue,
			delivery.GetAddRefedInterface());
}

void BlockingQueueConsumer::InternalConsumer::handleRecoverOk(
		const std::string& consumerTag) {
}

void BlockingQueueConsumer::InternalConsumer::handleDelivery(
		const std::string& consumerTag,
		const AmqpClient::SmartPtrEnvelope& envelope,
		const AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body) {
	CAF_CM_FUNCNAME_VALIDATE("handleDelivery");
	CAF_CM_VALIDATE_SMARTPTR(_parent);

	if (_parent->_isCanceled) {
		CAF_CM_LOG_DEBUG_VA0("Received message but parent is canceled.");
	} else {
		BlockingQueueConsumer::SmartPtrDelivery delivery;
		delivery.CreateInstance();
		delivery->envelope = envelope;
		delivery->properties = properties;
		delivery->body = body;
		g_async_queue_push(
				_deliveryQueue,
				delivery.GetAddRefedInterface());
		if (CAF_CM_IS_LOG_DEBUG_ENABLED) {
			CAF_CM_LOG_DEBUG_VA4(
					"Received message [exchange='%s'][rk='%s'][tag=%Lu][len=%d]",
					envelope->getExchange().c_str(),
					envelope->getRoutingKey().c_str(),
					envelope->getDeliveryTag(),
					body->getByteCount());
		}
	}
}

void BlockingQueueConsumer::InternalConsumer::handleShutdown(
		const std::string& consumerTag,
		SmartPtrCCafException& reason) {
	CAF_CM_FUNCNAME_VALIDATE("handleShutdown");
	CAF_CM_VALIDATE_SMARTPTR(_parent);
	CAF_CM_LOG_DEBUG_VA1(
			"Received shutdown signal - consumer='%s'",
			consumerTag.c_str());
	CAF_CM_LOCK_UNLOCK1(_parent->_parentLock);
	_parent->_shutdownException = reason;
	_parent->_deliveryTags.clear();
	_parent = NULL;
}

std::string BlockingQueueConsumer::InternalConsumer::getConsumerTag() {
	return _consumerTag;
}
#endif
