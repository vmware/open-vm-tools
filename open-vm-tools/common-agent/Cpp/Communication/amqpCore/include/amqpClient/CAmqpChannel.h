/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENT_CAMQPCHANNEL_H_
#define AMQPCLIENT_CAMQPCHANNEL_H_


#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/CAmqpConnection.h"
#include "amqpClient/CAmqpFrame.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Manages a set of channels for a connection.
 * Channels are indexed by channel number (<code><b>1.._channelMax</b></code>).
 */
class CAmqpChannel {
public:
	CAmqpChannel();
	virtual ~CAmqpChannel();

	void initialize(
			const SmartPtrCAmqpConnection& connection,
			const amqp_channel_t& channel);

public:
	AMQPStatus close();

	AMQPStatus closeOk();

	AMQPStatus receive(
			SmartPtrCAmqpFrame& frame,
			int32 timeout);

	AMQPStatus getId(
			uint16 *id);

	AMQPStatus basicAck(
			const uint64 deliveryTag,
			const bool multiple);

	AMQPStatus basicCancel(
			const std::string& consumerTag,
			const bool noWait);

	AMQPStatus basicConsume(
			const std::string& queue,
			const std::string& consumerTag,
			const bool noLocal,
			const bool noAck,
			const bool exclusive,
			const bool noWait,
			const amqp_table_t *arguments);

	AMQPStatus basicGet(
			const std::string& queue,
			const bool noAck);

	AMQPStatus basicPublish(
			const std::string& exchange,
			const std::string& routingKey,
			const bool mandatory,
			const bool immediate,
			const amqp_basic_properties_t *basicProps,
			const SmartPtrCDynamicByteArray& body);

	AMQPStatus basicRecover(
			const bool requeue);

	AMQPStatus basicQos(
			const uint32 prefetchSize,
			const uint16 prefetchCount,
			bool global);

	AMQPStatus exchangeDeclare(
			const std::string& exchange,
			const std::string& type,
			const bool passive,
			const bool durable,
			const bool noWait,
			const amqp_table_t *arguments);

	AMQPStatus exchangeDelete(
			const std::string& exchange,
			const bool ifUnused,
			const bool noWait);

	AMQPStatus queueBind(
			const std::string& queue,
			const std::string& exchange,
			const std::string& routingKey,
			const bool noWait,
			const amqp_table_t *arguments);

	AMQPStatus queueDeclare(
			const std::string& queue,
			const bool passive,
			const bool durable,
			const bool exclusive,
			const bool autoDelete,
			const bool noWait,
			const amqp_table_t *arguments);

	AMQPStatus queueDelete(
			const std::string& queue,
			const bool ifUnused,
			const bool ifEmpty,
			const bool noWait);

	AMQPStatus queuePurge(
			const std::string& queue,
			const bool noWait);

	AMQPStatus queueUnbind(
			const std::string& queue,
			const std::string& exchange,
			const std::string& bindingKey,
			const amqp_table_t *arguments);

private:
	bool _isInitialized;
	SmartPtrCAmqpConnection _connection;
	amqp_channel_t _channel;

	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CAmqpChannel);
};

}}

#endif /* AMQPCLIENT_CAMQPCHANNEL_H_ */
