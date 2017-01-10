/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "amqpCore/Binding.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#include "ExchangeInstance.h"
#include "BindingInstance.h"

using namespace Caf::AmqpIntegration;

ExchangeInstance::ExchangeInstance() :
	CAF_CM_INIT_LOG("ExchangeInstance") {
}

ExchangeInstance::~ExchangeInstance() {
}

void ExchangeInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME("initialize");

	_id = CStringUtils::createRandomUuid();

	SmartPtrIAppConfig appConfig = getAppConfig();

	const std::string exchange = appConfig->resolveValue(configSection->findRequiredAttribute("name"));
	const std::string durableStr = configSection->findOptionalAttribute("durable");
	if (durableStr.length()) {
		if ((durableStr != "true") && (durableStr != "false")) {
			CAF_CM_EXCEPTIONEX_VA3(
					InvalidArgumentException,
					0,
					"Invalid 'durable' value (%s) for %s '%s'. "
					"Value must be either 'true' or 'false'.",
					durableStr.c_str(),
					configSection->getName().c_str(),
					exchange.c_str());
		}
	}
	const bool durable = !durableStr.length() || (durableStr == "true");

	if (configSection->getName() == "rabbit-direct-exchange") {
		_exchange = createDirectExchange(exchange, durable);
	} else if (configSection->getName() == "rabbit-topic-exchange") {
		_exchange = createTopicExchange(exchange, durable);
	} else if (configSection->getName() == "rabbit-headers-exchange") {
		_exchange = createHeadersExchange(exchange, durable);
	} else if (configSection->getName() == "rabbit-fanout-exchange") {
		_exchange = createFanoutExchange(exchange, durable);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				InvalidArgumentException,
				0,
				"Invalid exchange type (%s)",
				configSection->getName().c_str());
	}

	SmartPtrIDocument bindingsSection = configSection->findOptionalChild("rabbit-bindings");
	if (bindingsSection) {
		IDocument::SmartPtrCChildCollection bindingSections =
				bindingsSection->getAllChildren();
		for (TSmartConstMapIterator<IDocument::CChildCollection> bindingSection(*bindingSections);
				bindingSection;
				bindingSection++) {
			if (bindingSection->getName() != "rabbit-binding") {
				CAF_CM_EXCEPTIONEX_VA2(
						InvalidArgumentException,
						0,
						"Invalid tag (%s) found in bindings section of "
						"exchange declaration (name=%s)",
						bindingSection->getName().c_str(),
						exchange.c_str());
			}
			const std::string queue = appConfig->resolveValue(bindingSection->findRequiredAttribute("queue"));
			const std::string key = appConfig->resolveValue(bindingSection->findRequiredAttribute("key"));
			CAF_CM_LOG_DEBUG_VA3(
					"Adding binding declaration [queue id=%s][exchange name=%s][key=%s]",
					queue.c_str(),
					exchange.c_str(),
					key.c_str());
			SmartPtrBindingInstance binding;
			binding.CreateInstance();
			binding->setBindingInternal(createBinding(queue, exchange, key));
			_bindings.push_back(binding);
		}
	}
}

std::string ExchangeInstance::getId() const {
	return _id;
}

std::string ExchangeInstance::getName() const {
	return _exchange->getName();
}

std::string ExchangeInstance::getType() const {
	return _exchange->getType();
}

bool ExchangeInstance::isDurable() const {
	return _exchange->isDurable();
}

std::deque<SmartPtrBinding> ExchangeInstance::getEmbeddedBindings() const {
	return _bindings;
}
