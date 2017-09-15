/*
 *  Created on: May 15, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "amqpClient/api/amqpClient.h"

using namespace Caf::AmqpClient;

const uint32 AmqpContentHeaders::BASIC_PROPERTY_CONTENT_TYPE_FLAG			= 0x8000;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_CONTENT_ENCODING_FLAG	= 0x4000;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_HEADERS_FLAG				= 0x2000;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_DEVLIVERY_MODE_FLAG		= 0x1000;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_PRIORITY_FLAG				= 0x800;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_CORRELATION_ID_FLAG		= 0x400;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_REPLY_TO_FLAG				= 0x200;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_EXPIRATION_FLAG			= 0x100;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_MESSAGE_ID_FLAG			= 0x80;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_TIMESTAMP_FLAG				= 0x40;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_TYPE_FLAG					= 0x20;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_USER_ID_FLAG				= 0x10;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_APP_ID_FLAG					= 0x8;
const uint32 AmqpContentHeaders::BASIC_PROPERTY_CLUSTER_ID_FLAG			= 0x4;

AmqpContentHeaders::SmartPtrBasicProperties AmqpContentHeaders::createBasicProperties() {
	Caf::AmqpClient::SmartPtrBasicProperties props;
	props.CreateInstance();
	props->init();
	SmartPtrBasicProperties result;
	result.QueryInterface(props);
	return result;
}

AmqpContentHeaders::SmartPtrBasicProperties AmqpContentHeaders::createBasicProperties(
	const uint32 flags,
	const std::string& contentType,
	const std::string& contentEncoding,
	const SmartPtrTable& headers,
	const uint8 deliveryMode,
	const uint8 priority,
	const std::string& correlationId,
	const std::string& replyTo,
	const std::string& expiration,
	const std::string& messageId,
	const uint64 timestamp,
	const std::string& type,
	const std::string& userId,
	const std::string& appId,
	const std::string& clusterId) {
	Caf::AmqpClient::SmartPtrBasicProperties props;
	props.CreateInstance();
	props->init(
			flags,
			contentType,
			contentEncoding,
			headers,
			deliveryMode,
			priority,
			correlationId,
			replyTo,
			expiration,
			messageId,
			timestamp,
			type,
			userId,
			appId,
			clusterId);
	SmartPtrBasicProperties result;
	result.QueryInterface(props);
	return result;
}
