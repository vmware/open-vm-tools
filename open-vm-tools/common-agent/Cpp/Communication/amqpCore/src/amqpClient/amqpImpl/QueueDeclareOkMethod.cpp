/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "QueueDeclareOkMethod.h"

using namespace Caf::AmqpClient;

QueueDeclareOkMethod::QueueDeclareOkMethod() :
	_messageCount(0),
	_consumerCount(0),
	CAF_CM_INIT("QueueDeclareOkMethod") {
}

QueueDeclareOkMethod::~QueueDeclareOkMethod() {
}

void QueueDeclareOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_QUEUE_DECLARE_OK_METHOD == method->id);
	const amqp_queue_declare_ok_t * const decoded =
			reinterpret_cast<const amqp_queue_declare_ok_t * const>(method->decoded);
	_queueName = AMQUtil::amqpBytesToString(&decoded->queue);
	_messageCount = decoded->message_count;
	_consumerCount = decoded->consumer_count;
}

std::string QueueDeclareOkMethod::getQueueName() {
	return _queueName;
}

uint32 QueueDeclareOkMethod::getMessageCount() {
	return _messageCount;
}

uint32 QueueDeclareOkMethod::getConsumerCount() {
	return _consumerCount;
}
