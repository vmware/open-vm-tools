/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IBean.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationAppContext.h"
#include "Integration/IIntegrationObject.h"
#include "amqpCore/Binding.h"
#include "amqpCore/BindingInternal.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpCore/Exchange.h"
#include "amqpCore/ExchangeInternal.h"
#include "amqpCore/Queue.h"
#include "amqpCore/QueueInternal.h"
#include "RabbitAdminInstance.h"

using namespace Caf::AmqpIntegration;

RabbitAdminInstance::RabbitAdminInstance() :
	_isRunning(false),
	CAF_CM_INIT_LOG("RabbitAdminInstance") {
}

RabbitAdminInstance::~RabbitAdminInstance() {
}

void RabbitAdminInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	_id = configSection->findOptionalAttribute("id");
	if (!_id.length()) {
		_id = "RabbitAdminInstance-";
		_id += CStringUtils::createRandomUuid();
	}
	_connectionFactoryId = configSection->findRequiredAttribute("connection-factory");
	_admin.CreateInstance();
}

std::string RabbitAdminInstance::getId() const {
	return _id;
}

void RabbitAdminInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_VALIDATE_INTERFACE(appContext);
	_appContext = appContext;
}

void RabbitAdminInstance::setIntegrationAppContext(SmartPtrIIntegrationAppContext context) {
	CAF_CM_FUNCNAME_VALIDATE("setIntegrationAppContext");
	CAF_CM_VALIDATE_INTERFACE(context);
	_integrationAppContext = context;
}

void RabbitAdminInstance::start(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("start");

	SmartPtrIBean factoryBean = _appContext->getBean(_connectionFactoryId);
	SmartPtrConnectionFactory connectionFactory;
	connectionFactory.QueryInterface(factoryBean, true);
	_admin->init(connectionFactory);

	try {
		CAF_CM_VALIDATE_INTERFACE(_integrationAppContext);
		CAF_CM_LOG_DEBUG_VA0("Initializing exchange/queue/binding declarations");

		std::deque<SmartPtrQueue> queues;
		IIntegrationAppContext::SmartPtrCObjectCollection objs =
				_integrationAppContext->getIntegrationObjects(Queue::IIDOF());
		CAF_CM_LOG_DEBUG_VA1("Declaring %d queues", objs->size());
		if (objs->size()) {
			for (TSmartConstIterator<IIntegrationAppContext::CObjectCollection> obj(*objs);
					obj;
					obj++) {
				SmartPtrQueue queue;
				queue.QueryInterface(*obj);
				if (queue->getName().length()) {
					CAF_CM_LOG_DEBUG_VA1("Declaring queue - %s", queue->getName().c_str());
					_admin->declareQueue(queue);
				} else {
					CAF_CM_LOG_DEBUG_VA0("Declaring anonymous queue");
					SmartPtrQueue anonQueue = _admin->declareQueue();
					SmartPtrQueueInternal queueInt;
					queueInt.QueryInterface(queue);
					queueInt->setQueueInternal(anonQueue);
				}
				queues.push_back(queue);
			}
		}

		std::deque<SmartPtrExchange> exchanges;
		std::deque<SmartPtrBinding> bindings;
		objs = _integrationAppContext->getIntegrationObjects(Exchange::IIDOF());
		CAF_CM_LOG_DEBUG_VA1("Declaring %d exchanges", objs->size());
		if (objs->size()) {
			for (TSmartConstIterator<IIntegrationAppContext::CObjectCollection> obj(*objs);
					obj;
					obj++) {
				SmartPtrExchange exchange;
				exchange.QueryInterface(*obj);
				_admin->declareExchange(exchange);
				exchanges.push_back(exchange);

				SmartPtrExchangeInternal exchangeInternal;
				exchangeInternal.QueryInterface(exchange);
				std::deque<SmartPtrBinding> embeddedBindings = exchangeInternal->getEmbeddedBindings();

				bindings.insert(
						bindings.end(),
						embeddedBindings.begin(),
						embeddedBindings.end());
			}
		}

		objs = _integrationAppContext->getIntegrationObjects(Binding::IIDOF());
		if (objs->size()) {
			for (TSmartConstIterator<IIntegrationAppContext::CObjectCollection> obj(*objs);
					obj;
					obj++) {
				SmartPtrBinding binding;
				binding.QueryInterface(*obj);
				bindings.push_back(binding);
			}
		}

		CAF_CM_LOG_DEBUG_VA1("Declaring %d bindings", bindings.size());
		for (TSmartConstIterator<std::deque<SmartPtrBinding> > binding(bindings);
				binding;
				binding++) {
			SmartPtrIIntegrationObject obj =
					_integrationAppContext->getIntegrationObject(binding->getQueue());

			// resolve the queue id to a queue name and replace the binding object
			SmartPtrQueue queue;
			queue.QueryInterface(obj);
			SmartPtrBindingInternal bindingInternal;
			bindingInternal.QueryInterface(*binding);
			bindingInternal->setBindingInternal(
					createBinding(
							queue->getName(),
							binding->getExchange(),
							binding->getRoutingKey()));

			// declare the binding
			_admin->declareBinding(*binding);
		}
	}
	CAF_CM_CATCH_ALL;
	if (CAF_CM_ISEXCEPTION) {
		_admin->term();
		_admin = NULL;
	}
	CAF_CM_THROWEXCEPTION;
	_isRunning = true;
}

void RabbitAdminInstance::stop(const uint32 timeoutMs) {
	if (_isRunning) {
		_isRunning = false;
		_admin->term();
	}
	_admin = NULL;
}

bool RabbitAdminInstance::isRunning() const {
	return _isRunning;
}

void RabbitAdminInstance::declareExchange(SmartPtrExchange exchange) {
	CAF_CM_FUNCNAME_VALIDATE("declareExchange");
	CAF_CM_VALIDATE_BOOL(isRunning());
	_admin->declareExchange(exchange);
}

bool RabbitAdminInstance::deleteExchange(const std::string& exchange) {
	CAF_CM_FUNCNAME_VALIDATE("deleteExchange");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _admin->deleteExchange(exchange);
}

SmartPtrQueue RabbitAdminInstance::declareQueue() {
	CAF_CM_FUNCNAME_VALIDATE("declareQueue");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _admin->declareQueue();
}

void RabbitAdminInstance::declareQueue(SmartPtrQueue queue) {
	CAF_CM_FUNCNAME_VALIDATE("declareQueue");
	CAF_CM_VALIDATE_BOOL(isRunning());
	_admin->declareQueue(queue);
}

bool RabbitAdminInstance::deleteQueue(const std::string& queue) {
	CAF_CM_FUNCNAME_VALIDATE("deleteQueue");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _admin->deleteQueue(queue);
}

void RabbitAdminInstance::deleteQueue(
		const std::string& queue,
		const bool unused,
		const bool empty) {
	CAF_CM_FUNCNAME_VALIDATE("deleteQueue");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _admin->deleteQueue(queue, unused, empty);
}

void RabbitAdminInstance::purgeQueue(const std::string& queue) {
	CAF_CM_FUNCNAME_VALIDATE("purgeQueue");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _admin->purgeQueue(queue);
}

void RabbitAdminInstance::declareBinding(SmartPtrBinding binding) {
	CAF_CM_FUNCNAME_VALIDATE("declareBinding");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _admin->declareBinding(binding);
}

void RabbitAdminInstance::removeBinding(SmartPtrBinding binding) {
	CAF_CM_FUNCNAME_VALIDATE("removeBinding");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _admin->removeBinding(binding);
}
