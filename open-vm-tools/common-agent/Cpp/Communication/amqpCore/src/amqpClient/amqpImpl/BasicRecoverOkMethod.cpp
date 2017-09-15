/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "BasicRecoverOkMethod.h"

using namespace Caf::AmqpClient;

BasicRecoverOkMethod::BasicRecoverOkMethod() :
	CAF_CM_INIT("BasicRecoverOkMethod") {
}

BasicRecoverOkMethod::~BasicRecoverOkMethod() {
}

void BasicRecoverOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_BASIC_RECOVER_OK_METHOD == method->id);
}
