/*
 *  Created on: Oct 7, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2014-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCORE_AMQPUTIL_H_
#define AMQPCORE_AMQPUTIL_H_

#include "amqpClient/CAmqpChannel.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf { namespace AmqpClient {

class AmqpUtil {
public:
	static AMQPStatus AMQP_BasicAck(
			const SmartPtrCAmqpChannel& channel,
			const uint64 deliveryTag,
			const bool multiple);

	static AMQPStatus AMQP_BasicCancel(
			const SmartPtrCAmqpChannel& channel,
			const std::string& consumerTag,
			const bool noWait);

	static AMQPStatus AMQP_BasicConsume(
			const SmartPtrCAmqpChannel& channel,
			const std::string& queue,
			const std::string& consumerTag,
			const bool noLocal,
			const bool noAck,
			const bool exclusive,
			const bool noWait,
			const amqp_table_t *arguments);

	static AMQPStatus AMQP_BasicGet(
			const SmartPtrCAmqpChannel& channel,
			const std::string& queue,
			const bool noAck);

	static AMQPStatus AMQP_BasicPublish(
			const SmartPtrCAmqpChannel& channel,
			const std::string& exchange,
			const std::string& routingKey,
			const bool mandatory,
			const bool immediate,
			const amqp_basic_properties_t *basicProps,
			const SmartPtrCDynamicByteArray& body);

	static AMQPStatus AMQP_BasicRecover(
			const SmartPtrCAmqpChannel& channel,
			const bool requeue);

	static AMQPStatus AMQP_BasicQos(
			const SmartPtrCAmqpChannel& channel,
			const uint32 prefetchSize,
			const uint16 prefetchCount,
			bool global);

	static AMQPStatus AMQP_ExchangeDeclare(
			const SmartPtrCAmqpChannel& channel,
			const std::string& exchange,
			const std::string& type,
			const bool passive,
			const bool durable,
			const bool noWait,
			const amqp_table_t *arguments);

	static AMQPStatus AMQP_ExchangeDelete(
			const SmartPtrCAmqpChannel& channel,
			const std::string& exchange,
			const bool ifUnused,
			const bool noWait);

	static AMQPStatus AMQP_QueueBind(
			const SmartPtrCAmqpChannel& channel,
			const std::string& queue,
			const std::string& exchange,
			const std::string& routingKey,
			const bool noWait,
			const amqp_table_t *arguments);

	static AMQPStatus AMQP_QueueDeclare(
			const SmartPtrCAmqpChannel& channel,
			const std::string& queue,
			const bool passive,
			const bool durable,
			const bool exclusive,
			const bool autoDelete,
			const bool noWait,
			const amqp_table_t *arguments);

	static AMQPStatus AMQP_QueueDelete(
			const SmartPtrCAmqpChannel& channel,
			const std::string& queue,
			const bool ifUnused,
			const bool ifEmpty,
			const bool noWait);

	static AMQPStatus AMQP_QueuePurge(
			const SmartPtrCAmqpChannel& channel,
			const std::string& queue,
			const bool noWait);

	static AMQPStatus AMQP_QueueUnbind(
			const SmartPtrCAmqpChannel& channel,
			const std::string& queue,
			const std::string& exchange,
			const std::string& bindingKey,
			const amqp_table_t *arguments);

private:
  CAF_CM_DECLARE_NOCREATE (AmqpUtil);
};

}}

#endif /* AMQPCORE_AMQPUTIL_H_ */
