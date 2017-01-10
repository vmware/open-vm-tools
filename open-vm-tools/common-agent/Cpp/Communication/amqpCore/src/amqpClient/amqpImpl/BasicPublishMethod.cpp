/*
 *  Created on: May 15, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/amqpImpl/BasicPublishMethod.h"

using namespace Caf::AmqpClient;

struct MethodBodyFreeData {
	SmartPtrCDynamicByteArray body;
};

BasicPublishMethod::BasicPublishMethod() :
	_isInitialized(false),
	_mandatory(false),
	_immediate(false),
	CAF_CM_INIT("BasicPublishMethod") {
}

BasicPublishMethod::~BasicPublishMethod() {
}

void BasicPublishMethod::init(
	const std::string& exchange,
	const std::string& routingKey,
	bool mandatory,
	bool immediate,
	const AmqpContentHeaders::SmartPtrBasicProperties& properties,
	const SmartPtrCDynamicByteArray& body) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_exchange = exchange;
	_routingKey = routingKey;
	_mandatory = mandatory;
	_immediate = immediate;
	_properties = properties;
	_body = body;
	_isInitialized = true;
}

std::string BasicPublishMethod::getMethodName() const {
	return "basic.publish";
}

AMQPStatus BasicPublishMethod::send(const SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME("send");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	AMQPStatus status = AMQP_ERROR_OK;

	amqp_basic_properties_t properties;
	try {
		if (_properties) {
			SmartPtrBasicProperties propertiesClass;
			propertiesClass.QueryInterface(_properties);
			propertiesClass->getAsApiProperties(properties);
		} else {
			memset(&properties, 0, sizeof(properties));
		}

		status = AmqpUtil::AMQP_BasicPublish(
				channel,
				_exchange,
				_routingKey,
				_mandatory,
				_immediate,
				&properties,
				_body);
	}
	CAF_CM_CATCH_ALL;
	if (properties.headers.entries) {
		AMQUtil::amqpFreeApiTable(&properties.headers);
	}
	CAF_CM_THROWEXCEPTION;
	return status;
}















