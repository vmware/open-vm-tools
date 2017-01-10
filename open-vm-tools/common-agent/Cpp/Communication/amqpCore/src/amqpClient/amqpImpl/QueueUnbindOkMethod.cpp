/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "QueueUnbindOkMethod.h"

using namespace Caf::AmqpClient;

QueueUnbindOkMethod::QueueUnbindOkMethod() :
	CAF_CM_INIT("QueueUnbindOkMethod") {
}

QueueUnbindOkMethod::~QueueUnbindOkMethod() {
}

void QueueUnbindOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_QUEUE_UNBIND_OK_METHOD == method->id);
}
