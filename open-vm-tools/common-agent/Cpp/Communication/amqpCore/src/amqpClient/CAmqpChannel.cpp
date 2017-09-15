/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/CAmqpConnection.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/CAmqpChannel.h"

using namespace Caf::AmqpClient;

CAmqpChannel::CAmqpChannel() :
	_isInitialized(false),
	_channel(0),
	CAF_CM_INIT("CAmqpChannel") {
	CAF_CM_INIT_THREADSAFE;
}

CAmqpChannel::~CAmqpChannel() {
}

void CAmqpChannel::initialize(
		const SmartPtrCAmqpConnection& connection,
		const amqp_channel_t& channel) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(connection);

	_connection = connection;
	_channel = channel;
	_isInitialized = true;
}

AMQPStatus CAmqpChannel::close() {
	AMQPStatus rc = AMQP_ERROR_OK;
	if (_isInitialized) {
		rc = _connection->channelClose(_channel);

		_connection = NULL;
		_channel = 0;
		_isInitialized = false;
	}

	return rc;
}

AMQPStatus CAmqpChannel::closeOk() {
	CAF_CM_FUNCNAME_VALIDATE("closeOk");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _connection->channelCloseOk(_channel);
}

AMQPStatus CAmqpChannel::receive(
		SmartPtrCAmqpFrame& frame,
		int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("receive");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _connection->receive(_channel, frame, timeout);
}

AMQPStatus CAmqpChannel::getId(
		uint16 *id) {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_PTR(id);

	*id = _channel;

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpChannel::basicAck(
		const uint64 deliveryTag,
		const bool multiple) {
	CAF_CM_FUNCNAME_VALIDATE("basicAck");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _connection->basicAck(_channel, deliveryTag, multiple);
}

AMQPStatus CAmqpChannel::basicCancel(
		const std::string& consumerTag,
		const bool noWait) {
	CAF_CM_FUNCNAME_VALIDATE("basicCancel");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(consumerTag);

	return _connection->basicCancel(_channel, consumerTag, noWait);
}

AMQPStatus CAmqpChannel::basicConsume(
		const std::string& queue,
		const std::string& consumerTag,
		const bool noLocal,
		const bool noAck,
		const bool exclusive,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("basicConsume");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);

	return _connection->basicConsume(_channel, queue, consumerTag,
			noLocal, noAck, exclusive, noWait, arguments);
}

AMQPStatus CAmqpChannel::basicGet(
		const std::string& queue,
		const bool noAck) {
	CAF_CM_FUNCNAME_VALIDATE("basicGet");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);

	return _connection->basicGet(_channel, queue, noAck);
}

AMQPStatus CAmqpChannel::basicPublish(
		const std::string& exchange,
		const std::string& routingKey,
		const bool mandatory,
		const bool immediate,
		const amqp_basic_properties_t *basicProps,
		const SmartPtrCDynamicByteArray& body) {
	CAF_CM_FUNCNAME_VALIDATE("basicPublish");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(routingKey);
	CAF_CM_VALIDATE_PTR(basicProps);
	CAF_CM_VALIDATE_SMARTPTR(body);

	return _connection->basicPublish(_channel, exchange, routingKey,
			mandatory, immediate, basicProps, body);
}

AMQPStatus CAmqpChannel::basicRecover(
		const bool requeue) {
	CAF_CM_FUNCNAME_VALIDATE("basicRecover");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _connection->basicRecover(_channel, requeue);
}

AMQPStatus CAmqpChannel::basicQos(
		const uint32 prefetchSize,
		const uint16 prefetchCount,
		const bool global) {
	CAF_CM_FUNCNAME_VALIDATE("basicQos");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _connection->basicQos(_channel, prefetchSize, prefetchCount, global);
}

AMQPStatus CAmqpChannel::exchangeDeclare(
		const std::string& exchange,
		const std::string& type,
		const bool passive,
		const bool durable,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("exchangeDeclare");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(type);

	return _connection->exchangeDeclare(_channel, exchange, type, passive,
			durable, noWait, arguments);
}

AMQPStatus CAmqpChannel::exchangeDelete(
		const std::string& exchange,
		const bool ifUnused,
		const bool noWait) {
	CAF_CM_FUNCNAME_VALIDATE("exchangeDelete");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(exchange);

	return _connection->exchangeDelete(_channel, exchange, ifUnused, noWait);
}

AMQPStatus CAmqpChannel::queueBind(
		const std::string& queue,
		const std::string& exchange,
		const std::string& routingKey,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("queueBind");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(routingKey);

	return _connection->queueBind(_channel, queue, exchange, routingKey,
			noWait, arguments);
}

AMQPStatus CAmqpChannel::queueDeclare(
		const std::string& queue,
		const bool passive,
		const bool durable,
		const bool exclusive,
		const bool autoDelete,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("queueDeclare");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);

	return _connection->queueDeclare(_channel, queue, passive, durable,
			exclusive, autoDelete, noWait, arguments);
}

AMQPStatus CAmqpChannel::queueDelete(
		const std::string& queue,
		const bool ifUnused,
		const bool ifEmpty,
		const bool noWait) {
	CAF_CM_FUNCNAME_VALIDATE("queueDelete");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);

	return _connection->queueDelete(_channel, queue, ifUnused, ifEmpty, noWait);
}

AMQPStatus CAmqpChannel::queuePurge(
		const std::string& queue,
		const bool noWait) {
	CAF_CM_FUNCNAME_VALIDATE("queuePurge");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);

	return _connection->queuePurge(_channel, queue, noWait);
}

AMQPStatus CAmqpChannel::queueUnbind(
		const std::string& queue,
		const std::string& exchange,
		const std::string& bindingKey,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("queueUnbind");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(bindingKey);

	return _connection->queueUnbind(_channel, queue, exchange, bindingKey, arguments);
}
