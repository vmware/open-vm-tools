/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CONSUMERDISPATCHER_H_
#define CONSUMERDISPATCHER_H_



#include "ICafObject.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Exception/CCafException.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/ConsumerWorkService.h"
#include "amqpClient/api/Consumer.h"
#include "amqpClient/api/Envelope.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Dispatches notifications to a {@link Consumer} on an internally-managed work pool.
 * <p>
 * Each {@link Channel} has a single {@link ConsumerDispatcher}, but the work pool may be
 * shared with other channels, typically those on the name {@link AMQConnection}.
 */
class ConsumerDispatcher {
public:
	ConsumerDispatcher();
	virtual ~ConsumerDispatcher();

	/**
	 * @brief Initialize the object
	 * @param workService work service providing a work pool for dispatching notifications
	 */
	void init(
			const SmartPtrConsumerWorkService& workService);

	/**
	 * @brief Prepare for shutdown of all consumers on this channel
	 */
	void quiesce();

	/**
	 * @brief Lock the dispatcher
	 * <p>
	 * Place a lock on the dispatcher.  All threads attemping to call the dispatcher will
	 * be blocked until unlock is called.
	 */
	void lock();

	/**
	 * @brief unlock the dispatcher
	 * <p>
	 * Remove the lock on the dispatcher.
	 */
	void unlock();

	/**
	 * @brief Adds a consumer to the dispatcher
	 * @param consumerTag consumer tag
	 * @param consumer consumer object
	 */
	void addConsumer(
			const std::string& consumerTag,
			const SmartPtrConsumer& consumer);

	/**
	 * @brief Removes a consumer from the dispatcher
	 * @param consumerTag consumer tag
	 */
	void removeConsumer(
			const std::string& consumerTag);

	/**
	 * @brief Retrieves a consumer from the dispatcher
	 * @param consumerTag consumer tag
	 * @return the consumer or <i>null</i> if not found
	 */
	SmartPtrConsumer getConsumer(
			const std::string& consumerTag);

	/**
	 * @brief Handle basic.consume-ok
	 * @param consumerTag consumer tag
	 */
	void handleConsumeOk(
			const std::string& consumerTag);

	/**
	 * @brief Handle basic.cancel-ok
	 * @param consumerTag consumer tag
	 */
	void handleCancelOk(
			const std::string& consumerTag);

	/**
	 * @brief Handle basic.recover-ok
	 */
	void handleRecoverOk();

	/**
	 * @brief Handle basic.delivery
	 * @param consumerTag consumer tag
	 * @param envelope message envelope
	 * @param properties message properties and headers
	 * @param body message body
	 */
	void handleDelivery(
			const std::string& consumerTag,
			const SmartPtrEnvelope& envelope,
			const AmqpContentHeaders::SmartPtrBasicProperties& properties,
			const SmartPtrCDynamicByteArray& body);

	/**
	 * @brief Handle a channel shutdown event
	 * @param exception reason for the shutdown
	 */
	void handleShutdown(SmartPtrCCafException exception);

private: // Task support
	typedef enum {
		DISPATCH_ITEM_METHOD_HANDLE_CONSUME_OK,
		DISPATCH_ITEM_METHOD_HANDLE_CANCEL_OK,
		DISPATCH_ITEM_METHOD_HANDLE_RECOVER_OK,
		DISPATCH_ITEM_METHOD_HANDLE_DELIVERY,
		DISPATCH_ITEM_METHOD_TERMINATE
	} DispatchItemMethod;

	class DispatcherWorkItem : public ICafObject {
	public:
		DispatcherWorkItem();

		void init(
				const DispatchItemMethod method);

		void init(
				const DispatchItemMethod method,
				const SmartPtrEnvelope& envelope,
				const AmqpContentHeaders::SmartPtrBasicProperties& properties,
				const SmartPtrCDynamicByteArray& body);

		DispatchItemMethod getMethod() const;
		SmartPtrEnvelope getEnvelope() const;
		AmqpContentHeaders::SmartPtrBasicProperties getProperties() const;
		SmartPtrCDynamicByteArray getBody() const;

	private:
		DispatchItemMethod _method;
		SmartPtrEnvelope _envelope;
		AmqpContentHeaders::SmartPtrBasicProperties _properties;
		SmartPtrCDynamicByteArray _body;
	};
	CAF_DECLARE_SMART_POINTER(DispatcherWorkItem);

	class DispatcherTask : public CManagedThreadPool::IThreadTask {
	public:
		DispatcherTask();
		~DispatcherTask();

		void init(
				const std::string& consumerTag,
				const SmartPtrConsumer& consumer);

		void term();

		void addWorkItem(const SmartPtrDispatcherWorkItem& workItem);

		bool run();

	private:
		static void FreeWorkItem(gpointer data);

	private:
		std::string _consumerTag;
		SmartPtrConsumer _consumer;
		GAsyncQueue *_workItemQueue;
	};
	CAF_DECLARE_SMART_POINTER(DispatcherTask);

private:
	typedef std::pair<SmartPtrConsumer, SmartPtrDispatcherTask> ConsumerItem;
	typedef std::map<std::string, ConsumerItem> ConsumerMap;

	ConsumerItem getConsumerItem(const std::string& consumerTag);

private:
	bool _isInitialized;
	volatile bool _isShuttingDown;
	SmartPtrConsumerWorkService _workService;
	ConsumerMap _consumers;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(ConsumerDispatcher);
};
CAF_DECLARE_SMART_POINTER(ConsumerDispatcher);

}}

#endif /* CONSUMERDISPATCHER_H_ */
