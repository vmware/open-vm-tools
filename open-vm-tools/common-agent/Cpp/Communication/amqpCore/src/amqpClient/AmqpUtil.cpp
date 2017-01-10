/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "AmqpUtil.h"

using namespace Caf::AmqpClient;

AMQPStatus AmqpUtil::AMQP_BasicAck(
		const SmartPtrCAmqpChannel& channel,
		const uint64 deliveryTag,
		const bool multiple) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_BasicAck");
	CAF_CM_VALIDATE_SMARTPTR(channel);

	return channel->basicAck(deliveryTag, multiple);
}

AMQPStatus AmqpUtil::AMQP_BasicCancel(
		const SmartPtrCAmqpChannel& channel,
		const std::string& consumerTag,
		const bool noWait) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_BasicCancel");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(consumerTag);

	return channel->basicCancel(consumerTag, noWait);
}

AMQPStatus AmqpUtil::AMQP_BasicConsume(
		const SmartPtrCAmqpChannel& channel,
		const std::string& queue,
		const std::string& consumerTag,
		const bool noLocal,
		const bool noAck,
		const bool exclusive,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_BasicConsume");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(queue);

	return channel->basicConsume(queue, consumerTag, noLocal, noAck,
			exclusive, noWait, arguments);
}

AMQPStatus AmqpUtil::AMQP_BasicGet(
		const SmartPtrCAmqpChannel& channel,
		const std::string& queue,
		const bool noAck) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_BasicGet");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(queue);

	return channel->basicGet(queue, noAck);
}

AMQPStatus AmqpUtil::AMQP_BasicPublish(
		const SmartPtrCAmqpChannel& channel,
		const std::string& exchange,
		const std::string& routingKey,
		const bool mandatory,
		const bool immediate,
		const amqp_basic_properties_t *basicProps,
		const SmartPtrCDynamicByteArray& body) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_BasicPublish");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(routingKey);
	CAF_CM_VALIDATE_PTR(basicProps);
	CAF_CM_VALIDATE_SMARTPTR(body);

	return channel->basicPublish(exchange, routingKey, mandatory,
			immediate, basicProps, body);
}

AMQPStatus AmqpUtil::AMQP_BasicRecover(
		const SmartPtrCAmqpChannel& channel,
		const bool requeue) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_BasicRecover");
	CAF_CM_VALIDATE_SMARTPTR(channel);

	return channel->basicRecover(requeue);
}

AMQPStatus AmqpUtil::AMQP_BasicQos(
		const SmartPtrCAmqpChannel& channel,
		const uint32 prefetchSize,
		const uint16 prefetchCount,
		const bool global) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_BasicQos");
	CAF_CM_VALIDATE_SMARTPTR(channel);

	return channel->basicQos(prefetchSize, prefetchCount, global);
}

AMQPStatus AmqpUtil::AMQP_ExchangeDeclare(
		const SmartPtrCAmqpChannel& channel,
		const std::string& exchange,
		const std::string& type,
		const bool passive,
		const bool durable,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_ExchangeDeclare");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(type);

	return channel->exchangeDeclare(exchange, type, passive, durable,
			noWait, arguments);
}

AMQPStatus AmqpUtil::AMQP_ExchangeDelete(
		const SmartPtrCAmqpChannel& channel,
		const std::string& exchange,
		const bool ifUnused,
		const bool noWait) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_ExchangeDelete");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(exchange);

	return channel->exchangeDelete(exchange, ifUnused, noWait);
}

AMQPStatus AmqpUtil::AMQP_QueueBind(
		const SmartPtrCAmqpChannel& channel,
		const std::string& queue,
		const std::string& exchange,
		const std::string& routingKey,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_QueueBind");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(queue);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(routingKey);

	return channel->queueBind(queue, exchange, routingKey, noWait, arguments);
}

AMQPStatus AmqpUtil::AMQP_QueueDeclare(
		const SmartPtrCAmqpChannel& channel,
		const std::string& queue,
		const bool passive,
		const bool durable,
		const bool exclusive,
		const bool autoDelete,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_QueueDeclare");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(queue);

	return channel->queueDeclare(queue, passive, durable, exclusive,
			autoDelete, noWait, arguments);
}

AMQPStatus AmqpUtil::AMQP_QueueDelete(
		const SmartPtrCAmqpChannel& channel,
		const std::string& queue,
		const bool ifUnused,
		const bool ifEmpty,
		const bool noWait) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_QueueDelete");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(queue);

	return channel->queueDelete(queue, ifUnused, ifEmpty, noWait);
}

AMQPStatus AmqpUtil::AMQP_QueuePurge(
		const SmartPtrCAmqpChannel& channel,
		const std::string& queue,
		const bool noWait) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_QueuePurge");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(queue);

	return channel->queuePurge(queue, noWait);
}

AMQPStatus AmqpUtil::AMQP_QueueUnbind(
		const SmartPtrCAmqpChannel& channel,
		const std::string& queue,
		const std::string& exchange,
		const std::string& bindingKey,
		const amqp_table_t *arguments) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpUtil", "AMQP_QueueUnbind");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_STRING(queue);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(bindingKey);

	return channel->queueUnbind(queue, exchange, bindingKey, arguments);
}
