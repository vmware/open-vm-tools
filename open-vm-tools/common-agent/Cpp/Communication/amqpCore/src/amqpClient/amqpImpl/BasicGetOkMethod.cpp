/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "BasicGetOkMethod.h"

using namespace Caf::AmqpClient;

BasicGetOkMethod::BasicGetOkMethod() :
	_deliveryTag(0),
	_messageCount(0),
	_redelivered(false),
	CAF_CM_INIT("BasicGetOkMethod") {
}

BasicGetOkMethod::~BasicGetOkMethod() {
}

void BasicGetOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_BASIC_GET_OK_METHOD == method->id);
	const amqp_basic_get_ok_t * const decoded =
			reinterpret_cast<const amqp_basic_get_ok_t * const>(method->decoded);
	_deliveryTag = decoded->delivery_tag;
	_exchange = AMQUtil::amqpBytesToString(&decoded->exchange);
	_messageCount = decoded->message_count;
	_redelivered = decoded->redelivered;
	_routingKey = AMQUtil::amqpBytesToString(&decoded->routing_key);
}

uint64 BasicGetOkMethod::getDeliveryTag() {
	return _deliveryTag;
}

std::string BasicGetOkMethod::getExchange() {
	return _exchange;
}

uint32 BasicGetOkMethod::getMessageCount() {
	return _messageCount;
}

bool BasicGetOkMethod::getRedelivered() {
	return _redelivered;
}

std::string BasicGetOkMethod::getRoutingKey() {
	return _routingKey;
}
