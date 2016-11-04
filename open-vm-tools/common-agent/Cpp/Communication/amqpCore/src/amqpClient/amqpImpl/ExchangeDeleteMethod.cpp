/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/ExchangeDeleteMethod.h"

using namespace Caf::AmqpClient;

ExchangeDeleteMethod::ExchangeDeleteMethod() :
	_isInitialized(false),
	_ifUnused(false),
	CAF_CM_INIT("ExchangeDeleteMethod") {
}

ExchangeDeleteMethod::~ExchangeDeleteMethod() {
}

void ExchangeDeleteMethod::init(
	const std::string& exchange,
	const bool ifUnused) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_exchange = exchange;
	_ifUnused = ifUnused;
	_isInitialized = true;
}

std::string ExchangeDeleteMethod::getMethodName() const {
	return "exchange.delete";
}

AMQPStatus ExchangeDeleteMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return AmqpUtil::AMQP_ExchangeDelete(
			channel,
			_exchange,
			_ifUnused,
			false);
}
