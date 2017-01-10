/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/amqpClient.h"
#include "amqpClient/amqpImpl/QueueUnbindMethod.h"

using namespace Caf::AmqpClient;

QueueUnbindMethod::QueueUnbindMethod() :
	_isInitialized(false),
	CAF_CM_INIT("QueueUnbindMethod") {
}

QueueUnbindMethod::~QueueUnbindMethod() {
}

void QueueUnbindMethod::init(
	const std::string& queue,
	const std::string& exchange,
	const std::string& routingKey,
	const SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_queue = queue;
	_exchange = exchange;
	_routingKey = routingKey;
	_isInitialized = true;
}

std::string QueueUnbindMethod::getMethodName() const {
	return "queue.unbind";
}

AMQPStatus QueueUnbindMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_QueueUnbind(
			channel,
			_queue,
			_exchange,
			_routingKey,
			NULL);
}
