/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/QueuePurgeMethod.h"

using namespace Caf::AmqpClient;

QueuePurgeMethod::QueuePurgeMethod() :
	_isInitialized(false),
	CAF_CM_INIT("QueuePurgeMethod") {
}

QueuePurgeMethod::~QueuePurgeMethod() {
}

void QueuePurgeMethod::init(
	const std::string& queue) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_queue = queue;
	_isInitialized = true;
}

std::string QueuePurgeMethod::getMethodName() const {
	return "queue.purge";
}

AMQPStatus QueuePurgeMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_QueuePurge(
			channel,
			_queue,
			false);
}
