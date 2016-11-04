/*
 *  Created on: May 17, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/ChannelCloseOkMethod.h"

using namespace Caf::AmqpClient;

ChannelCloseOkMethod::ChannelCloseOkMethod() :
	_isInitialized(false),
	CAF_CM_INIT("ChannelCloseOkMethod") {
}

ChannelCloseOkMethod::~ChannelCloseOkMethod() {
}

void ChannelCloseOkMethod::init() {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_isInitialized = true;
}

std::string ChannelCloseOkMethod::getMethodName() const {
	return "channel.close-ok";
}

AMQPStatus ChannelCloseOkMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpChannel::AMQP_ChannelCloseOk(channel);
}
