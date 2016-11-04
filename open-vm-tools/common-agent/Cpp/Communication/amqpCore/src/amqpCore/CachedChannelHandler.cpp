/*
 *  Created on: Jun 1, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Exception/CCafException.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/AmqpMethods.h"
#include "amqpClient/api/Channel.h"
#include "amqpClient/api/Consumer.h"
#include "amqpClient/api/GetResponse.h"
#include "amqpClient/api/ReturnListener.h"
#include "amqpClient/api/amqpClient.h"
#include "amqpCore/CachingConnectionFactory.h"

using namespace Caf::AmqpIntegration;

CachingConnectionFactory::CachedChannelHandler::CachedChannelHandler() :
	_parent(NULL),
	CAF_CM_INIT_LOG("CachingConnectionFactory::CachedChannelHandler") {
	CAF_CM_INIT_THREADSAFE;
}

CachingConnectionFactory::CachedChannelHandler::~CachedChannelHandler() {
}

void CachingConnectionFactory::CachedChannelHandler::init(
		CachingConnectionFactory *parent,
		AmqpClient::SmartPtrChannel channel) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_VALIDATE_PTR(parent);
	CAF_CM_VALIDATE_SMARTPTR(channel);

	_parent = parent;
	_channel = channel;
}

void CachingConnectionFactory::CachedChannelHandler::logicalClose() {
	if (_channel && !_channel->isOpen()) {
		_channel = NULL;
	} else {
		// Allow for multiple close calls - if this channel is already
		// in the cached channels container then noop else add it
		// to the cached channel collection
		TSmartConstIterator<ProxyDeque> proxy(*(_parent->_cachedChannels));
		while (proxy) {
			if (*proxy == this) {
				break;
			}
			proxy++;
		}
		if (!proxy) {
			_parent->_cachedChannels->push_back(this);
		}
	}
}

void CachingConnectionFactory::CachedChannelHandler::physicalClose() {
	if (_channel && _channel->isOpen()) {
			_channel->close();
		_channel = NULL;
	}
}

void CachingConnectionFactory::CachedChannelHandler::checkChannel() {
	if (!_channel || !_channel->isOpen()) {
		_channel = _parent->createBareChannel();
	}
}

void CachingConnectionFactory::CachedChannelHandler::postProcessCall(SmartPtrCCafException exception) {
	CAF_CM_FUNCNAME_VALIDATE("postProcessCall");
	if (exception) {
		if (!_channel || !_channel->isOpen()) {
			CAF_CM_LOG_DEBUG_VA0("Detected closed channel on exception. Re-initializing");
			_channel = _parent->createBareChannel();
		}
		exception->throwAddRefedSelf();
	}
}

AmqpClient::SmartPtrChannel
CachingConnectionFactory::CachedChannelHandler::getTargetChannel() {
	CAF_CM_LOCK_UNLOCK;
	return _channel;
}

void CachingConnectionFactory::CachedChannelHandler::close() {
	CAF_CM_LOCK_UNLOCK;
	bool shouldPhysicallyClose = true;
	if (_parent->_isActive) {
		CAF_CM_LOCK_UNLOCK1(_parent->_cachedChannelsMonitor);
		if (_parent->_cachedChannels->size() < _parent->getChannelCacheSize()) {
			logicalClose();
			shouldPhysicallyClose = false;
		}
	}

	if (shouldPhysicallyClose) {
		physicalClose();
	}
}

bool CachingConnectionFactory::CachedChannelHandler::isOpen() {
	CAF_CM_LOCK_UNLOCK;
	return (_channel && _channel->isOpen());
}

uint16 CachingConnectionFactory::CachedChannelHandler::getChannelNumber() {
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	return _channel->getChannelNumber();
}

void CachingConnectionFactory::CachedChannelHandler::basicAck(
	const uint64 deliveryTag,
	const bool ackMultiple) {
	CAF_CM_FUNCNAME("basicAck");
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	try {
		_channel->basicAck(deliveryTag, ackMultiple);
	}
	CAF_CM_CATCH_ALL;
	postProcessCall(CAF_CM_GETEXCEPTION);
}

AmqpClient::SmartPtrGetResponse
CachingConnectionFactory::CachedChannelHandler::basicGet(
	const std::string& queue,
	const bool noAck) {
	CAF_CM_FUNCNAME("basicAck");
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	AmqpClient::SmartPtrGetResponse response;
	try {
		response = _channel->basicGet(queue, noAck);
	}
	CAF_CM_CATCH_ALL;
	postProcessCall(CAF_CM_GETEXCEPTION);
	return response;
}

void CachingConnectionFactory::CachedChannelHandler::basicPublish(
	const std::string& exchange,
	const std::string& routingKey,
	const AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties& properties,
	const SmartPtrCDynamicByteArray& body) {
	CAF_CM_FUNCNAME("basicPublish");
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	try {
		_channel->basicPublish(exchange, routingKey, properties, body);
	}
	CAF_CM_CATCH_ALL;
	postProcessCall(CAF_CM_GETEXCEPTION);
}

void CachingConnectionFactory::CachedChannelHandler::basicPublish(
	const std::string& exchange,
	const std::string& routingKey,
	const bool mandatory,
	const bool immediate,
	const AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties& properties,
	const SmartPtrCDynamicByteArray& body) {
	CAF_CM_FUNCNAME("basicPublish");
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	try {
		 _channel->basicPublish(
			exchange,
			routingKey,
			mandatory,
			immediate,
			properties,
			body);
	}
	CAF_CM_CATCH_ALL;
	postProcessCall(CAF_CM_GETEXCEPTION);
}

AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk
CachingConnectionFactory::CachedChannelHandler::basicConsume(
		const std::string& queue,
		const AmqpClient::SmartPtrConsumer& consumer) {
	CAF_CM_FUNCNAME("basicConsume");
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk consumeOk;
	try {
		consumeOk = _channel->basicConsume(queue, consumer);
	}
	CAF_CM_CATCH_ALL;
	postProcessCall(CAF_CM_GETEXCEPTION);
	return consumeOk;
}

AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk
CachingConnectionFactory::CachedChannelHandler::basicConsume(
		const std::string& queue,
		const bool noAck,
		const AmqpClient::SmartPtrConsumer& consumer) {
	CAF_CM_FUNCNAME("basicConsume");
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk consumeOk;
	try {
		consumeOk = _channel->basicConsume(queue, noAck, consumer);
	}
	CAF_CM_CATCH_ALL;
	postProcessCall(CAF_CM_GETEXCEPTION);
	return consumeOk;
}

AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk
CachingConnectionFactory::CachedChannelHandler::basicConsume(
		const std::string& queue,
		const std::string& consumerTag,
		const bool noAck,
		const bool noLocal,
		const bool exclusive,
		const AmqpClient::SmartPtrConsumer& consumer,
		const AmqpClient::SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME("basicConsume");
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk consumeOk;
	try {
		consumeOk = _channel->basicConsume(
				queue,
				consumerTag,
				noAck,
				noLocal,
				exclusive,
				consumer,
				arguments);
	}
	CAF_CM_CATCH_ALL;
	postProcessCall(CAF_CM_GETEXCEPTION);
	return consumeOk;
}

AmqpClient::AmqpMethods::Basic::SmartPtrCancelOk
CachingConnectionFactory::CachedChannelHandler::basicCancel(
		const std::string& consumerTag) {
	CAF_CM_FUNCNAME("basicCancel");
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	AmqpClient::AmqpMethods::Basic::SmartPtrCancelOk cancelOk;
	try {
		cancelOk = _channel->basicCancel(consumerTag);
	}
	CAF_CM_CATCH_ALL;
	postProcessCall(CAF_CM_GETEXCEPTION);
	return cancelOk;
}

AmqpClient::AmqpMethods::Basic::SmartPtrRecoverOk
CachingConnectionFactory::CachedChannelHandler::basicRecover(
		const bool requeue) {
	CAF_CM_FUNCNAME("basicRecover");
	CAF_CM_LOCK_UNLOCK;
	checkChannel();
	AmqpClient::AmqpMethods::Basic::SmartPtrRecoverOk recoverOk;
	try {
		recoverOk = _channel->basicRecover(requeue);
	}
	CAF_CM_CATCH_ALL;
	postProcessCall(CAF_CM_GETEXCEPTION);
	return recoverOk;
}

AmqpClient::AmqpMethods::Basic::SmartPtrQosOk
CachingConnectionFactory::CachedChannelHandler::basicQos(
		const uint32 prefetchSize,
		const uint32 prefetchCount,
		const bool global) {
	return _channel->basicQos(prefetchSize, prefetchCount, global);
}

void CachingConnectionFactory::CachedChannelHandler::basicReject(
		const uint64 deliveryTag,
		const bool requeue) {
	return _channel->basicReject(deliveryTag, requeue);
}

AmqpClient::AmqpMethods::Exchange::SmartPtrDeclareOk
CachingConnectionFactory::CachedChannelHandler::exchangeDeclare(
	const std::string& exchange,
	const std::string& type,
	const bool durable,
	const AmqpClient::SmartPtrTable& arguments) {
	return _channel->exchangeDeclare(exchange, type, durable, arguments);
}

AmqpClient::AmqpMethods::Exchange::SmartPtrDeleteOk
CachingConnectionFactory::CachedChannelHandler::exchangeDelete(
	const std::string& exchange,
	const bool ifUnused) {
	return _channel->exchangeDelete(exchange, ifUnused);
}

AmqpClient::AmqpMethods::Queue::SmartPtrDeclareOk
CachingConnectionFactory::CachedChannelHandler::queueDeclare() {
	return _channel->queueDeclare();
}

AmqpClient::AmqpMethods::Queue::SmartPtrDeclareOk
CachingConnectionFactory::CachedChannelHandler::queueDeclare(
	const std::string& queue,
	const bool durable,
	const bool exclusive,
	const bool autoDelete,
	const AmqpClient::SmartPtrTable& arguments) {
	return _channel->queueDeclare(
			queue,
			durable,
			exclusive,
			autoDelete,
			arguments);
}

AmqpClient::AmqpMethods::Queue::SmartPtrDeclareOk
CachingConnectionFactory::CachedChannelHandler::queueDeclarePassive(
	const std::string& queue) {
	return _channel->queueDeclarePassive(queue);
}

AmqpClient::AmqpMethods::Queue::SmartPtrDeleteOk
CachingConnectionFactory::CachedChannelHandler::queueDelete(
	const std::string& queue,
	const bool ifUnused,
	const bool ifEmpty) {
	return _channel->queueDelete(queue, ifUnused, ifEmpty);
}

AmqpClient::AmqpMethods::Queue::SmartPtrPurgeOk
CachingConnectionFactory::CachedChannelHandler::queuePurge(
	const std::string& queue) {
	return _channel->queuePurge(queue);
}

AmqpClient::AmqpMethods::Queue::SmartPtrBindOk
CachingConnectionFactory::CachedChannelHandler::queueBind(
	const std::string& queue,
	const std::string& exchange,
	const std::string& routingKey,
	const AmqpClient::SmartPtrTable& arguments) {
	return _channel->queueBind(
			queue,
			exchange,
			routingKey,
			arguments);
}

AmqpClient::AmqpMethods::Queue::SmartPtrUnbindOk
CachingConnectionFactory::CachedChannelHandler::queueUnbind(
	const std::string& queue,
	const std::string& exchange,
	const std::string& routingKey,
	const AmqpClient::SmartPtrTable& arguments) {
	return _channel->queueUnbind(
			queue,
			exchange,
			routingKey,
			arguments);
}

void CachingConnectionFactory::CachedChannelHandler::addReturnListener(
		const AmqpClient::SmartPtrReturnListener& listener) {
	return _channel->addReturnListener(listener);
}

bool CachingConnectionFactory::CachedChannelHandler::removeReturnListener(
		const AmqpClient::SmartPtrReturnListener& listener) {
	return _channel->removeReturnListener(listener);
}
