/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "BasicConsumeOkMethod.h"

using namespace Caf::AmqpClient;

BasicConsumeOkMethod::BasicConsumeOkMethod() :
	CAF_CM_INIT("BasicConsumeOkMethod") {
}

BasicConsumeOkMethod::~BasicConsumeOkMethod() {
}

void BasicConsumeOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_BASIC_CONSUME_OK_METHOD == method->id);
	const amqp_basic_consume_ok_t * const decoded =
			reinterpret_cast<const amqp_basic_consume_ok_t * const>(method->decoded);
	_consumerTag = AMQUtil::amqpBytesToString(&decoded->consumer_tag);
}

std::string BasicConsumeOkMethod::getConsumerTag() {
	return _consumerTag;
}
