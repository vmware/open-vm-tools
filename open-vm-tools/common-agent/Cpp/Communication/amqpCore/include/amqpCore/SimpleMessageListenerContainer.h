/*
 *  Created on: Aug 1, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef SIMPLEMESSAGELISTENERCONTAINER_H_
#define SIMPLEMESSAGELISTENERCONTAINER_H_



#include "Integration/IErrorHandler.h"

#include "Exception/CCafException.h"
#include "Integration/Core/CSimpleAsyncTaskExecutor.h"
#include "Integration/IIntMessage.h"
#include "Integration/IThrowable.h"
#include "amqpClient/api/Channel.h"
#include "amqpCore/BlockingQueueConsumer.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpCore/MessageListener.h"
#include "Integration/ILifecycle.h"
#include "Integration/IRunnable.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief A simple message listener used by channels to retrieve messages
 * <p>
 * This container manages message acknowledgment, broker connection recoverability and
 * other aspects of consuming from queues.
 */
class AMQPINTEGRATIONCORE_LINKAGE SimpleMessageListenerContainer :
	public ILifecycle {
public:
	SimpleMessageListenerContainer();
	virtual ~SimpleMessageListenerContainer();

	/**
	 * @brief initialize the object
	 */
	void init();

	/**
	 * @brief initialize the object
	 * @param connectionFactory the ConnectionFactory
	 */
	void init(
			SmartPtrConnectionFactory connectionFactory);

public:
	/**
	 * @brief Set the message acknowledgment mode
	 * @param acknowledgeMode acknowledgment mode
	 */
	void setAcknowledgeMode(AcknowledgeMode acknowledgeMode);

	void setPrefetchCount(const uint32 prefetchCount);

	void setReceiveTimeout(const uint32 receiveTimeout);

	void setRecoveryInterval(const uint32 recoveryInterval);

	void setTxSize(const uint32 txSize);

	void setQueue(const std::string& queue);

	void setConnectionFactory(SmartPtrConnectionFactory connectionFactory);

	void setMessagerListener(SmartPtrMessageListener messageListener);

	SmartPtrMessageListener getMessageListener();

public: // ILifecycle
	void start(const uint32 timeoutMs);

	void stop(const uint32 timeoutMs);

	bool isRunning() const;

private:
	void validateConfig();

	bool isActive();

	bool receiveAndExecute(SmartPtrBlockingQueueConsumer consumer);

	void executeListener(
			AmqpClient::SmartPtrChannel channel,
			SmartPtrIIntMessage message);

	void doInvokeListener(
			SmartPtrIIntMessage message);

	void restart();

private:
	typedef TBlockingCell<SmartPtrCCafException> StartupExceptionHandoff;
	CAF_DECLARE_SMART_POINTER(StartupExceptionHandoff);

private:
	class AsyncMessageProcessingConsumer :
		public IRunnable,
		public IErrorHandler {
	public:
		AsyncMessageProcessingConsumer();
		~AsyncMessageProcessingConsumer();
		void init(
				SimpleMessageListenerContainer *parent,
				SmartPtrBlockingQueueConsumer consumer,
				SmartPtrStartupExceptionHandoff startupException,
				const uint32 timeout,
				const uint32 recoveryInterval);
		void run();
		void cancel();
		void handleError(
				const SmartPtrIThrowable& throwable,
				const SmartPtrIIntMessage& message) const;

	private:
		void handleStartupFailure();

	private:
		SimpleMessageListenerContainer *_parent;
		SmartPtrBlockingQueueConsumer _consumer;
		SmartPtrStartupExceptionHandoff _startupException;
		uint32 _timeout;
		uint32 _recoveryInterval;
		bool _isCanceled;

		CAF_CM_CREATE;
		CAF_CM_CREATE_LOG;
		CAF_CM_DECLARE_NOCOPY(AsyncMessageProcessingConsumer);
	};
	CAF_DECLARE_SMART_POINTER(AsyncMessageProcessingConsumer);

private:
	bool _isInitialized;
	volatile bool _isRunning;
	volatile bool _isActive;
	bool _debugTrace;
	SmartPtrBlockingQueueConsumer _consumer;
	SmartPtrCSimpleAsyncTaskExecutor _executor;
	SmartPtrStartupExceptionHandoff _startupException;

	SmartPtrConnectionFactory _connectionFactory;
	SmartPtrMessageListener _messageListener;
	std::string _queue;
	AcknowledgeMode _acknowledgeMode;
	uint32 _receiveTimeout;
	uint32 _prefetchCount;
	uint32 _txSize;
	uint32 _recoveryInterval;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(SimpleMessageListenerContainer);
};
CAF_DECLARE_SMART_POINTER(SimpleMessageListenerContainer);

}}

#endif /* SIMPLEMESSAGELISTENERCONTAINER_H_ */
