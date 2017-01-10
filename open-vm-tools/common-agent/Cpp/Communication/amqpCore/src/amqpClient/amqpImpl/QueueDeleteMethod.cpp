/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/QueueDeleteMethod.h"

using namespace Caf::AmqpClient;

QueueDeleteMethod::QueueDeleteMethod() :
	_isInitialized(false),
	_ifUnused(false),
	_ifEmpty(false),
	CAF_CM_INIT("QueueDeleteMethod") {
}

QueueDeleteMethod::~QueueDeleteMethod() {
}

void QueueDeleteMethod::init(
	const std::string& queue,
	const bool ifUnused,
	const bool ifEmpty) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_queue = queue;
	_ifUnused = ifUnused;
	_ifEmpty = ifEmpty;
	_isInitialized = true;
}

std::string QueueDeleteMethod::getMethodName() const {
	return "queue.delete";
}

AMQPStatus QueueDeleteMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_QueueDelete(
			channel,
			_queue,
			_ifUnused,
			_ifEmpty,
			false);
}
