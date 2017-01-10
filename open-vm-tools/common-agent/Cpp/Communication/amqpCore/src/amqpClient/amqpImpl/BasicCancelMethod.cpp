/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicCancelMethod.h"

using namespace Caf::AmqpClient;

BasicCancelMethod::BasicCancelMethod() :
	_isInitialized(false),
	CAF_CM_INIT("BasicCancelMethod") {
}

BasicCancelMethod::~BasicCancelMethod() {
}

void BasicCancelMethod::init(
	const std::string& consumerTag) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_consumerTag = consumerTag;
	_isInitialized = true;
}

std::string BasicCancelMethod::getMethodName() const {
	return "basic.cancel";
}

AMQPStatus BasicCancelMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_BasicCancel(channel, _consumerTag, false);
}
