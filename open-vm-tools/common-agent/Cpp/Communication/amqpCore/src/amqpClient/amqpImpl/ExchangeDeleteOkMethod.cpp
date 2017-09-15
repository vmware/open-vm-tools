/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "ExchangeDeleteOkMethod.h"

using namespace Caf::AmqpClient;

ExchangeDeleteOkMethod::ExchangeDeleteOkMethod() :
	CAF_CM_INIT("ExchangeDeleteOkMethod") {
}

ExchangeDeleteOkMethod::~ExchangeDeleteOkMethod() {
}

void ExchangeDeleteOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_EXCHANGE_DELETE_OK_METHOD == method->id);
}
