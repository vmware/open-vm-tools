/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/amqpClient.h"
#include "amqpClient/amqpImpl/ExchangeDeclareMethod.h"

using namespace Caf::AmqpClient;

ExchangeDeclareMethod::ExchangeDeclareMethod() :
	_isInitialized(false),
	_passive(false),
	_durable(false),
	CAF_CM_INIT("ExchangeDeclareMethod") {
}

ExchangeDeclareMethod::~ExchangeDeclareMethod() {
}

void ExchangeDeclareMethod::init(
	const std::string& exchange,
	const std::string& type,
	const bool passive,
	const bool durable,
	const SmartPtrTable& arguments) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_exchange = exchange;
	_type = type;
	_passive = false;
	_durable = durable;
	_isInitialized = true;
}

std::string ExchangeDeclareMethod::getMethodName() const {
	return "exchange.declare";
}

AMQPStatus ExchangeDeclareMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_ExchangeDeclare(
			channel,
			_exchange,
			_type,
			false,
			_durable,
			false,
			NULL);
}
