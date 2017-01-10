/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "amqpClient/amqpImpl/EnvelopeImpl.h"

using namespace Caf::AmqpClient;

EnvelopeImpl::EnvelopeImpl() :
	_isInitialized(false),
	_deliveryTag(0),
	_redelivered(false),
	CAF_CM_INIT("EnvelopeImpl") {
}

EnvelopeImpl::~EnvelopeImpl() {
}

void EnvelopeImpl::init(
	const uint64 deliveryTag,
	const bool redeliver,
	const std::string& exchange,
	const std::string& routingKey) {
	_deliveryTag = deliveryTag;
	_redelivered = redeliver;
	_exchange = exchange;
	_routingKey = routingKey;
	_isInitialized = true;
}

uint64 EnvelopeImpl::getDeliveryTag() {
	CAF_CM_FUNCNAME_VALIDATE("getDeliveryTag");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _deliveryTag;
}

bool EnvelopeImpl::getRedelivered() {
	CAF_CM_FUNCNAME_VALIDATE("getRedelivered");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _redelivered;
}

std::string EnvelopeImpl::getExchange() {
	CAF_CM_FUNCNAME_VALIDATE("getExchange");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _exchange;
}

std::string EnvelopeImpl::getRoutingKey() {
	CAF_CM_FUNCNAME_VALIDATE("getRoutingKey");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _routingKey;
}
