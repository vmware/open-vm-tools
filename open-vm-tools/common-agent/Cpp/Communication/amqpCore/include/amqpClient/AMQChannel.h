/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQCHANNEL_H_
#define AMQCHANNEL_H_

#include "amqpClient/TCopyOnWriteContainer.h"
#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Common/CAutoMutex.h"
#include "Exception/CCafException.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/AMQCommand.h"
#include "amqpClient/ConsumerDispatcher.h"
#include "amqpClient/ConsumerWorkService.h"
#include "amqpClient/IConnectionInt.h"
#include "amqpClient/IRpcContinuation.h"
#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/api/AmqpMethods.h"
#include "amqpClient/api/Consumer.h"
#include "amqpClient/api/GetResponse.h"
#include "amqpClient/api/ReturnListener.h"
#include "amqpClient/api/amqpClient.h"
#include "Common/CThreadSignal.h"
#include "amqpClient/api/Channel.h"

namespace Caf { namespace AmqpClient {

class AMQChannel;
CAF_DECLARE_SMART_POINTER(AMQChannel);

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Concrete implementation of a Channel object representing an AMQP channel.
 */
class AMQChannel : public Channel {
public:
	AMQChannel();
	virtual ~AMQChannel();

	/**
	 * @brief Initializer
	 * @param connection the owning connection
	 * @param workService the work service to add channel tasks to
	 */
	void init(
			const SmartPtrIConnectionInt& connection,
			const SmartPtrConsumerWorkService& workService);

	/**
	 * @brief Notification of connection closure
	 * <p>
	 * Called by the AMQChannelManager to notify the channel that the parent connect
	 * has closed for the supplied reason.
	 * @param exception the reason for the closure
	 */
	void notifyConnectionClosed(SmartPtrCCafException& exception);

	/**
	 * @brief Close the channel with the given reason
	 * @param exception reason for closure
	 */
	void close(SmartPtrCCafException& exception);

public: // Channel
	uint16 getChannelNumber();

	void close();

	bool isOpen();

public: // Basic
	void basicAck(
		const uint64 deliveryTag,
		const bool ackMultiple);

	SmartPtrGetResponse basicGet(
		const std::string& queue,
		const bool noAck);

	void basicPublish(
		const std::string& exchange,
		const std::string& routingKey,
		const AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body);

	void basicPublish(
		const std::string& exchange,
		const std::string& routingKey,
		const bool mandatory,
		const bool immediate,
		const AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body);

	AmqpMethods::Basic::SmartPtrConsumeOk basicConsume(
			const std::string& queue,
			const SmartPtrConsumer& consumer);

	AmqpMethods::Basic::SmartPtrConsumeOk basicConsume(
			const std::string& queue,
			const bool noAck,
			const SmartPtrConsumer& consumer);

	AmqpMethods::Basic::SmartPtrConsumeOk basicConsume(
			const std::string& queue,
			const std::string& consumerTag,
			const bool noAck,
			const bool noLocal,
			const bool exclusive,
			const SmartPtrConsumer& consumer,
			const SmartPtrTable& arguments = SmartPtrTable());

	AmqpMethods::Basic::SmartPtrCancelOk basicCancel(
			const std::string& consumerTag);

	AmqpMethods::Basic::SmartPtrRecoverOk basicRecover(
			const bool requeue);

	AmqpMethods::Basic::SmartPtrQosOk basicQos(
			const uint32 prefetchSize,
			const uint32 prefetchCount,
			const bool global);

	void basicReject(
			const uint64 deliveryTag,
			const bool requeue);

public: // Exchange
	AmqpMethods::Exchange::SmartPtrDeclareOk exchangeDeclare(
		const std::string& exchange,
		const std::string& type,
		const bool durable = false,
		const SmartPtrTable& arguments = SmartPtrTable());

	AmqpMethods::Exchange::SmartPtrDeleteOk exchangeDelete(
		const std::string& exchange,
		const bool ifUnused);

public: // Queue
	AmqpMethods::Queue::SmartPtrDeclareOk queueDeclare();

	AmqpMethods::Queue::SmartPtrDeclareOk queueDeclare(
		const std::string& queue,
		const bool durable,
		const bool exclusive,
		const bool autoDelete,
		const SmartPtrTable& arguments = SmartPtrTable());

	AmqpMethods::Queue::SmartPtrDeclareOk queueDeclarePassive(
		const std::string& queue);

	AmqpMethods::Queue::SmartPtrDeleteOk queueDelete(
		const std::string& queue,
		const bool ifUnused,
		const bool ifEmpty);

	AmqpMethods::Queue::SmartPtrPurgeOk queuePurge(
		const std::string& queue);

	AmqpMethods::Queue::SmartPtrBindOk queueBind(
		const std::string& queue,
		const std::string& exchange,
		const std::string& routingKey,
		const SmartPtrTable& arguments = SmartPtrTable());

	AmqpMethods::Queue::SmartPtrUnbindOk queueUnbind(
		const std::string& queue,
		const std::string& exchange,
		const std::string& routingKey,
		const SmartPtrTable& arguments = SmartPtrTable());

	void addReturnListener(
			const SmartPtrReturnListener& listener);

	bool removeReturnListener(
			const SmartPtrReturnListener& listener);

private:
	bool taskHandler();

	void handleCompleteInboundCommand(const SmartPtrAMQCommand& command);

	bool processAsync(const SmartPtrAMQCommand& command);

	void ensureIsOpen();

	SmartPtrAMQCommand execRpc(const SmartPtrIServerMethod& method);

	SmartPtrIRpcContinuation nextOutstandingRpc();

	void transmit(const SmartPtrIServerMethod& method);

	void channelCloseByServerShutdown(const AmqpMethods::Channel::SmartPtrClose& closeMethod);

	void callReturnListeners(const SmartPtrAMQCommand& command);

private:
	/*
	 * This class hooks the channel into the worker service
	 * thread pool for channel inbound AMQP frame processing
	 */
	class ChannelTask : public CManagedThreadPool::IThreadTask {
	public:
		ChannelTask();

		virtual ~ChannelTask();

		/*
		 * Initialize the thread task
		 * channel is the pointer to the parent channel
		 */
		void init(SmartPtrAMQChannel channel);

		/*
		 * Thread pool task execution callback
		 *
		 * Calls through to the channel to do the actual work
		 * returns 'true' if the thread pool should remove this task
		 * returns 'false' if the thread pool should requeue this task
		 */
		bool run();

	private:
		SmartPtrAMQChannel _channel;
		CAF_CM_CREATE;
		CAF_CM_DECLARE_NOCOPY(ChannelTask);
	};
	CAF_DECLARE_SMART_POINTER(ChannelTask);

private:
	static const uint8 DEBUGLOG_FLAG_ENTRYEXIT;
	static const uint8 DEBUGLOG_FLAG_AMQP;

	bool _isInitialized;
	volatile bool _isOpen;
	uint8 _debugLogFlags;
	SmartPtrIConnectionInt _connection;
	SmartPtrConsumerWorkService _workService;
	SmartPtrConsumerDispatcher _dispatcher;
	uint16 _channelNumber;
	SmartPtrCAmqpChannel _channelHandle;
	SmartPtrAMQCommand _command;
	SmartPtrIRpcContinuation _activeRpc;
	SmartPtrCAutoMutex _channelMutex;
	CThreadSignal _channelSignal;

	typedef std::deque<SmartPtrReturnListener> ReturnListenerCollection;
	typedef TCopyOnWriteContainer<ReturnListenerCollection> CowReturnListenerCollection;
	CowReturnListenerCollection _returnListeners;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(AMQChannel);
};

}}

#endif /* AMQCHANNEL_H_ */
