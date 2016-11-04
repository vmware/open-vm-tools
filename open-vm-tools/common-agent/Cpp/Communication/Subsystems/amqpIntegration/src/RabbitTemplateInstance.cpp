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
#include "Integration/IIntMessage.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpCore/AmqpTemplate.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "Common/IAppConfig.h"
#include "RabbitTemplateInstance.h"

using namespace Caf::AmqpIntegration;

RabbitTemplateInstance::RabbitTemplateInstance() :
	_isWired(false),
	CAF_CM_INIT_LOG("RabbitTemplateInstance") {
}

RabbitTemplateInstance::~RabbitTemplateInstance() {
}

void RabbitTemplateInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	_id = configSection->findOptionalAttribute("id");
	if (!_id.length()) {
		_id = CStringUtils::createRandomUuid();
	}
	_configSection = configSection;
}

std::string RabbitTemplateInstance::getId() const {
	return _id;
}

void RabbitTemplateInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_VALIDATE_INTERFACE(_configSection);

	const std::string connectionFactoryId = _configSection->findRequiredAttribute("connection-factory");
	SmartPtrIBean factoryBean = appContext->getBean(connectionFactoryId);
	SmartPtrConnectionFactory connectionFactory;
	connectionFactory.QueryInterface(factoryBean, true);

	SmartPtrIAppConfig appConfig = getAppConfig();

	_template.CreateInstance();
	std::string param = _configSection->findOptionalAttribute("exchange");
	if (param.length()) {
		CAF_CM_LOG_DEBUG_VA1("Setting exchange='%s'", param.c_str());
		_template->setExchange(appConfig->resolveValue(param));
	}
	param = _configSection->findOptionalAttribute("queue");
	if (param.length()) {
		CAF_CM_LOG_DEBUG_VA1("Setting queue='%s'", param.c_str());
		_template->setQueue(appConfig->resolveValue(param));
	}
	param = _configSection->findOptionalAttribute("routing-key");
	if (param.length()) {
		CAF_CM_LOG_DEBUG_VA1("Setting routing-key='%s'", param.c_str());
		_template->setRoutingKey(appConfig->resolveValue(param));
	}
	param = _configSection->findOptionalAttribute("reply_timeout");
	if (param.length()) {
		param = appConfig->resolveValue(param);
		uint32 timeout = CStringConv::fromString<uint32>(param);
		CAF_CM_LOG_DEBUG_VA1("Setting reply_timeout=%d", timeout);
		_template->setReplyTimeout(timeout);
	}

	_template->init(connectionFactory);
	_isWired = true;
	_configSection = NULL;
}

void RabbitTemplateInstance::start(const uint32 timeoutMs) {
}

void RabbitTemplateInstance::stop(const uint32 timeoutMs) {
	if (_isWired) {
		_template->term();
	}
	_template = NULL;
	_isWired = false;
}

bool RabbitTemplateInstance::isRunning() const {
	return (_isWired && !_template.IsNull());
}

void RabbitTemplateInstance::send(
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper headerMapper) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_VALIDATE_BOOL(isRunning());
	_template->send(message, headerMapper);
}

void RabbitTemplateInstance::send(
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper headerMapper) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_VALIDATE_BOOL(isRunning());
	_template->send(routingKey, message, headerMapper);
}

void RabbitTemplateInstance::send(
		const std::string& exchange,
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper headerMapper) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_VALIDATE_BOOL(isRunning());
	_template->send(exchange, routingKey, message, headerMapper);
}

SmartPtrIIntMessage RabbitTemplateInstance::receive(
		SmartPtrAmqpHeaderMapper headerMapper) {
	CAF_CM_FUNCNAME_VALIDATE("receive");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _template->receive(headerMapper);
}

SmartPtrIIntMessage RabbitTemplateInstance::receive(
		const std::string& queueName,
		SmartPtrAmqpHeaderMapper headerMapper) {
	CAF_CM_FUNCNAME_VALIDATE("receive");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _template->receive(queueName, headerMapper);
}

SmartPtrIIntMessage RabbitTemplateInstance::sendAndReceive(
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper requestHeaderMapper,
		SmartPtrAmqpHeaderMapper responseHeaderMapper) {
	CAF_CM_FUNCNAME_VALIDATE("sendAndReceive");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _template->sendAndReceive(
			message,
			requestHeaderMapper,
			responseHeaderMapper);
}

SmartPtrIIntMessage RabbitTemplateInstance::sendAndReceive(
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper requestHeaderMapper,
		SmartPtrAmqpHeaderMapper responseHeaderMapper) {
	CAF_CM_FUNCNAME_VALIDATE("sendAndReceive");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _template->sendAndReceive(
			routingKey,
			message,
			requestHeaderMapper,
			responseHeaderMapper);
}

SmartPtrIIntMessage RabbitTemplateInstance::sendAndReceive(
		const std::string& exchange,
		const std::string& routingKey,
		SmartPtrIIntMessage message,
		SmartPtrAmqpHeaderMapper requestHeaderMapper,
		SmartPtrAmqpHeaderMapper responseHeaderMapper) {
	CAF_CM_FUNCNAME_VALIDATE("sendAndReceive");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _template->sendAndReceive(
			exchange,
			routingKey,
			message,
			requestHeaderMapper,
			responseHeaderMapper);
}

gpointer RabbitTemplateInstance::execute(SmartPtrExecutor executor, gpointer data) {
	CAF_CM_FUNCNAME_VALIDATE("execute");
	CAF_CM_VALIDATE_BOOL(isRunning());
	return _template->execute(executor, data);
}
