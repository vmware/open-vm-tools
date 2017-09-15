/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "BasicReturnMethod.h"

using namespace Caf::AmqpClient;

BasicReturnMethod::BasicReturnMethod() :
	_replyCode(0),
	CAF_CM_INIT("BasicReturnMethod") {
}

BasicReturnMethod::~BasicReturnMethod() {
}

void BasicReturnMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_BASIC_RETURN_METHOD == method->id);
	const amqp_basic_return_t * const decoded =
			reinterpret_cast<const amqp_basic_return_t * const>(method->decoded);
	_replyCode = decoded->reply_code;
	_replyText = AMQUtil::amqpBytesToString(&decoded->reply_text);
	_exchange = AMQUtil::amqpBytesToString(&decoded->exchange);
	_routingKey = AMQUtil::amqpBytesToString(&decoded->routing_key);
}

uint16 BasicReturnMethod::getReplyCode() {
	return _replyCode;
}

std::string BasicReturnMethod::getReplyText() {
	return _replyText;
}

std::string BasicReturnMethod::getExchange() {
	return _exchange;
}

std::string BasicReturnMethod::getRoutingKey() {
	return _routingKey;
}
