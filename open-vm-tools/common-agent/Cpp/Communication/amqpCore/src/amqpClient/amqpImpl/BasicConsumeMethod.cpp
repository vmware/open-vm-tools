/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/amqpClient.h"
#include "amqpClient/amqpImpl/BasicConsumeMethod.h"

using namespace Caf::AmqpClient;

BasicConsumeMethod::BasicConsumeMethod() :
	_isInitialized(false),
	_noLocal(false),
	_noAck(false),
	_exclusive(false),
	CAF_CM_INIT("BasicConsumeMethod") {
}

BasicConsumeMethod::~BasicConsumeMethod() {
}

void BasicConsumeMethod::init(
		const std::string& queue,
		const std::string& consumerTag,
		const bool noLocal,
		const bool noAck,
		const bool exclusive,
		const SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_queue = queue;
	_consumerTag = consumerTag;
	_noLocal = noLocal;
	_noAck = noAck;
	_exclusive = exclusive;
	_isInitialized = true;
}

std::string BasicConsumeMethod::getMethodName() const {
	return "basic.consume";
}

AMQPStatus BasicConsumeMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_BasicConsume(
			channel,
			_queue,
			_consumerTag,
			_noLocal,
			_noAck,
			_exclusive,
			false,
			NULL);
}
