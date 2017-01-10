/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CVariant.h"
#include "amqpClient/api/AmqpMethods.h"
#include "amqpClient/api/Channel.h"
#include "amqpCore/Binding.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpCore/Exchange.h"
#include "amqpCore/Queue.h"
#include "amqpCore/RabbitAdmin.h"

using namespace Caf::AmqpIntegration;

RabbitAdmin::RabbitAdmin() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("RabbitAdmin") {
}

RabbitAdmin::~RabbitAdmin() {
	CAF_CM_FUNCNAME("~RabbitAdmin");
	try {
		if (_rabbitTemplate) {
			_rabbitTemplate->term();
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

void RabbitAdmin::init(SmartPtrConnectionFactory connectionFactory) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(connectionFactory);
	_rabbitTemplate.CreateInstance();
	_rabbitTemplate->init(connectionFactory);
	_isInitialized = true;
}

void RabbitAdmin::term() {
	CAF_CM_FUNCNAME_VALIDATE("term");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_rabbitTemplate->term();
	_rabbitTemplate = NULL;
}

void RabbitAdmin::declareExchange(SmartPtrExchange exchange) {
	CAF_CM_FUNCNAME_VALIDATE("declareExchange");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(exchange);
	SmartPtrDeclareExchangeExecutor executor;
	executor.CreateInstance();
	_rabbitTemplate->execute(executor, exchange.GetNonAddRefedInterface());
}

bool RabbitAdmin::deleteExchange(const std::string& exchange) {
	CAF_CM_FUNCNAME_VALIDATE("deleteExchange");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(exchange);
	SmartPtrDeleteExchangeExecutor executor;
	executor.CreateInstance();
	gpointer result = _rabbitTemplate->execute(
			executor,
			const_cast<char*>(exchange.c_str()));
	return result != NULL;
}

SmartPtrQueue RabbitAdmin::declareQueue() {
	CAF_CM_FUNCNAME_VALIDATE("declareQueue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	SmartPtrDeclareQueueExecutor executor;
	executor.CreateInstance();
	gpointer result = _rabbitTemplate->execute(executor, NULL);
	CAF_CM_VALIDATE_PTR(result);
	Queue *queuePtr = reinterpret_cast<Queue*>(result);
	SmartPtrQueue queue(queuePtr);
	queuePtr->Release();
	return queue;
}

void RabbitAdmin::declareQueue(SmartPtrQueue queue) {
	CAF_CM_FUNCNAME_VALIDATE("declareQueue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	SmartPtrDeclareQueueExecutor executor;
	executor.CreateInstance();
	_rabbitTemplate->execute(executor, queue.GetNonAddRefedInterface());
}

bool RabbitAdmin::deleteQueue(const std::string& queue) {
	CAF_CM_FUNCNAME_VALIDATE("deleteQueue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);
	SmartPtrDeleteQueueExecutor executor;
	executor.CreateInstance();
	gpointer result = _rabbitTemplate->execute(
			executor,
			const_cast<char*>(queue.c_str()));
	return result != NULL;
}

void RabbitAdmin::deleteQueue(
		const std::string& queue,
		const bool unused,
		const bool empty) {
	CAF_CM_FUNCNAME_VALIDATE("deleteQueue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);
	SmartPtrCVariant args[3];
	args[0] = CVariant::createString(queue);
	args[1] = CVariant::createBool(unused);
	args[2] = CVariant::createBool(empty);
	SmartPtrDeleteQueueExExecutor executor;
	executor.CreateInstance();
	_rabbitTemplate->execute(executor, args);
}

void RabbitAdmin::purgeQueue(const std::string& queue) {
	CAF_CM_FUNCNAME_VALIDATE("purgeQueue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(queue);
	SmartPtrPurgeQueueExecutor executor;
	executor.CreateInstance();
	_rabbitTemplate->execute(
			executor,
			const_cast<char*>(queue.c_str()));
}

void RabbitAdmin::declareBinding(SmartPtrBinding binding) {
	CAF_CM_FUNCNAME_VALIDATE("declareBinding");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(binding);
	SmartPtrDeclareBindingExecutor executor;
	executor.CreateInstance();
	_rabbitTemplate->execute(
			executor,
			binding.GetNonAddRefedInterface());
}

void RabbitAdmin::removeBinding(SmartPtrBinding binding) {
	CAF_CM_FUNCNAME_VALIDATE("removeBinding");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(binding);
	SmartPtrRemoveBindingExecutor executor;
	executor.CreateInstance();
	_rabbitTemplate->execute(
			executor,
			binding.GetNonAddRefedInterface());
}

gpointer RabbitAdmin::DeclareExchangeExecutor::execute(
		AmqpClient::SmartPtrChannel channel,
		gpointer data) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RabbitAdmin::DeclareExchangeExecutor", "execute");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_PTR(data);
	Exchange *exchange = reinterpret_cast<Exchange*>(data);
	CAF_CM_LOG_DEBUG_VA3(
			"declaring exchange '%s' type '%s' durable=%d",
			exchange->getName().c_str(),
			exchange->getType().c_str(),
			exchange->isDurable());
	channel->exchangeDeclare(
			exchange->getName(),
			exchange->getType(),
			exchange->isDurable());
	return NULL;
}

gpointer RabbitAdmin::DeleteExchangeExecutor::execute(
		AmqpClient::SmartPtrChannel channel,
		gpointer data) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RabbitAdmin::DeleteExchangeExecutor", "execute");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_PTR(data);
	const char *exchange = reinterpret_cast<const char*>(data);
	CAF_CM_LOG_DEBUG_VA1(
			"deleting exchange '%s'",
			exchange);
	gpointer result = NULL;
	try {
		channel->exchangeDelete(exchange, false);
		result = reinterpret_cast<gpointer>(0x01);
	} catch (AmqpClient::AmqpExceptions::ChannelClosedByServerException *ex) {
			ex->Release();
	}
	return NULL;
}

gpointer RabbitAdmin::DeclareQueueExecutor::execute(
		AmqpClient::SmartPtrChannel channel,
		gpointer data) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RabbitAdmin::DeclareQueueExecutor", "execute");
	CAF_CM_VALIDATE_SMARTPTR(channel);

	gpointer result = NULL;
	if (data) {
		Queue *queue = reinterpret_cast<Queue*>(data);
		const std::string queueName = queue->getName();
		if (queueName.find("amq.", 0) == 0) {
			CAF_CM_LOG_ERROR_VA1(
					"Cannot declare queue '%s' because it's name begins with 'amq.'",
					queueName.c_str());
		} else {
			CAF_CM_LOG_DEBUG_VA4(
					"declaring queue '%s' [durable=%d][exclusive=%d][autoDelete=%d]",
					queueName.c_str(),
					queue->isDurable(),
					queue->isExclusive(),
					queue->isAutoDelete());

			channel->queueDeclare(
					queue->getName(),
					queue->isDurable(),
					queue->isExclusive(),
					queue->isAutoDelete());
		}
	} else {
		AmqpClient::AmqpMethods::Queue::SmartPtrDeclareOk declareOk =
				channel->queueDeclare();
		SmartPtrQueue queue = createQueue(
				declareOk->getQueueName(),
				false,
				true,
				true);
		result = queue.GetAddRefedInterface();
	}
	return result;
}

gpointer RabbitAdmin::DeleteQueueExecutor::execute(
		AmqpClient::SmartPtrChannel channel,
		gpointer data) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RabbitAdmin::DeleteQueueExecutor", "execute");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_PTR(data);
	const char *queue = reinterpret_cast<const char*>(data);
	CAF_CM_LOG_DEBUG_VA1(
			"deleting queue '%s'",
			queue);
	gpointer result = NULL;
	try {
		channel->queueDelete(queue, false, false);
		result = reinterpret_cast<gpointer>(0x01);
	} catch (AmqpClient::AmqpExceptions::ChannelClosedByServerException *ex) {
		ex->Release();
	}
	return result;
}

gpointer RabbitAdmin::DeleteQueueExExecutor::execute(
		AmqpClient::SmartPtrChannel channel,
		gpointer data) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RabbitAdmin::DeleteQueueExExecutor", "execute");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_PTR(data);
	SmartPtrCVariant *args = reinterpret_cast<SmartPtrCVariant*>(data);
	CAF_CM_LOG_DEBUG_VA3(
			"deleting queue '%s' [unused=%d][empty=%d]",
			g_variant_get_string(args[0]->get(), NULL),
			g_variant_get_boolean(args[1]->get()),
			g_variant_get_boolean(args[2]->get()));
	channel->queueDelete(
			g_variant_get_string(args[0]->get(), NULL),
			g_variant_get_boolean(args[1]->get()),
			g_variant_get_boolean(args[2]->get()));
	return NULL;
}

gpointer RabbitAdmin::PurgeQueueExecutor::execute(
		AmqpClient::SmartPtrChannel channel,
		gpointer data) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RabbitAdmin::PurgeQueueExecutor", "execute");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_PTR(data);
	const char *queue = reinterpret_cast<const char*>(data);
	CAF_CM_LOG_DEBUG_VA1(
			"purging queue '%s'",
			queue);
	channel->queuePurge(queue);
	return NULL;
}

gpointer RabbitAdmin::DeclareBindingExecutor::execute(
		AmqpClient::SmartPtrChannel channel,
		gpointer data) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RabbitAdmin::DeclareBindingExecutor", "execute");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_PTR(data);
	Binding *binding = reinterpret_cast<Binding*>(data);
	channel->queueBind(
			binding->getQueue(),
			binding->getExchange(),
			binding->getRoutingKey());
	return NULL;
}

gpointer RabbitAdmin::RemoveBindingExecutor::execute(
		AmqpClient::SmartPtrChannel channel,
		gpointer data) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RabbitAdmin::RemoveBindingExecutor", "execute");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_PTR(data);
	Binding *binding = reinterpret_cast<Binding*>(data);
	channel->queueUnbind(
			binding->getQueue(),
			binding->getExchange(),
			binding->getRoutingKey());
	return NULL;
}
