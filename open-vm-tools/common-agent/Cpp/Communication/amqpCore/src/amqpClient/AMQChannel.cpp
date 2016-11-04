/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/AMQCommand.h"
#include "amqpClient/BlockingRpcContinuation.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/ConsumerWorkService.h"
#include "amqpClient/IConnectionInt.h"
#include "amqpClient/IRpcContinuation.h"
#include "amqpClient/TCopyOnWriteContainer.h"
#include "amqpClient/amqpImpl/BasicAckMethod.h"
#include "amqpClient/amqpImpl/BasicCancelMethod.h"
#include "amqpClient/amqpImpl/BasicConsumeMethod.h"
#include "amqpClient/amqpImpl/BasicGetMethod.h"
#include "amqpClient/amqpImpl/BasicPublishMethod.h"
#include "amqpClient/amqpImpl/BasicQosMethod.h"
#include "amqpClient/amqpImpl/BasicRecoverMethod.h"
#include "amqpClient/amqpImpl/BasicRejectMethod.h"
#include "amqpClient/amqpImpl/ChannelCloseOkMethod.h"
#include "amqpClient/amqpImpl/EnvelopeImpl.h"
#include "amqpClient/amqpImpl/ExchangeDeclareMethod.h"
#include "amqpClient/amqpImpl/ExchangeDeleteMethod.h"
#include "amqpClient/amqpImpl/GetResponseImpl.h"
#include "amqpClient/amqpImpl/IContentHeader.h"
#include "amqpClient/amqpImpl/IMethod.h"
#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/amqpImpl/QueueBindMethod.h"
#include "amqpClient/amqpImpl/QueueDeclareMethod.h"
#include "amqpClient/amqpImpl/QueueDeleteMethod.h"
#include "amqpClient/amqpImpl/QueuePurgeMethod.h"
#include "amqpClient/amqpImpl/QueueUnbindMethod.h"
#include "amqpClient/api/AmqpMethods.h"
#include "amqpClient/api/Consumer.h"
#include "amqpClient/api/GetResponse.h"
#include "amqpClient/api/ReturnListener.h"
#include "amqpClient/api/amqpClient.h"
#include "amqpClient/AMQChannel.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#include "AMQUtil.h"

using namespace Caf::AmqpClient;

const uint8 AMQChannel::DEBUGLOG_FLAG_ENTRYEXIT = 0x01;
const uint8 AMQChannel::DEBUGLOG_FLAG_AMQP = 0x02;

#define AMQCHANNEL_ENTRY \
	if (_debugLogFlags & DEBUGLOG_FLAG_ENTRYEXIT) { CAF_CM_LOG_DEBUG_VA0("entry"); }

#define AMQCHANNEL_EXIT \
	if (_debugLogFlags & DEBUGLOG_FLAG_ENTRYEXIT) { CAF_CM_LOG_DEBUG_VA0("exit"); }

#if (1) // init
AMQChannel::AMQChannel() :
	_isInitialized(false),
	_isOpen(false),
	_debugLogFlags(0),
	_channelNumber(0),
	CAF_CM_INIT_LOG("AMQChannel") {
	CAF_CM_INIT_THREADSAFE;
	_channelMutex.CreateInstance();
	_channelMutex->initialize();
}

AMQChannel::~AMQChannel() {
	if (_channelHandle) {
		AmqpChannel::AMQP_ChannelClose(_channelHandle);
	}
}

void AMQChannel::init(
		const SmartPtrIConnectionInt& connection,
		const SmartPtrConsumerWorkService& workService) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(connection);
	CAF_CM_VALIDATE_SMARTPTR(workService);

	uint32 debugFlags = 0;
	if (getAppConfig()->getUint32(
			"AMQChannel",
			"debugLogFlags",
			debugFlags,
			IConfigParams::PARAM_OPTIONAL)) {
		_debugLogFlags = debugFlags;
	}

	AMQCHANNEL_ENTRY;
	_connection = connection;
	_workService = workService;
	_channelSignal.initialize("channelSignal");
	_dispatcher.CreateInstance();
	_dispatcher->init(_workService);

	// Sequence is important here. Once amqpConnectionOpenChannel is called the
	// channel.open method will be sent.  Therefore we must have an _activeRpc
	// registered to handle the channel.open-ok response BEFORE activating the
	// _channelTask.

	// Create the _activeRpc to listen for channel-open.ok
	SmartPtrBlockingRpcContinuation continuation;
	continuation.CreateInstance();
	continuation->init();
	_activeRpc = continuation;

	// Open the channel
	AMQUtil::checkAmqpStatus(
			_connection->amqpConnectionOpenChannel(_channelHandle),
			"_connection->amqpConnectionOpenChannel");
	AMQUtil::checkAmqpStatus(
			AmqpChannel::AMQP_ChannelGetId(_channelHandle, &_channelNumber),
			"AmqpChannel::AMQP_ChannelGetId");

	// Set up AMQP frame processing
	_command.CreateInstance();
	_command->init();
	SmartPtrChannelTask channelTask;
	channelTask.CreateInstance();
	channelTask->init(this);
	_workService->addWork(channelTask);

	// Wait for the channel-open.ok response
	SmartPtrAMQCommand command;
	{
		CAF_CM_UNLOCK_LOCK;
		command = continuation->getReply();
	}
	CAF_CM_VALIDATE_SMARTPTR(command);
	AmqpMethods::Channel::SmartPtrOpenOk openOk;
	openOk.QueryInterface(command->getMethod(), false);
	if (!openOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				IllegalStateException,
				0,
				"Expected to receive channel.open-ok but received "
				"%s instead. This channel cannot be used.",
				command->getMethod()->getProtocolMethodName().c_str());
	}

	if (_debugLogFlags & DEBUGLOG_FLAG_AMQP) {
		CAF_CM_LOG_DEBUG_VA1("channel #%d is open", _channelNumber);
	}
	_isOpen = true;
	_isInitialized = true;
	AMQCHANNEL_EXIT;
}

uint16 AMQChannel::getChannelNumber() {
	CAF_CM_FUNCNAME_VALIDATE("getChannelNumber");
	CAF_CM_LOCK_UNLOCK;
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ensureIsOpen();
	AMQCHANNEL_EXIT;
	return _channelNumber;
}

bool AMQChannel::isOpen() {
	CAF_CM_LOCK_UNLOCK;
	return _isOpen;
}

void AMQChannel::close() {
	CAF_CM_FUNCNAME_VALIDATE("close");
	AMQCHANNEL_ENTRY;
	_connection->channelCloseChannel(this);
	AMQCHANNEL_EXIT;
}

void AMQChannel::notifyConnectionClosed(SmartPtrCCafException& exception) {
	CAF_CM_FUNCNAME_VALIDATE("notifyConnectionClosed");
	AMQCHANNEL_ENTRY
	close(exception);
	AMQCHANNEL_EXIT;
}

void AMQChannel::close(SmartPtrCCafException& exception) {
	CAF_CM_FUNCNAME_VALIDATE("close");
	CAF_CM_LOCK_UNLOCK;
	AMQCHANNEL_ENTRY
	if (_debugLogFlags & DEBUGLOG_FLAG_AMQP) {
		CAF_CM_LOG_DEBUG_VA1("Closing channel #%d", _channelNumber);
	}
	if (_isOpen) {
		_isOpen = false;
		_dispatcher->quiesce();

		SmartPtrIRpcContinuation activeRpc;
		{
			CAF_CM_UNLOCK_LOCK;
			activeRpc = nextOutstandingRpc();
		}
		if (activeRpc) {
			activeRpc->handleAbort(exception);
		}
		if (_dispatcher) {
			_dispatcher->handleShutdown(exception);
		}
		AMQPStatus status = AmqpChannel::AMQP_ChannelClose(_channelHandle);
		if (status != AMQP_ERROR_OK) {
			CAF_CM_LOG_WARN_VA2(
					"channel #%d closed with API code %d",
					_channelNumber,
					status);
		}
	}
	AMQCHANNEL_EXIT;
}
#endif

#if (1) // basic
void AMQChannel::basicAck(
	const uint64 deliveryTag,
	const bool ackMultiple) {
	CAF_CM_FUNCNAME_VALIDATE("basicAck");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrBasicAckMethod method;
	method.CreateInstance();
	method->init(deliveryTag, ackMultiple);
	transmit(method);
	AMQCHANNEL_EXIT;
}

SmartPtrGetResponse AMQChannel::basicGet(
	const std::string& queue,
	const bool noAck) {
	CAF_CM_FUNCNAME("basicGet");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrBasicGetMethod method;
	method.CreateInstance();
	method->init(queue, noAck);
	SmartPtrAMQCommand reply = execRpc(method);
	SmartPtrIMethod replyMethod = reply->getMethod();

	SmartPtrGetResponseImpl getResponse;
	AmqpMethods::Basic::SmartPtrGetOk getOk;
	getOk.QueryInterface(replyMethod, false);
	if (getOk) {
		AmqpContentHeaders::SmartPtrBasicProperties properties;
		SmartPtrIContentHeader contentHeader = reply->getContentHeader();
		if (contentHeader) {
			properties.QueryInterface(contentHeader, false);
			if (!properties) {
				CAF_CM_EXCEPTIONEX_VA1(
						NoSuchInterfaceException,
						0,
						"Expected a basic properties content header. Received '%s'. "
						"Please report this bug.",
						contentHeader->getClassName().c_str());
			}
		}

		SmartPtrEnvelopeImpl envelope;
		envelope.CreateInstance();
		envelope->init(
				getOk->getDeliveryTag(),
				getOk->getRedelivered(),
				getOk->getExchange(),
				getOk->getRoutingKey());
		getResponse.CreateInstance();
		getResponse->init(
				envelope,
				properties,
				reply->getContentBody(),
				getOk->getMessageCount());
	} else {
		AmqpMethods::Basic::SmartPtrGetEmpty getEmpty;
		getEmpty.QueryInterface(replyMethod, false);
		if(!getEmpty) {
			CAF_CM_EXCEPTIONEX_VA2(
					NoSuchInterfaceException,
					0,
					"Expected a basic.get-ok or basic.get-empty response. Received '%s'. "
					"Please report this bug.",
					method->getMethodName().c_str(),
					replyMethod->getProtocolMethodName().c_str());
		}
	}
	AMQCHANNEL_EXIT;
	return getResponse;
}

void AMQChannel::basicPublish(
	const std::string& exchange,
	const std::string& routingKey,
	const AmqpContentHeaders::SmartPtrBasicProperties& properties,
	const SmartPtrCDynamicByteArray& body) {
	basicPublish(
			exchange,
			routingKey,
			false,
			false,
			properties,
			body);
}

void AMQChannel::basicPublish(
	const std::string& exchange,
	const std::string& routingKey,
	const bool mandatory,
	const bool immediate,
	const AmqpContentHeaders::SmartPtrBasicProperties& properties,
	const SmartPtrCDynamicByteArray& body) {
	CAF_CM_FUNCNAME_VALIDATE("basicPublish");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrBasicPublishMethod method;
	method.CreateInstance();
	method->init(
			exchange,
			routingKey,
			mandatory,
			immediate,
			properties,
			body);
	transmit(method);
	AMQCHANNEL_EXIT;
}

AmqpMethods::Basic::SmartPtrConsumeOk AMQChannel::basicConsume(
		const std::string& queue,
		const SmartPtrConsumer& consumer) {
	return basicConsume(queue, false, consumer);
}

AmqpMethods::Basic::SmartPtrConsumeOk AMQChannel::basicConsume(
		const std::string& queue,
		const bool noAck,
		const SmartPtrConsumer& consumer) {
	return basicConsume(queue, "", noAck, false, false, consumer);
}

AmqpMethods::Basic::SmartPtrConsumeOk AMQChannel::basicConsume(
		const std::string& queue,
		const std::string& consumerTag,
		const bool noAck,
		const bool noLocal,
		const bool exclusive,
		const SmartPtrConsumer& consumer,
		const SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME("basicConsume");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	AmqpMethods::Basic::SmartPtrConsumeOk consumeOk;
	// Put a lock on the dispatcher.
	// DO NOT FORGET TO UNLOCK IT NO MATTER WHAT!
	_dispatcher->lock();
	try {
		SmartPtrBasicConsumeMethod method;
		method.CreateInstance();
		method->init(queue, consumerTag, noLocal, noAck, exclusive, arguments);
		SmartPtrAMQCommand reply = execRpc(method);
		SmartPtrIMethod replyMethod = reply->getMethod();
		consumeOk.QueryInterface(replyMethod, false);
		if (consumeOk) {
			const std::string& consumerTagActual = consumeOk->getConsumerTag();
			_dispatcher->addConsumer(consumerTagActual, consumer);
			_dispatcher->handleConsumeOk(consumerTagActual);
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					NoSuchInterfaceException,
					0,
					"Expected a basic.consume-ok response. Received '%s'. "
					"Please report this bug.",
					replyMethod->getProtocolMethodName().c_str());
		}
	}
	CAF_CM_CATCH_ALL;
	_dispatcher->unlock();
	CAF_CM_THROWEXCEPTION;
	AMQCHANNEL_EXIT;
	return consumeOk;
}

AmqpMethods::Basic::SmartPtrCancelOk AMQChannel::basicCancel(
		const std::string& consumerTag) {
	CAF_CM_FUNCNAME("basicCancel");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	AmqpMethods::Basic::SmartPtrCancelOk cancelOk;
	SmartPtrConsumer originalConsumer = _dispatcher->getConsumer(consumerTag);
	if (originalConsumer) {
		SmartPtrBasicCancelMethod method;
		method.CreateInstance();
		method->init(consumerTag);
		SmartPtrAMQCommand reply = execRpc(method);
		SmartPtrIMethod replyMethod = reply->getMethod();
		cancelOk.QueryInterface(replyMethod, false);
		if (cancelOk) {
			try {
				_dispatcher->handleCancelOk(consumerTag);
			}
			CAF_CM_CATCH_ALL;
			_dispatcher->removeConsumer(consumerTag);
			CAF_CM_THROWEXCEPTION;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					NoSuchInterfaceException,
					0,
					"Expected a basic.cancel-ok response. Received '%s'. "
					"Please report this bug.",
					replyMethod->getProtocolMethodName().c_str());
		}
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"No such consumer tag '%s'",
				consumerTag.c_str());
	}
	AMQCHANNEL_EXIT;
	return cancelOk;
}

AmqpMethods::Basic::SmartPtrRecoverOk AMQChannel::basicRecover(
		const bool requeue) {
	CAF_CM_FUNCNAME("basicRecover");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	AmqpMethods::Basic::SmartPtrRecoverOk recoverOk;
	SmartPtrBasicRecoverMethod method;
	method.CreateInstance();
	method->init(requeue);
	SmartPtrAMQCommand reply = execRpc(method);
	SmartPtrIMethod replyMethod = reply->getMethod();
	recoverOk.QueryInterface(replyMethod, false);
	if (recoverOk) {
		_dispatcher->handleRecoverOk();
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a basic.recover-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return recoverOk;
}

AmqpMethods::Basic::SmartPtrQosOk AMQChannel::basicQos(
		const uint32 prefetchSize,
		const uint32 prefetchCount,
		const bool global) {
	CAF_CM_FUNCNAME("basicQos");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrBasicQosMethod method;
	method.CreateInstance();
	method->init(prefetchSize, prefetchCount, global);
	SmartPtrAMQCommand reply = execRpc(method);
	AmqpMethods::Basic::SmartPtrQosOk qosOk;
	SmartPtrIMethod replyMethod = reply->getMethod();
	qosOk.QueryInterface(replyMethod, false);
	if (!qosOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a basic.qos-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return qosOk;
}

void AMQChannel::basicReject(
		const uint64 deliveryTag,
		const bool requeue) {
	CAF_CM_FUNCNAME_VALIDATE("basicReject");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrBasicRejectMethod method;
	method.CreateInstance();
	method->init(deliveryTag, requeue);
	transmit(method);
	AMQCHANNEL_EXIT;
}
#endif

#if (1) // exchange
AmqpMethods::Exchange::SmartPtrDeclareOk AMQChannel::exchangeDeclare(
	const std::string& exchange,
	const std::string& type,
	const bool durable,
	const SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME("exchangeDeclare");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrExchangeDeclareMethod method;
	method.CreateInstance();
	method->init(exchange, type, false, durable, arguments);
	SmartPtrAMQCommand reply = execRpc(method);
	AmqpMethods::Exchange::SmartPtrDeclareOk declareOk;
	SmartPtrIMethod replyMethod = reply->getMethod();
	declareOk.QueryInterface(replyMethod, false);
	if (!declareOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a exchange.declare-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return declareOk;
}

AmqpMethods::Exchange::SmartPtrDeleteOk AMQChannel::exchangeDelete(
	const std::string& exchange,
	const bool ifUnused) {
	CAF_CM_FUNCNAME("exchangeDelete");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrExchangeDeleteMethod method;
	method.CreateInstance();
	method->init(exchange, ifUnused);
	SmartPtrAMQCommand reply = execRpc(method);
	AmqpMethods::Exchange::SmartPtrDeleteOk deleteOk;
	SmartPtrIMethod replyMethod = reply->getMethod();
	deleteOk.QueryInterface(replyMethod, false);
	if (!deleteOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a exchange.delete-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return deleteOk;
}

#endif

#if (1) // queue
AmqpMethods::Queue::SmartPtrDeclareOk AMQChannel::queueDeclare() {
	return queueDeclare(
			"",
			false,
			true,
			true);
}

AmqpMethods::Queue::SmartPtrDeclareOk AMQChannel::queueDeclare(
	const std::string& queue,
	const bool durable,
	const bool exclusive,
	const bool autoDelete,
	const SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME("queueDeclare");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrQueueDeclareMethod method;
	method.CreateInstance();
	method->init(queue, durable, exclusive, autoDelete, arguments);
	SmartPtrAMQCommand reply = execRpc(method);
	AmqpMethods::Queue::SmartPtrDeclareOk declareOk;
	SmartPtrIMethod replyMethod = reply->getMethod();
	declareOk.QueryInterface(replyMethod, false);
	if (!declareOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a queue.declare-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return declareOk;
}

AmqpMethods::Queue::SmartPtrDeclareOk AMQChannel::queueDeclarePassive(
	const std::string& queue) {
	CAF_CM_FUNCNAME("queueDeclarePassive");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrQueueDeclareMethod method;
	method.CreateInstance();
	method->initPassive(queue);
	SmartPtrAMQCommand reply = execRpc(method);
	AmqpMethods::Queue::SmartPtrDeclareOk declareOk;
	SmartPtrIMethod replyMethod = reply->getMethod();
	declareOk.QueryInterface(replyMethod, false);
	if (!declareOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a queue.declare-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return declareOk;
}

AmqpMethods::Queue::SmartPtrDeleteOk AMQChannel::queueDelete(
	const std::string& queue,
	const bool ifUnused,
	const bool ifEmpty) {
	CAF_CM_FUNCNAME("queueDelete");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrQueueDeleteMethod method;
	method.CreateInstance();
	method->init(queue, ifUnused, ifEmpty);
	SmartPtrAMQCommand reply = execRpc(method);
	AmqpMethods::Queue::SmartPtrDeleteOk deleteOk;
	SmartPtrIMethod replyMethod = reply->getMethod();
	deleteOk.QueryInterface(replyMethod, false);
	if (!deleteOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a queue.delete-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return deleteOk;
}

AmqpMethods::Queue::SmartPtrPurgeOk AMQChannel::queuePurge(
	const std::string& queue) {
	CAF_CM_FUNCNAME("queuePurge");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrQueuePurgeMethod method;
	method.CreateInstance();
	method->init(queue);
	SmartPtrAMQCommand reply = execRpc(method);
	AmqpMethods::Queue::SmartPtrPurgeOk purgeOk;
	SmartPtrIMethod replyMethod = reply->getMethod();
	purgeOk.QueryInterface(replyMethod, false);
	if (!purgeOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a queue.purge-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return purgeOk;
}

AmqpMethods::Queue::SmartPtrBindOk AMQChannel::queueBind(
	const std::string& queue,
	const std::string& exchange,
	const std::string& routingKey,
	const SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME("queueBind");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrQueueBindMethod method;
	method.CreateInstance();
	method->init(queue, exchange, routingKey, arguments);
	SmartPtrAMQCommand reply = execRpc(method);
	AmqpMethods::Queue::SmartPtrBindOk bindOk;
	SmartPtrIMethod replyMethod = reply->getMethod();
	bindOk.QueryInterface(replyMethod, false);
	if (!bindOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a queue.bind-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return bindOk;
}

AmqpMethods::Queue::SmartPtrUnbindOk AMQChannel::queueUnbind(
	const std::string& queue,
	const std::string& exchange,
	const std::string& routingKey,
	const SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME("queueUnbind");
	AMQCHANNEL_ENTRY;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrQueueUnbindMethod method;
	method.CreateInstance();
	method->init(queue, exchange, routingKey, arguments);
	SmartPtrAMQCommand reply = execRpc(method);
	AmqpMethods::Queue::SmartPtrUnbindOk unbindOk;
	SmartPtrIMethod replyMethod = reply->getMethod();
	unbindOk.QueryInterface(replyMethod, false);
	if (!unbindOk) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Expected a queue.unbind-ok response. Received '%s'. "
				"Please report this bug.",
				replyMethod->getProtocolMethodName().c_str());
	}
	AMQCHANNEL_EXIT;
	return unbindOk;
}
#endif
#if (1) // ReturnListener
void AMQChannel::addReturnListener(
		const SmartPtrReturnListener& listener) {
	CAF_CM_FUNCNAME_VALIDATE("addReturnListener");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_LOCK_UNLOCK;
	_returnListeners.add(listener);
}

bool AMQChannel::removeReturnListener(
		const SmartPtrReturnListener& listener) {
	CAF_CM_FUNCNAME_VALIDATE("removeReturnListener");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _returnListeners.remove(listener);
}
#endif

#if (1) // processing
/*
 * Worker thread execution callback.
 * Check for an incoming frame and if one exists, process it.
 * This method is also called during init() and it waiting for the
 * channel.open-ok. While establishing the connection we may get timeout errors
 * because the broker is down.  In that case retry the connection for the allotted
 * time before giving up.
 */
bool AMQChannel::taskHandler() {
	CAF_CM_FUNCNAME("taskHandler");
	CAF_CM_LOCK_UNLOCK;

	try {
		uint32 frameCount = 0;
		AMQPStatus status = AMQP_ERROR_OK;
		SmartPtrCAmqpFrame frame;
		while (_channelHandle && (frameCount < 1000)) {
			SmartPtrCAmqpChannel channelHandle = _channelHandle;
			{
				CAF_CM_UNLOCK_LOCK;
				status = AmqpChannel::AMQP_ChannelReceive(channelHandle, frame, 0);
			}
			if (frame) {
				++frameCount;
				try {
					SmartPtrAMQCommand command = _command;
					if (command->handleFrame(frame)) {
						_command.CreateInstance();
						_command->init();
						handleCompleteInboundCommand(command);
					}
				}
				CAF_CM_CATCH_ALL;
				if (CAF_CM_ISEXCEPTION) {
					CAF_CM_LOG_CRIT_VA1("channel #%d", _channelNumber);
					CAF_CM_LOG_CRIT_CAFEXCEPTION;

					// Throw away the current command
					_command.CreateInstance();
					_command->init();

					// Abort any outstanding rpc
					SmartPtrIRpcContinuation rpc;
					{
						CAF_CM_UNLOCK_LOCK;
						rpc = nextOutstandingRpc();
					}
					if (rpc) {
						SmartPtrCCafException exception = CAF_CM_GETEXCEPTION;
						rpc->handleAbort(exception);
					}
					CAF_CM_CLEAREXCEPTION;
				}
			} else if ((status == AMQP_ERROR_TIMEOUT) || (status == AMQP_ERROR_IO_INTERRUPTED)) {
				break;
			} else {
				AMQUtil::checkAmqpStatus(status, "AmqpChannel::AMQP_ChannelReceive");
			}
		}
	}
	CAF_CM_CATCH_ALL;
	if (CAF_CM_ISEXCEPTION) {
		CAF_CM_LOG_CRIT_VA1("channel #%d", _channelNumber);
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}

	// If we get here and the _channelHandle is NULL then
	// the channel is closed.  If so, return 'true'.
	//
	// If the _channelHandle is still good, return 'false' and let
	// this task run again.
	return (_channelHandle == NULL);
}

/*
 * A complete inbound AMQP command is available for processing.
 */
void AMQChannel::handleCompleteInboundCommand(const SmartPtrAMQCommand& command) {
	CAF_CM_FUNCNAME("handleCompleteInboundCommand");
	// No LOCK_UNLOCK because there aren't any members to protect and because
	// this routine makes a blocking call.
	AMQCHANNEL_ENTRY;
	// Check with asyn processing first
	if (!processAsync(command)) {
		// Command was not handled so must be part of an outstanding
		// RPC call.  Get the outstanding RPC call and have it process
		// it.
		//
		// The outstanding RPC call may have been aborted because we are
		// closing.  That is the only time the RPC object should be null
		// when in this method
		SmartPtrIRpcContinuation rpc = nextOutstandingRpc();
		if (rpc) {
			rpc->handleCommand(command);
		} else if (isOpen()) {
			CAF_CM_EXCEPTIONEX_VA3(
					NullPointerException,
					0,
					"[command=%s, class_id: 0x%08x, method_id: 0x%08x] nextOutstandingRpc() returned NULL and the channel is open. "
					"This should never happen. Please report this bug.",
					command->getMethod()->getProtocolMethodName().c_str(),
					command->getMethod()->getProtocolClassId(),
					command->getMethod()->getProtocolMethodId());
		}
	}
	AMQCHANNEL_EXIT;
}

/*
 * First line of incomming command processing. This method handles
 * non-RPC commands such as channel.close, basic.deliver, basic.recover, etc.
 */
bool AMQChannel::processAsync(const SmartPtrAMQCommand& command) {
	CAF_CM_FUNCNAME("processAsync");
	CAF_CM_LOCK_UNLOCK;
	AMQCHANNEL_ENTRY;
	bool commandHandled = false;
	const amqp_method_number_t amqpMethodId =
			(command->getMethod()->getProtocolClassId() << 16) |
			command->getMethod()->getProtocolMethodId();

	if (_debugLogFlags & DEBUGLOG_FLAG_AMQP) {
		CAF_CM_LOG_DEBUG_VA4(
				"Method [channel=%d][class id=0x%04X][method id=0x%04X][name=%s]",
				_channelNumber,
				command->getMethod()->getProtocolClassId(),
				command->getMethod()->getProtocolMethodId(),
				command->getMethod()->getProtocolMethodName().c_str());
	}

	if (amqpMethodId == AMQP_CHANNEL_OPEN_OK_METHOD) {
		// no-op. let it pass through
	}
	else if (amqpMethodId == AMQP_CHANNEL_CLOSE_METHOD) {
		// first order of business - stop the dispatcher from handling incoming messages
		_dispatcher->quiesce();

		AmqpMethods::Channel::SmartPtrClose chClose;
		chClose.QueryInterface(command->getMethod(), false);
		if (chClose) {
			if (_debugLogFlags & DEBUGLOG_FLAG_AMQP) {
				CAF_CM_LOG_INFO_VA5(
						"channel.close %s [channel=%d][code=0x%04X][class id=0x%04X][method id=0x%04X]",
						chClose->getReplyText().c_str(),
						_channelNumber,
						chClose->getReplyCode(),
						chClose->getClassId(),
						chClose->getMethodId());
			}
		} else {
			// Okay - very weird but we need to keep on going...
			CAF_CM_LOG_CRIT_VA0(
					"Received AMQP_CHANNEL_CLOSE_METHOD but method object "
					"is not a AmqpMethods::Channel::Close instance. Please report "
					"this bug.");
		}
		{
			CAF_CM_UNLOCK_LOCK;
			channelCloseByServerShutdown(chClose);
		}
		commandHandled = true;
	} else if (amqpMethodId == AMQP_CHANNEL_CLOSE_OK_METHOD) {
		_channelHandle = AMQP_HANDLE_INVALID;
		commandHandled = true;
	} else if (isOpen()) {
		try {
			SmartPtrIMethod method = command->getMethod();
			SmartPtrIContentHeader contentHeader = command->getContentHeader();
			switch (amqpMethodId) {
			case AMQP_BASIC_DELIVER_METHOD:
				{
					commandHandled = true;
					AmqpMethods::Basic::SmartPtrDeliver deliverMethod;
					deliverMethod.QueryInterface(method, false);
					if (deliverMethod) {
						SmartPtrEnvelopeImpl envelope;
						envelope.CreateInstance();
						envelope->init(
								deliverMethod->getDeliveryTag(),
								deliverMethod->getRedelivered(),
								deliverMethod->getExchange(),
								deliverMethod->getRoutingKey());

						AmqpContentHeaders::SmartPtrBasicProperties properties;
						if (contentHeader) {
							properties.QueryInterface(contentHeader, false);
							if (!properties) {
								CAF_CM_EXCEPTIONEX_VA1(
										NoSuchInterfaceException,
										0,
										"Expected a basic properties content header. Received '%s'. "
										"Please report this bug.",
										contentHeader->getClassName().c_str());
							}
						}
						_dispatcher->handleDelivery(
								deliverMethod->getConsumerTag(),
								envelope,
								properties,
								command->getContentBody());
					} else {
						CAF_CM_EXCEPTIONEX_VA0(
								IllegalStateException,
								0,
								"Received AMQP_BASIC_DELIVER_METHOD but the method object "
								"is not a AmqpClient::AmqpMethods::Basic::Deliver instance. "
								"Please report this bug.");
					}
				}
			break;

			case AMQP_BASIC_RETURN_METHOD:
				callReturnListeners(command);
				commandHandled = true;
				break;
			}
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		if (CAF_CM_ISEXCEPTION) {
			// discard the command
			commandHandled = true;
		}
		CAF_CM_CLEAREXCEPTION;
	} else {
		// We are shutting down so the inbound command should be
		// discarded per spec.  'Consume' it by returning true.
		if (_debugLogFlags & DEBUGLOG_FLAG_AMQP) {
			CAF_CM_LOG_INFO_VA0("isOpen() is false. Discarding command.");
		}
		commandHandled = true;
	}
	AMQCHANNEL_EXIT;
	return commandHandled;
}

/*
 * Checks the _isOpen flag and throws an exception if the
 * channel is closed.
 */
void AMQChannel::ensureIsOpen() {
	CAF_CM_FUNCNAME("ensureIsOpen");
	CAF_CM_LOCK_UNLOCK;
	AMQCHANNEL_ENTRY;
	if (!isOpen()) {
		CAF_CM_EXCEPTIONEX_VA0(
				AmqpExceptions::ChannelClosedException,
				0,
				"Attempt to use closed channel");
	}
	AMQCHANNEL_EXIT;
}

/*
 * Execute a synchronous call such as basic.get, queue.declare, exchange.delete,
 * etc.  The AMQP synchronous calls are all executed through this mechanism.
 */
SmartPtrAMQCommand AMQChannel::execRpc(const SmartPtrIServerMethod& method) {
	CAF_CM_FUNCNAME("execRpc");
	AMQCHANNEL_ENTRY;
	SmartPtrBlockingRpcContinuation rpc;
	try {
		CAF_CM_LOCK_UNLOCK1(_channelMutex);
		CAF_CM_LOCK_UNLOCK;

		ensureIsOpen();

		const std::string methodName = method->getMethodName();
		rpc.CreateInstance();
		rpc->init();

		// Wait for the current rpc to finish
		while (_activeRpc) {
			{
				CAF_CM_UNLOCK_LOCK;
				_channelSignal.waitOrTimeout(_channelMutex, 0);
			}
		}

		// indicate the new active RPC call
		_activeRpc = rpc;

		if (_debugLogFlags & DEBUGLOG_FLAG_AMQP) {
			CAF_CM_LOG_DEBUG_VA2(
					"[channel=%d] Sending AMQP method %s",
					_channelNumber,
					method->getMethodName().c_str());
		}

		// send the call to the server
		AMQUtil::checkAmqpStatus(
			method->send(_channelHandle),
			methodName.c_str());
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_THROWEXCEPTION;

	// Check the reply. If null there should be
	// a reason (exception) explaining why the call failed.
	// This getReply() doesn't need to be in a UNLOCK_LOCK because the LOCK_UNLOCK
	// was scoped by the try.
	SmartPtrAMQCommand reply = rpc->getReply();
	CAF_CM_LOG_DEBUG_VA1("RPC reply - %s", (reply.IsNull() ? "NULL" : reply->getMethod()->getProtocolMethodName().c_str()));
	AMQCHANNEL_EXIT;
	if (!reply) {
		SmartPtrCCafException exception = rpc->getException();
		if (exception) {
			exception->throwAddRefedSelf();
		} else {
			CAF_CM_EXCEPTIONEX_VA0(
					IllegalStateException,
					0,
					"AMQP reply was not returned and no exception (reason) was provided.");
		}
	}
	return reply;
}

/*
 * Retrieve the outstanding RPC call.  This method interacts with the execRpc()
 * method indirectly through the _channelMutex and _channelSignal.
 * The execRpc() call blocks until the current RPC call is handled by the thread
 * running the taskHandler (which eventually calls handleCompleteInboundCommand())
 * which calls this.
 *
 * When handleCompleteInboundCommand() gets the outstanding RPC to handle the command,
 * the _activeRpc member is cleared and _channelSignal is signaled thus freeing
 * the execRpc() call to execute its RPC.
 */
SmartPtrIRpcContinuation AMQChannel::nextOutstandingRpc() {
	CAF_CM_FUNCNAME_VALIDATE("nextOutstandingRpc");

	CAF_CM_LOCK_UNLOCK1(_channelMutex);
	CAF_CM_LOCK_UNLOCK;

	SmartPtrIRpcContinuation result = _activeRpc;
	_activeRpc = NULL;
	_channelSignal.signal();
	AMQCHANNEL_EXIT;
	return result;
}

/*
 * Transmit an AMQP method to the server
 */
void AMQChannel::transmit(const SmartPtrIServerMethod& method) {
	CAF_CM_FUNCNAME_VALIDATE("transmit");
	CAF_CM_LOCK_UNLOCK;
	AMQCHANNEL_ENTRY;
	CAF_CM_VALIDATE_SMARTPTR(method);
	if (_debugLogFlags & DEBUGLOG_FLAG_AMQP) {
		CAF_CM_LOG_DEBUG_VA2(
				"[channel=%d] Sending AMQP method %s",
				_channelNumber,
				method->getMethodName().c_str());
	}
	AMQUtil::checkAmqpStatus(method->send(_channelHandle));
	AMQCHANNEL_EXIT;
}

/*
 * This method is called when we have received a channel.close method
 * from the server.  Respond with a channel.close-ok method then
 * abort the outstanding RPC (if any) with the exception (reason) for the abort.
 */
void AMQChannel::channelCloseByServerShutdown(
	const AmqpMethods::Channel::SmartPtrClose& closeMethod) {
	CAF_CM_FUNCNAME("channelCloseByServerShutdown");
	AMQCHANNEL_ENTRY;
	try {
		CAF_CM_LOCK_UNLOCK1(_channelMutex);
		CAF_CM_LOCK_UNLOCK;

		_isOpen = false;
		_dispatcher->quiesce();

		// Send channel.close-ok
		SmartPtrChannelCloseOkMethod method;
		method.CreateInstance();
		method->init();
		transmit(method);

		// Create the reason for the shutdown.
		AmqpExceptions::SmartPtrChannelClosedByServerException exception;
		try {
			exception.CreateInstance();

			// We *should* have received a Channel::Close method BUT you can
			// never be too careful...
			if (closeMethod) {
				exception->populateVA(
						closeMethod->getReplyCode(),
						_cm_className_,
						_cm_funcName_,
						"channel.close %s [channel=%d][code=0x%04X][class id=0x%04X][method id=0x%04X]",
						closeMethod->getReplyText().c_str(),
						_channelNumber,
						closeMethod->getReplyCode(),
						closeMethod->getClassId(),
						closeMethod->getMethodId());
			} else {
				exception->populate(
						"channel.close - no other information available",
						0,
						_cm_className_,
						_cm_funcName_);
			}

			// Abort the outstanding rpc if any
			if (_activeRpc) {
				SmartPtrIRpcContinuation rpc = _activeRpc;
				_activeRpc = NULL;
				rpc->handleAbort(exception);
			}

			// Notify the dispatcher
			_dispatcher->handleShutdown(exception);
		}
		CAF_CM_CATCH_ALL;

		// Tear down
		AmqpChannel::AMQP_ChannelClose(_channelHandle);
		_channelHandle = AMQP_HANDLE_INVALID;

		// Remove this channel from management
		_connection->notifyChannelClosedByServer(_channelNumber);
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
	AMQCHANNEL_EXIT;
}

void AMQChannel::callReturnListeners(const SmartPtrAMQCommand& command) {
	CAF_CM_FUNCNAME("callReturnListeners");
	CAF_CM_LOCK_UNLOCK;
	try {
		AmqpMethods::Basic::SmartPtrReturn basicReturn;
		basicReturn.QueryInterface(command->getMethod(), false);
		if (basicReturn) {
			CowReturnListenerCollection::SmartPtrContainer listeners =
					_returnListeners.getAll();
			AmqpContentHeaders::SmartPtrBasicProperties properties;
			properties.QueryInterface(command->getContentHeader(), false);
			if (properties) {
				for (TSmartIterator<ReturnListenerCollection> listener(*listeners);
						listener;
						listener++) {
					listener->handleReturn(
							basicReturn->getReplyCode(),
							basicReturn->getReplyText(),
							basicReturn->getExchange(),
							basicReturn->getRoutingKey(),
							properties,
							command->getContentBody());
				}
			} else {
				CAF_CM_EXCEPTIONEX_VA1(
						NoSuchInterfaceException,
						0,
						"Expected content header to be a basic.properties object. Instead it is "
						"a '%s' object. Please report this bug.",
						command->getContentHeader()->getClassName().c_str());
			}
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					NoSuchInterfaceException,
					0,
					"Expected command to be a basic.return command.  Instead it is a "
					"'%s' command. Please report this bug.",
					command->getMethod()->getProtocolMethodName().c_str());
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

#endif

#if (1) // ChannelTask
AMQChannel::ChannelTask::ChannelTask() :
		CAF_CM_INIT("AMQChannel::ChannelTask")	{
}

AMQChannel::ChannelTask::~ChannelTask() {
}

void AMQChannel::ChannelTask::init(SmartPtrAMQChannel channel) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_channel);
	CAF_CM_VALIDATE_PTR(channel);
	_channel = channel;
}

bool AMQChannel::ChannelTask::run() {
	CAF_CM_FUNCNAME_VALIDATE("run");
	CAF_CM_PRECOND_ISINITIALIZED(_channel);
	return  _channel->taskHandler();
}
#endif
