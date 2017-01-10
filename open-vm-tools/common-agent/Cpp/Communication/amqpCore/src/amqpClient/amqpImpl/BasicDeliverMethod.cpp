/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "BasicDeliverMethod.h"

using namespace Caf::AmqpClient;

BasicDeliverMethod::BasicDeliverMethod() :
	_deliveryTag(0),
	_redelivered(false),
	CAF_CM_INIT("BasicDeliverMethod") {
}

BasicDeliverMethod::~BasicDeliverMethod() {
}

void BasicDeliverMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_BASIC_DELIVER_METHOD == method->id);
	const amqp_basic_deliver_t * const decoded =
			reinterpret_cast<const amqp_basic_deliver_t * const>(method->decoded);
	_consumerTag = AMQUtil::amqpBytesToString(&decoded->consumer_tag);
	_deliveryTag = decoded->delivery_tag;
	_exchange = AMQUtil::amqpBytesToString(&decoded->exchange);
	_redelivered = decoded->redelivered;
	_routingKey = AMQUtil::amqpBytesToString(&decoded->routing_key);
}

std::string BasicDeliverMethod::getConsumerTag() {
	return _consumerTag;
}

uint64 BasicDeliverMethod::getDeliveryTag() {
	return _deliveryTag;
}

std::string BasicDeliverMethod::getExchange() {
	return _exchange;
}

bool BasicDeliverMethod::getRedelivered() {
	return _redelivered;
}

std::string BasicDeliverMethod::getRoutingKey() {
	return _routingKey;
}
