/*
 *  Created on: Aug 1, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IIntMessage.h"
#include "Integration/IThrowable.h"
#include "amqpClient/api/Channel.h"
#include "amqpCore/BlockingQueueConsumer.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpCore/DefaultAmqpHeaderMapper.h"
#include "amqpCore/MessageListener.h"
#include "amqpCore/SimpleMessageListenerContainer.h"
#include "Integration/Core/CIntException.h"
#include "Exception/CCafException.h"

using namespace Caf::AmqpIntegration;

SimpleMessageListenerContainer::SimpleMessageListenerContainer() :
	_isInitialized(false),
	_isRunning(false),
	_isActive(false),
	_debugTrace(true),
	_acknowledgeMode(ACKNOWLEDGEMODE_NONE),
	_receiveTimeout(5000),
	_prefetchCount(0),
	_txSize(1),
	_recoveryInterval(30000),
	CAF_CM_INIT_LOG("SimpleMessageListenerContainer") {
}

SimpleMessageListenerContainer::~SimpleMessageListenerContainer() {
}

void SimpleMessageListenerContainer::init() {
	init(_connectionFactory);
}

void SimpleMessageListenerContainer::init(
		SmartPtrConnectionFactory connectionFactory) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(_connectionFactory);
	_connectionFactory = connectionFactory;
	validateConfig();
	_isInitialized = true;

}
void SimpleMessageListenerContainer::setAcknowledgeMode(
		AcknowledgeMode acknowledgeMode) {
	CAF_CM_FUNCNAME_VALIDATE("setAcknowledgeMode");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_acknowledgeMode = acknowledgeMode;
}

void SimpleMessageListenerContainer::setPrefetchCount(
		const uint32 prefetchCount) {
	CAF_CM_FUNCNAME_VALIDATE("setPrefetchCount");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_prefetchCount = prefetchCount;
}

void SimpleMessageListenerContainer::setReceiveTimeout(
		const uint32 receiveTimeout) {
	CAF_CM_FUNCNAME_VALIDATE("setReceiveTimeout");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_NOTZERO(receiveTimeout);
	_receiveTimeout = receiveTimeout;
}

void SimpleMessageListenerContainer::setRecoveryInterval(
		const uint32 recoveryInterval) {
	CAF_CM_FUNCNAME_VALIDATE("setRecoveryInterval");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_NOTZERO(recoveryInterval);
	_recoveryInterval = recoveryInterval;
}

void SimpleMessageListenerContainer::setTxSize(
		const uint32 txSize) {
	CAF_CM_FUNCNAME_VALIDATE("setTxSize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_NOTZERO(txSize);
	_txSize = txSize;
}

void SimpleMessageListenerContainer::setQueue(
		const std::string& queue) {
	CAF_CM_FUNCNAME_VALIDATE("setQueue");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);
	_queue = queue;
}

void SimpleMessageListenerContainer::setConnectionFactory(
		SmartPtrConnectionFactory connectionFactory) {
	CAF_CM_FUNCNAME_VALIDATE("setConnectionFactory");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(connectionFactory);
	_connectionFactory = connectionFactory;
}

void SimpleMessageListenerContainer::setMessagerListener(
		SmartPtrMessageListener messageListener) {
	CAF_CM_FUNCNAME_VALIDATE("setMessageListener");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(messageListener);
	_messageListener = messageListener;
}

SmartPtrMessageListener SimpleMessageListenerContainer::getMessageListener() {
	return _messageListener;
}

void SimpleMessageListenerContainer::start(const uint32 timeout) {
	CAF_CM_FUNCNAME("start");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(!_isRunning);
	_isActive = true;

	CAF_CM_LOG_DEBUG_VA0("Starting Rabbit listener container");

	_startupException.CreateInstance();

	// There is no point in prefetching less than the transaction size because
	// the consumer will stall since the broker will not receive an ack for delivered
	// messages
	const uint32 actualPrefetchCount = _prefetchCount > _txSize ? _prefetchCount : _txSize;
	CAF_CM_LOG_DEBUG_VA3(
			"Config: [prefetchCount=%d][txSize=%d][actualPrefetchCount=%d]",
			_prefetchCount,
			_txSize,
			actualPrefetchCount);

	// At this level simply allow all headers to pass through.
	// The message listener consuming the message will have
	// an opportunity to filter the headers.
	SmartPtrDefaultAmqpHeaderMapper headerMapper;
	headerMapper.CreateInstance();
	headerMapper->init(".*");

	_consumer.CreateInstance();
	_consumer->init(
			_connectionFactory,
			headerMapper,
			_acknowledgeMode,
			actualPrefetchCount,
			_queue);

	SmartPtrAsyncMessageProcessingConsumer processor;
	processor.CreateInstance();
	processor->init(
			this,
			_consumer,
			_startupException,
			timeout,
			_recoveryInterval);

	_executor.CreateInstance();
	_executor->initialize(processor, processor);
	_executor->execute(timeout);

	// Wait for the consumer to start or fail
	const uint64 remainingTime = CDateTimeUtils::calcRemainingTime(
			CDateTimeUtils::getTimeMs(),
			timeout);
	if (remainingTime) {
		SmartPtrCCafException ex = _startupException->get(static_cast<uint32>(remainingTime));
		if (ex) {
			CAF_CM_LOG_CRIT_VA0("Fatal exception on listener startup");
			ex->throwAddRefedSelf();
		}
	} else {
		try {
			_executor->cancel(timeout);
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
		CAF_CM_EXCEPTIONEX_VA0(
				TimeoutException,
				0,
				"The timeout value specified is not int32 enough to determine "
				"if the consumer has started. Increase the timeout value.");
	}

	_startupException = NULL;
	_isRunning = true;
}

void SimpleMessageListenerContainer::stop(const uint32 timeout) {
	CAF_CM_FUNCNAME("stop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_isActive = false;
	if (_isRunning) {
		try {
			_executor->cancel(timeout);
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}
	_isRunning = false;
}

bool SimpleMessageListenerContainer::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _isRunning;
}

void SimpleMessageListenerContainer::validateConfig() {
	CAF_CM_FUNCNAME_VALIDATE("validateConfig");
	CAF_CM_VALIDATE_INTERFACE(_connectionFactory);
	CAF_CM_VALIDATE_INTERFACE(_messageListener);
	CAF_CM_VALIDATE_STRING(_queue);
	CAF_CM_VALIDATE_NOTZERO(_receiveTimeout);
	CAF_CM_VALIDATE_NOTZERO(_recoveryInterval);
	CAF_CM_VALIDATE_NOTZERO(_txSize);
}

bool SimpleMessageListenerContainer::isActive() {
	return _isActive;
}

bool SimpleMessageListenerContainer::receiveAndExecute(
		SmartPtrBlockingQueueConsumer consumer) {
	CAF_CM_FUNCNAME("receiveAndExecute");
	AmqpClient::SmartPtrChannel channel = _consumer->getChannel();
	for (uint32 i = 0; i < _txSize; ++i) {
		if (_debugTrace) {
			CAF_CM_LOG_DEBUG_VA0("Waiting for message from consumer");
		}
		SmartPtrIIntMessage message = _consumer->nextMessage(_receiveTimeout);
		if (!message) {
			break;
		}

		try {
			executeListener(channel, message);
		}
		CAF_CM_CATCH_ALL;
		if (CAF_CM_ISEXCEPTION) {
			SmartPtrCCafException ex = CAF_CM_GETEXCEPTION;
			CAF_CM_CLEAREXCEPTION;
			_consumer->rollbackOnExceptionIfNecessary(ex);
			ex->throwAddRefedSelf();
		}
	}

	return _consumer->commitIfNecessary();
}

void SimpleMessageListenerContainer::executeListener(
		AmqpClient::SmartPtrChannel channel,
		SmartPtrIIntMessage message) {
	doInvokeListener(message);
}

void SimpleMessageListenerContainer::doInvokeListener(
		SmartPtrIIntMessage message) {
	CAF_CM_FUNCNAME("doInvokeListener");
	try {
		_messageListener->onMessage(message);
	} catch (ListenerExecutionFailedException *ex) {
		// noop - let 'er rip
		ex->throwSelf();
	}
	CAF_CM_CATCH_ALL;
	if (CAF_CM_ISEXCEPTION) {
		const std::string orig = CAF_CM_EXCEPTION_GET_FULLMSG;
		CAF_CM_CLEAREXCEPTION;
		CAF_CM_EXCEPTIONEX_VA1(
				ListenerExecutionFailedException,
				0,
				"Listener threw exception: %s",
				orig.c_str());
	}
}

void SimpleMessageListenerContainer::restart() {
	CAF_CM_FUNCNAME_VALIDATE("restart");
	CAF_CM_LOG_DEBUG_VA0("Restarting Rabbit listener container");
	_isRunning = false;
	start(30000);
}

#if (1) // AsyncMessageProcessingConsumer
SimpleMessageListenerContainer::AsyncMessageProcessingConsumer::AsyncMessageProcessingConsumer() :
	_parent(NULL),
	_timeout(0),
	_recoveryInterval(0),
	_isCanceled(false),
	CAF_CM_INIT_LOG("SimpleMessageListenerContainer::AsyncMessageProcessingConsumer") {
}

SimpleMessageListenerContainer::AsyncMessageProcessingConsumer::~AsyncMessageProcessingConsumer() {
}

void SimpleMessageListenerContainer::AsyncMessageProcessingConsumer::init(
		SimpleMessageListenerContainer *parent,
		SmartPtrBlockingQueueConsumer consumer,
		SmartPtrStartupExceptionHandoff startupException,
		const uint32 timeout,
		const uint32 recoveryInterval) {
	_parent = parent;
	_consumer = consumer;
	_startupException = startupException;
	_timeout = timeout;
	_recoveryInterval = recoveryInterval;
}

void SimpleMessageListenerContainer::AsyncMessageProcessingConsumer::run() {
	CAF_CM_FUNCNAME("run");
	bool isAborted = false;

	try {
		try {
			_consumer->start(_timeout);
			_startupException->set(NULL);
			_startupException = NULL;
		} catch (FatalListenerStartupException *ex) {
			ex->throwSelf();
		}
		CAF_CM_CATCH_ALL;
		if (CAF_CM_ISEXCEPTION) {
			_startupException->set(NULL);
			_startupException = NULL;
			CAF_CM_LOG_ERROR_CAFEXCEPTION;
			handleStartupFailure();
			CAF_CM_THROWEXCEPTION;
		}

		bool isContinuable = false;
		while (!_isCanceled && (_parent->isActive() || isContinuable)) {
			try {
				isContinuable = _parent->receiveAndExecute(_consumer);
			} catch (ListenerExecutionFailedException *ex) {
				// ignore
				_logger.log(
						log4cpp::Priority::ERROR,
						_cm_funcName_,
						__LINE__,
						ex);
				ex->Release();
			}
			CAF_CM_CATCH_ALL;
			CAF_CM_LOG_ERROR_CAFEXCEPTION;
			CAF_CM_THROWEXCEPTION;
		}
	} catch (FatalListenerStartupException *ex) {
		CAF_CM_LOG_ERROR_VA1(
				"Consumer received fatal exception on startup: %s",
				ex->getFullMsg().c_str());
		ex->Release();
		isAborted = true;
	}
	CAF_CM_CATCH_ALL;
	if (CAF_CM_ISEXCEPTION) {
		CAF_CM_LOG_WARN_VA1(
				"Consumer raised exception. Processing will restart if the connection "
				"factory supports it. Exception: %s",
				(CAF_CM_EXCEPTION_GET_FULLMSG).c_str());
		CAF_CM_CLEAREXCEPTION;
	}

	if (_startupException) {
		_startupException->set(NULL);
		_startupException = NULL;
	}

	if (_isCanceled) {
		CAF_CM_LOG_DEBUG_VA0("Canceling due to TaskContainer->cancel()");
	} else {
		if (!_parent->isActive() || isAborted) {
			CAF_CM_LOG_DEBUG_VA0("Canceling consumer");
			try {
				_consumer->stop(_timeout);
			}
			CAF_CM_CATCH_ALL;
			CAF_CM_LOG_ERROR_CAFEXCEPTION;
			CAF_CM_CLEAREXCEPTION;

			if (isAborted) {
				CAF_CM_LOG_INFO_VA0("Stopping parent container because of aborted consumer");
				_parent->stop(_timeout);
			}
		} else {
			CAF_CM_LOG_INFO_VA0("Restarting consumer");
			_parent->restart();
		}
	}
}

void SimpleMessageListenerContainer::AsyncMessageProcessingConsumer::cancel() {
	_isCanceled = true;
}

void SimpleMessageListenerContainer::AsyncMessageProcessingConsumer::handleError(
		const SmartPtrIThrowable& throwable,
		const SmartPtrIIntMessage& message) const {
}

void SimpleMessageListenerContainer::AsyncMessageProcessingConsumer::handleStartupFailure() {
	uint64 start = CDateTimeUtils::getTimeMs();
	while (!_isCanceled && CDateTimeUtils::calcRemainingTime(start, _recoveryInterval)) {
		CThreadUtils::sleep(100);
	}
}
#endif
