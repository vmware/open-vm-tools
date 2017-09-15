/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicRecoverMethod.h"

using namespace Caf::AmqpClient;

BasicRecoverMethod::BasicRecoverMethod() :
	_isInitialized(false),
	_requeue(false),
	CAF_CM_INIT("BasicRecoverMethod") {
}

BasicRecoverMethod::~BasicRecoverMethod() {
}

void BasicRecoverMethod::init(
	const bool requeue) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_requeue = requeue;
	_isInitialized = true;
}

std::string BasicRecoverMethod::getMethodName() const {
	return "basic.recover";
}

AMQPStatus BasicRecoverMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_BasicRecover(
			channel,
			_requeue);
}
