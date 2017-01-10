/*
 *  Created on: Aug 1, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicRejectMethod.h"

using namespace Caf::AmqpClient;

BasicRejectMethod::BasicRejectMethod() :
	_isInitialized(false),
	_deliveryTag(0),
	_requeue(false),
	CAF_CM_INIT("BasicRejectMethod") {
}

BasicRejectMethod::~BasicRejectMethod() {
}

void BasicRejectMethod::init(
	const uint64 deliveryTag,
	const bool requeue) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_deliveryTag = deliveryTag;
	_requeue = requeue;
	_isInitialized = true;
}

std::string BasicRejectMethod::getMethodName() const {
	return "basic.Reject";
}

AMQPStatus BasicRejectMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
//	return AMQP_BasicReject(
//			channel,
//			_deliveryTag,
//			_requeue);
	return AMQP_ERROR_UNIMPLEMENTED;
}
