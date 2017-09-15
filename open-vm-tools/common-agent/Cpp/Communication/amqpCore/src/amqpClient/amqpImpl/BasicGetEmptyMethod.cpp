/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "BasicGetEmptyMethod.h"

using namespace Caf::AmqpClient;

BasicGetEmptyMethod::BasicGetEmptyMethod() :
	CAF_CM_INIT("BasicGetEmptyMethod") {
}

BasicGetEmptyMethod::~BasicGetEmptyMethod() {
}

void BasicGetEmptyMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_BASIC_GET_EMPTY_METHOD == method->id);
}
