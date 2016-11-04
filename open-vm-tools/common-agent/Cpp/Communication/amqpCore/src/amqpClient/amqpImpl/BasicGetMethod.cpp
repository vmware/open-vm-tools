/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicGetMethod.h"

using namespace Caf::AmqpClient;

BasicGetMethod::BasicGetMethod() :
	_isInitialized(false),
	_noAck(false),
	CAF_CM_INIT("BasicGetMethod") {
}

BasicGetMethod::~BasicGetMethod() {
}

void BasicGetMethod::init(
	const std::string& queue,
	const bool noAck) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_queue = queue;
	_noAck = noAck;
	_isInitialized = true;
}

std::string BasicGetMethod::getMethodName() const {
	return "basic.get";
}

AMQPStatus BasicGetMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_BasicGet(channel, _queue, _noAck);
}
