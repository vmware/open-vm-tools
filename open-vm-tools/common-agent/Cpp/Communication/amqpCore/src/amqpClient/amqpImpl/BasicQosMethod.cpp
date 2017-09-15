/*
 *  Created on: May 22, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicQosMethod.h"

using namespace Caf::AmqpClient;

BasicQosMethod::BasicQosMethod() :
	_isInitialized(false),
	_prefetchSize(0),
	_prefetchCount(0),
	_global(false),
	CAF_CM_INIT("BasicQosMethod") {
}

BasicQosMethod::~BasicQosMethod() {
}

void BasicQosMethod::init(
	const uint32 prefetchSize,
	const uint16 prefetchCount,
	const bool global) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_prefetchSize = prefetchSize;
	_prefetchCount = prefetchCount;
	_global = global;
	_isInitialized = true;
}

std::string BasicQosMethod::getMethodName() const {
	return "basic.qos";
}

AMQPStatus BasicQosMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_BasicQos(
			channel,
			_prefetchSize,
			_prefetchCount,
			_global);
}
