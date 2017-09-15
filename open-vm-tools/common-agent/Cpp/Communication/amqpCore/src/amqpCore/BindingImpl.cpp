/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpCore/Binding.h"
#include "amqpCore/BindingImpl.h"

using namespace Caf::AmqpIntegration;

BindingImpl::BindingImpl() {
}

BindingImpl::~BindingImpl() {
}

void BindingImpl::init(
		const std::string queue,
		const std::string exchange,
		const std::string routingKey) {
	_queue = queue;
	_exchange = exchange;
	_routingKey = routingKey;
}

std::string BindingImpl::getQueue() const {
	return _queue;
}

std::string BindingImpl::getExchange() const {
	return _exchange;
}

std::string BindingImpl::getRoutingKey() const {
	return _routingKey;
}

SmartPtrBinding AMQPINTEGRATIONCORE_LINKAGE Caf::AmqpIntegration::createBinding(
		const std::string queue,
		const std::string exchange,
		const std::string routingKey) {
	SmartPtrBindingImpl binding;
	binding.CreateInstance();
	binding->init(queue, exchange, routingKey);
	return binding;
}
