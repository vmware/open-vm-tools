/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "QueueDeleteOkMethod.h"

using namespace Caf::AmqpClient;

QueueDeleteOkMethod::QueueDeleteOkMethod() :
	_messageCount(0),
	CAF_CM_INIT("QueueDeleteOkMethod") {
}

QueueDeleteOkMethod::~QueueDeleteOkMethod() {
}

void QueueDeleteOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_QUEUE_DELETE_OK_METHOD == method->id);
	const amqp_queue_delete_ok_t * const decoded =
			reinterpret_cast<const amqp_queue_delete_ok_t * const>(method->decoded);
	_messageCount = decoded->message_count;
}

uint32 QueueDeleteOkMethod::getMessageCount() {
	return _messageCount;
}
