/*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/amqpClient.h"
#include "amqpClient/amqpImpl/QueueDeclareMethod.h"

using namespace Caf::AmqpClient;

QueueDeclareMethod::QueueDeclareMethod() :
	_isInitialized(false),
	_passive(false),
	_durable(false),
	_exclusive(false),
	_autoDelete(false),
	_noWait(false),
	CAF_CM_INIT("QueueDeclareMethod") {
}

QueueDeclareMethod::~QueueDeclareMethod() {
}

void QueueDeclareMethod::init() {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	init(
		"",
		false,
		true,
		true,
		SmartPtrTable());
}

void QueueDeclareMethod::init(
	const std::string& queue,
	bool durable,
	bool exclusive,
	bool autoDelete,
	const SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_queue = queue;
	_passive = false;
	_durable = durable;
	_exclusive = exclusive;
	_autoDelete = autoDelete;
	_noWait = false;
	_isInitialized = true;
}

void QueueDeclareMethod::initPassive(
		const std::string& queue) {
	CAF_CM_FUNCNAME_VALIDATE("initPassive");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_queue = queue;
	_passive = true;
	_noWait = false;
	_isInitialized = true;
}

std::string QueueDeclareMethod::getMethodName() const {
	return "queue.declare";
}

AMQPStatus QueueDeclareMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_QueueDeclare(
			channel,
			_queue,
			_passive,
			_durable,
			_exclusive,
			_autoDelete,
			_noWait,
			NULL);
}
