/*
 *  Created on: Jul 30, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_BLOCKINGQUEUECONSUMER_H_
#define AMQPINTEGRATIONCORE_BLOCKINGQUEUECONSUMER_H_

#include "ICafObject.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Common/CAutoRecMutex.h"
#include "Exception/CCafException.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/Channel.h"
#include "amqpClient/api/Envelope.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpClient/api/Connection.h"
#include "amqpCore/ConnectionFactory.h"
#include "Integration/ILifecycle.h"

namespace Caf { namespace AmqpIntegration {

// Forward-declare
class BlockingQueueConsumer;
CAF_DECLARE_SMART_POINTER(BlockingQueueConsumer)

/**
 * @ingroup IntObjImpl
 * @brief Specialized consumer encapsulating knowledge of the broker connections and having its own lifecycle
 */
class AMQPINTEGRATIONCORE_LINKAGE BlockingQueueConsumer :
	public ILifecycle {
public:
	BlockingQueueConsumer();
	virtual ~BlockingQueueConsumer();

	/**
	 * @brief object initializer
	 * @param connectionFactory connection factory object
	 * @param headerMapper header mapper object to map incoming headers
	 * @param acknowledgeMode message acknowledgement mode
	 * @param prefetchCount if <i>acknowledgeMode</i> is <i>AUTO</i> or <i>MANUAL</i> this
	 * message prefetch count value will be sent to the broker via the basic.qos method.
	 * @param queue the queue to consume
	 */
	void init(
			SmartPtrConnectionFactory connectionFactory,
			SmartPtrAmqpHeaderMapper headerMapper,
			AcknowledgeMode acknowledgeMode,
			uint32 prefetchCount,
			const std::string& queue);

	/**
	 * @retval the underlying channel
	 */
	AmqpClient::SmartPtrChannel getChannel();

	/**
	 * @retval the AMQP consumer tag(s)
	 */
	std::string getConsumerTag();

	/**
	 * @brief Wait for the next message delivery and return it
	 * <p>
	 * This is a blocking call and will return only when a message has been delivered
	 * or the consumer is canceled.  In the case of cancellation NULL will be returned.
	 * @retval the next message or NULL
	 */
	SmartPtrIIntMessage nextMessage();

	/**
	 * @brief Wait for the next message delivery and return it
	 * <p>
	 * This is a non-blocking call and will return when a message has been delivered
	 * within the timeout specified or if the consumer is canceled.
	 * @param timeout the timeout in milliseconds
	 * @retval the next message or NULL
	 */
	SmartPtrIIntMessage nextMessage(int32 timeout);

	/**
	 * @brief Acknowledges unacknowledged messages
	 * <p>
	 * Sends a basic.ack for messages that have been delivered.  This method only
	 * sends a basic.ack if the acknowledge mode flag is set to ACKNOWLEDGEMODE_AUTO.
	 */
	bool commitIfNecessary();

	/**
	 * @brief Rejects unacknowledged messaages
	 * <p>
	 * Sends a basic.reject for messages that have been delivered.  This method only
	 * sends a basic.reject if the acknowledge mode flag is set to ACKNOWLEDGEMODE_AUTO.
	 * @param ex the application exception necessitating a rollback
	 */
	void rollbackOnExceptionIfNecessary(SmartPtrCCafException& ex);

public:
	void start(const uint32 timeoutMs);
	void stop(const uint32 timeoutMs);
	bool isRunning() const;

private:
	class InternalConsumer : public AmqpClient::Consumer {
	public:
		InternalConsumer();
		virtual ~InternalConsumer();

		void init(
				BlockingQueueConsumer* parent);

	public:
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

		std::string getConsumerTag();

	private:
		SmartPtrBlockingQueueConsumer _parent;
		AmqpClient::SmartPtrChannel _channel;
		std::string _consumerTag;
		GAsyncQueue *_deliveryQueue;
		CAF_CM_CREATE;
		CAF_CM_CREATE_LOG;
		CAF_CM_DECLARE_NOCOPY(InternalConsumer);
	};
	CAF_DECLARE_SMART_POINTER(InternalConsumer);

private:
	struct Delivery : public ICafObject {
		AmqpClient::SmartPtrEnvelope envelope;
		AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties properties;
		SmartPtrCDynamicByteArray body;
	};
	CAF_DECLARE_SMART_POINTER(Delivery);

private:
	void checkShutdown();

	SmartPtrIIntMessage handle(SmartPtrDelivery delivery);

	static void destroyQueueItem(gpointer data);

private:
	friend class InternalConsumer;

	bool _isInitialized;
	volatile bool _isRunning;
	volatile bool _isCanceled;
	std::set<uint64> _deliveryTags;
	SmartPtrInternalConsumer _consumer;
	SmartPtrCAutoRecMutex _parentLock;
	SmartPtrCCafException _shutdownException;
	GAsyncQueue *_deliveryQueue;
	SmartPtrConnectionFactory _connectionFactory;
	SmartPtrConnection _connection;
	AmqpClient::SmartPtrChannel _channel;
	SmartPtrAmqpHeaderMapper _headerMapper;
	AcknowledgeMode _acknowledgeMode;
	uint32 _prefetchCount;
	std::string _queue;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(BlockingQueueConsumer);
};

}}

#endif /* AMQPINTEGRATIONCORE_BLOCKINGQUEUECONSUMER_H_ */
