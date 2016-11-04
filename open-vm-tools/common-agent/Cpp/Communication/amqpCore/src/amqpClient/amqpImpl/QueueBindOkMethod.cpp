/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "QueueBindOkMethod.h"

using namespace Caf::AmqpClient;

QueueBindOkMethod::QueueBindOkMethod() :
	CAF_CM_INIT("QueueBindOkMethod") {
}

QueueBindOkMethod::~QueueBindOkMethod() {
}

void QueueBindOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_QUEUE_BIND_OK_METHOD == method->id);
}
