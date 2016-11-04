/*
 *  Created on: Jun 5, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_RABBITTEMPLATE_H_
#define AMQPINTEGRATIONCORE_RABBITTEMPLATE_H_

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Exception/CCafException.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/Channel.h"
#include "amqpClient/api/Envelope.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpCore/AmqpTemplate.h"
#include "amqpClient/api/Connection.h"
#include "amqpCore/ConnectionFactory.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Implementation of the RabbitTemplate Integration Object
 */
class AMQPINTEGRATIONCORE_LINKAGE RabbitTemplate : public AmqpTemplate {
public:
	RabbitTemplate();
	virtual ~RabbitTemplate();

	void init(SmartPtrConnectionFactory connectionFactory);

	void term();

	void setExchange(const std::string& exchange);

	void setRoutingKey(const std::string& routingKey);

	void setQueue(const std::string& queue);

	void setReplyTimeout(const uint32 replyTimeout);

	void setHeaderMapper(const SmartPtrAmqpHeaderMapper& headerMapper);

public: // AmqpTemplate
	void send(
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	void send(
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	void send(
			const std::string& exchange,
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage receive(
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage receive(
			const std::string& queueName,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage sendAndReceive(
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage sendAndReceive(
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage sendAndReceive(
			const std::string& exchange,
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper());

	gpointer execute(SmartPtrExecutor executor, gpointer data);

private:
	void doSend(
			AmqpClient::SmartPtrChannel channel,
			const std::string& exchange,
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper());

	SmartPtrIIntMessage doSendAndReceive(
			AmqpClient::SmartPtrChannel channel,
			const std::string& exchange,
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper());

private:
	typedef TBlockingCell<SmartPtrIIntMessage> SynchronousHandoff;
	CAF_DECLARE_SMART_POINTER(SynchronousHandoff);

	class DefaultConsumer : public AmqpClient::Consumer {
	public:
		DefaultConsumer();
		virtual ~DefaultConsumer();

		void init(
				SmartPtrAmqpHeaderMapper mapper,
				SmartPtrSynchronousHandoff handoff);

		void handleConsumeOk(
				const std::string& consumerTag);

		void handleCancelOk(
				const std::string& consumerTag);

		void handleRecoverOk(
				const std::string& consumerTag);

		void handleDelivery(
				const std::string& consumerTag,
				const AmqpClient::SmartPtrEnvelope& envelope,
				const AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties& properties,
				const SmartPtrCDynamicByteArray& body);

		void handleShutdown(
				const std::string& consumerTag,
				SmartPtrCCafException& reason);

	private:
		SmartPtrAmqpHeaderMapper _mapper;
		SmartPtrSynchronousHandoff _handoff;
	};
	CAF_DECLARE_SMART_POINTER(DefaultConsumer);

private:
	static std::string DEFAULT_EXCHANGE;
	static std::string DEFAULT_ROUTING_KEY;
	static int32 DEFAULT_REPLY_TIMEOUT;

private:
	bool _isInitialized;
	std::string _exchange;
	std::string _routingKey;
	std::string _queue;
	uint32 _replyTimeout;
	SmartPtrConnectionFactory _connectionFactory;
	SmartPtrConnection _connection;
	SmartPtrAmqpHeaderMapper _headerMapper;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(RabbitTemplate);
};
CAF_DECLARE_SMART_POINTER(RabbitTemplate);

}}

#endif
