/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "BasicCancelOkMethod.h"

using namespace Caf::AmqpClient;

BasicCancelOkMethod::BasicCancelOkMethod() :
	CAF_CM_INIT("BasicCancelOkMethod") {
}

BasicCancelOkMethod::~BasicCancelOkMethod() {
}

void BasicCancelOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_BASIC_CANCEL_OK_METHOD == method->id);
	const amqp_basic_cancel_ok_t * const decoded =
			reinterpret_cast<const amqp_basic_cancel_ok_t * const>(method->decoded);
	_consumerTag = AMQUtil::amqpBytesToString(&decoded->consumer_tag);
}

std::string BasicCancelOkMethod::getConsumerTag() {
	return _consumerTag;
}
