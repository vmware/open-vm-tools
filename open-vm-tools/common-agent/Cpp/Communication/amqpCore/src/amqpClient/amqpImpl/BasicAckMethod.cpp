/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicAckMethod.h"

using namespace Caf::AmqpClient;

BasicAckMethod::BasicAckMethod() :
	_isInitialized(false),
	_deliveryTag(0),
	_ackMultiple(false),
	CAF_CM_INIT("BasicAckMethod") {
}

BasicAckMethod::~BasicAckMethod() {
}

void BasicAckMethod::init(
	const uint64 deliveryTag,
	const bool ackMultiple) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_deliveryTag = deliveryTag;
	_ackMultiple = ackMultiple;
	_isInitialized = true;
}

std::string BasicAckMethod::getMethodName() const {
	return "basic.ack";
}

AMQPStatus BasicAckMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_BasicAck(
			channel,
			_deliveryTag,
			_ackMultiple);
}
