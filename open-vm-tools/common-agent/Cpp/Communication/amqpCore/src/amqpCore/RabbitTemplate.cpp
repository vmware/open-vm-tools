/*
 *  Created on: Jun 5, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/AmqpMethods.h"
#include "amqpClient/api/Channel.h"
#include "amqpClient/api/Envelope.h"
#include "amqpClient/api/GetResponse.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpCore/AmqpTemplate.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpCore/DefaultAmqpHeaderMapper.h"
#include "amqpCore/RabbitTemplate.h"
#include "Exception/CCafException.h"
#include "AutoChannelClose.h"

using namespace Caf::AmqpIntegration;

std::string RabbitTemplate::DEFAULT_EXCHANGE = "";
std::string RabbitTemplate::DEFAULT_ROUTING_KEY = "";
int32 RabbitTemplate::DEFAULT_REPLY_TIMEOUT = 5000;

RabbitTemplate::RabbitTemplate() :
	_isInitialized(false),
	_exchange(DEFAULT_EXCHANGE),
	_routingKey(DEFAULT_ROUTING_KEY),
	_replyTimeout(DEFAULT_REPLY_TIMEOUT),
	CAF_CM_INIT_LOG("RabbitTemplate") {
}

RabbitTemplate::~RabbitTemplate() {
	CAF_CM_FUNCNAME("~RabbitTemplate");
	try {
		term();
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_CLEAREXCEPTION;
}

void RabbitTemplate::init(SmartPtrConnectionFactory connectionFactory) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(connectionFactory);
	SmartPtrDefaultAmqpHeaderMapper defaultMapper;
	defaultMapper.CreateInstance();
	defaultMapper->init();
	_headerMapper = defaultMapper;
	_connectionFactory = connectionFactory;
	_connection = _connectionFactory->createConnection();
	_isInitialized = true;
}

void RabbitTemplate::term() {
	if (_connection) {
		_connection->close();
		_connection = NULL;
	}
}

void RabbitTemplate::setExchange(const std::string& exchange) {
	_exchange = exchange;
}

void RabbitTemplate::setRoutingKey(const std::string& routingKey) {
	_routingKey = routingKey;
}

void RabbitTemplate::setQueue(const std::string& queue) {
	_queue = queue;
}

void RabbitTemplate::setReplyTimeout(const uint32 replyTimeout) {
	_replyTimeout = replyTimeout;
}

void RabbitTemplate::setHeaderMapper(const SmartPtrAmqpHeaderMapper& headerMapper) {
	CAF_CM_FUNCNAME_VALIDATE("setHeaderMapper");
	CAF_CM_VALIDATE_SMARTPTR(headerMapper);
	_headerMapper = headerMapper;
}

void RabbitTemplate::send(
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper headerMapper) {
	send(
			_exchange,
			_routingKey,
			message,
			headerMapper);
}

void RabbitTemplate::send(
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper headerMapper) {
	send(
			_exchange,
			routingKey,
			message,
			headerMapper);
}

void RabbitTemplate::send(
		const std::string& exchange,
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper headerMapper) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	AmqpClient::SmartPtrChannel channel = _connection->createChannel();
	AutoChannelClose closer(channel);
	doSend(channel, exchange, routingKey, message, headerMapper);
}

SmartPtrIIntMessage RabbitTemplate::receive(SmartPtrAmqpHeaderMapper headerMapper) {
	return receive(_queue, headerMapper);
}

SmartPtrIIntMessage RabbitTemplate::receive(
		const std::string& queueName,
		SmartPtrAmqpHeaderMapper headerMapper) {
	CAF_CM_FUNCNAME_VALIDATE("receive");
	CAF_CM_VALIDATE_STRING(queueName);

	SmartPtrCIntMessage message;
	AmqpClient::SmartPtrChannel channel = _connection->createChannel();
	AutoChannelClose closer(channel);
	AmqpClient::SmartPtrGetResponse response = channel->basicGet(queueName, true);
	if (response) {
		if (!headerMapper) {
			headerMapper = _headerMapper;
		}
		IIntMessage::SmartPtrCHeaders headers =
				headerMapper->toHeaders(
						response->getProperties(),
						response->getEnvelope());
		message.CreateInstance();
		message->initialize(response->getBody(), headers,
			IIntMessage::SmartPtrCHeaders());
	}
	return message;
}

SmartPtrIIntMessage RabbitTemplate::sendAndReceive(
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper requestHeaderMapper,
		SmartPtrAmqpHeaderMapper responseHeaderMapper) {
	return sendAndReceive(
			_exchange,
			_routingKey,
			message,
			requestHeaderMapper,
			responseHeaderMapper);
}

SmartPtrIIntMessage RabbitTemplate::sendAndReceive(
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper requestHeaderMapper,
		SmartPtrAmqpHeaderMapper responseHeaderMapper) {
	return sendAndReceive(
			_exchange,
			routingKey,
			message,
			requestHeaderMapper,
			responseHeaderMapper);
}

SmartPtrIIntMessage RabbitTemplate::sendAndReceive(
		const std::string& exchange,
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper requestHeaderMapper,
		SmartPtrAmqpHeaderMapper responseHeaderMapper) {
	AmqpClient::SmartPtrChannel channel = _connection->createChannel();
	AutoChannelClose closer(channel);
	return doSendAndReceive(
			channel,
			exchange,
			routingKey,
			message,
			requestHeaderMapper,
			responseHeaderMapper);
}

gpointer RabbitTemplate::execute(SmartPtrExecutor executor, gpointer data) {
	CAF_CM_FUNCNAME_VALIDATE("execute");
	CAF_CM_VALIDATE_INTERFACE(executor);
	AmqpClient::SmartPtrChannel channel = _connection->createChannel();
	CAF_CM_VALIDATE_SMARTPTR(channel);
	AutoChannelClose closer(channel);
	return executor->execute(channel, data);
}

void RabbitTemplate::doSend(
		AmqpClient::SmartPtrChannel channel,
		const std::string& exchange,
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper headerMapper) {
	CAF_CM_FUNCNAME_VALIDATE("doSend");
	CAF_CM_LOG_DEBUG_VA2(
			"Publishing message on exchange [%s], routingKey= [%s]",
			exchange.c_str(),
			routingKey.c_str());

	if (!headerMapper) {
		headerMapper = _headerMapper;
	}

	AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties props = headerMapper->fromHeaders(message->getHeaders());
	channel->basicPublish(
			exchange,
			routingKey,
			false,
			false,
			props,
			message->getPayload());
}

SmartPtrIIntMessage RabbitTemplate::doSendAndReceive(
		AmqpClient::SmartPtrChannel channel,
		const std::string& exchange,
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper requestHeaderMapper,
		SmartPtrAmqpHeaderMapper responseHeaderMapper) {
	CAF_CM_FUNCNAME("doSendAndReceive");

	if (!requestHeaderMapper) {
		requestHeaderMapper = _headerMapper;
	}
	if (!responseHeaderMapper) {
		responseHeaderMapper = _headerMapper;
	}

	IIntMessage::SmartPtrCHeaders headers = message->getHeaders();
	if (headers->find(AmqpHeaderMapper::REPLY_TO) != headers->end()) {
		CAF_CM_EXCEPTIONEX_VA1(
				IllegalStateException,
				0,
				"Send-and-receive methods can only be used if the message "
				"does not already have a %s property",
				AmqpHeaderMapper::REPLY_TO.c_str());
	}

	// Declare a temporary queue and set the replyTo
	AmqpClient::AmqpMethods::Queue::SmartPtrDeclareOk queueDeclareOk =
			channel->queueDeclare();
	headers->insert(
			std::make_pair(
					AmqpHeaderMapper::REPLY_TO,
					std::make_pair(
						CVariant::createString(queueDeclareOk->getQueueName()), SmartPtrICafObject())));

	// Create an inter-thread RPC mechanism to capture the response
	SmartPtrSynchronousHandoff handoff;
	handoff.CreateInstance();

	// Spin up a consumer to wait for the response
	SmartPtrDefaultConsumer consumer;
	consumer.CreateInstance();
	consumer->init(responseHeaderMapper, handoff);
	const std::string consumerTag = CStringUtils::createRandomUuid();
	const bool noAck = false;
	const bool noLocal = true;
	const bool exclusive = true;
	AmqpClient::AmqpMethods::Basic::SmartPtrConsumeOk consumeOk =
			channel->basicConsume(
					queueDeclareOk->getQueueName(),
					consumerTag,
					noAck,
					noLocal,
					exclusive,
					consumer);

	// Send the message
	doSend(channel, exchange, routingKey, message, requestHeaderMapper);

	// Wait for the reply
	SmartPtrIIntMessage reply = handoff->get(_replyTimeout);

	// Cancel the consumer
	channel->basicCancel(consumerTag);

	return reply;
}

RabbitTemplate::DefaultConsumer::DefaultConsumer() {
}

RabbitTemplate::DefaultConsumer::~DefaultConsumer() {
}

void RabbitTemplate::DefaultConsumer::init(
		SmartPtrAmqpHeaderMapper mapper,
		SmartPtrSynchronousHandoff handoff) {
	_mapper = mapper;
	_handoff = handoff;
}

void RabbitTemplate::DefaultConsumer::handleDelivery(
		const std::string& consumerTag,
		const AmqpClient::SmartPtrEnvelope& envelope,
		const AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body) {
	const IIntMessage::SmartPtrCHeaders headers =
			_mapper->toHeaders(properties, envelope);
	SmartPtrCIntMessage message;
	message.CreateInstance();
	message->initialize(body, headers, IIntMessage::SmartPtrCHeaders());
	_handoff->set(message);
}

void RabbitTemplate::DefaultConsumer::handleConsumeOk(
		const std::string& consumerTag) {
}

void RabbitTemplate::DefaultConsumer::handleCancelOk(
		const std::string& consumerTag) {
}

void RabbitTemplate::DefaultConsumer::handleRecoverOk(
		const std::string& consumerTag) {
}

void RabbitTemplate::DefaultConsumer::handleShutdown(
		const std::string& consumerTag,
		SmartPtrCCafException& reason) {
}
