/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/api/amqpClient.h"
#include "Exception/CCafException.h"
#include "amqpClient/amqpImpl/BasicProperties.h"

using namespace Caf::AmqpClient;

BasicProperties::BasicProperties() :
	_isInitialized(false),
	_flags(0),
	_bodySize(0),
	_deliveryMode(0),
	_priority(0),
	_timestamp(0),
	CAF_CM_INIT("BasicProperties") {
}

BasicProperties::~BasicProperties() {
}

void BasicProperties::init() {
	_isInitialized = true;
}

void BasicProperties::init(
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
	CAF_CM_FUNCNAME_VALIDATE("init");

	_flags = flags;
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_CONTENT_TYPE_FLAG) {
		CAF_CM_VALIDATE_STRING(contentType);
		_contentType = contentType;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_CONTENT_ENCODING_FLAG) {
		CAF_CM_VALIDATE_STRING(contentEncoding);
		_contentEncoding = contentEncoding;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_HEADERS_FLAG) {
		CAF_CM_VALIDATE_SMARTPTR(headers);
		_headers = headers;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_DEVLIVERY_MODE_FLAG) {
		_deliveryMode = deliveryMode;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_PRIORITY_FLAG) {
		_priority = priority;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_CORRELATION_ID_FLAG) {
		_correlationId = correlationId;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_REPLY_TO_FLAG) {
		_replyTo = replyTo;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_EXPIRATION_FLAG) {
		_expiration = expiration;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_MESSAGE_ID_FLAG) {
		_messageId = messageId;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_TIMESTAMP_FLAG) {
		_timestamp = timestamp;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_TYPE_FLAG) {
		_type = type;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_USER_ID_FLAG) {
		_userId = userId;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_APP_ID_FLAG) {
		_appId = appId;
	}
	if (flags & AmqpContentHeaders::BASIC_PROPERTY_CLUSTER_ID_FLAG) {
		_clusterId = clusterId;
	}
	_isInitialized = true;
}

void BasicProperties::init(const SmartPtrCAmqpFrame& frame) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_PTR(frame);

	if (frame->getFrameType() != AMQP_FRAME_HEADER) {
		CAF_CM_EXCEPTIONEX_VA1(
				AmqpExceptions::UnexpectedFrameException,
				0,
				"Expected an AMQP header frame. Received type %d",
				frame->getFrameType());
	}

	if (frame->getHeaderClassId() != AMQP_BASIC_CLASS) {
		CAF_CM_EXCEPTIONEX_VA1(
				AmqpExceptions::UnknownClassOrMethodException,
				0,
				"[class=0x%04X]",
				frame->getHeaderClassId());
	}

	const amqp_basic_properties_t * const decoded = frame->getHeaderProperties();
	_bodySize = frame->getHeaderBodySize();
	const amqp_flags_t flags = decoded->_flags;

	if (flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
		_contentType = AMQUtil::amqpBytesToString(&decoded->content_type);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_CONTENT_TYPE_FLAG;
	}
	if (flags & AMQP_BASIC_CONTENT_ENCODING_FLAG) {
		_contentEncoding = AMQUtil::amqpBytesToString(&decoded->content_encoding);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_CONTENT_ENCODING_FLAG;
	}
	if (flags & AMQP_BASIC_HEADERS_FLAG) {
		_headers = AMQUtil::amqpApiTableToTableObj(&decoded->headers);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_HEADERS_FLAG;
	}
	if (flags & AMQP_BASIC_DELIVERY_MODE_FLAG) {
		_deliveryMode = decoded->delivery_mode;
		_flags |=  AmqpContentHeaders::BASIC_PROPERTY_DEVLIVERY_MODE_FLAG;
	}
	if (flags & AMQP_BASIC_PRIORITY_FLAG) {
		_priority = decoded->priority;
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_PRIORITY_FLAG;
	}
	if (flags & AMQP_BASIC_CORRELATION_ID_FLAG) {
		_correlationId = AMQUtil::amqpBytesToString(&decoded->correlation_id);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_CORRELATION_ID_FLAG;
	}
	if (flags & AMQP_BASIC_REPLY_TO_FLAG) {
		_replyTo = AMQUtil::amqpBytesToString(&decoded->reply_to);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_REPLY_TO_FLAG;
	}
	if (flags & AMQP_BASIC_EXPIRATION_FLAG) {
		_expiration = AMQUtil::amqpBytesToString(&decoded->expiration);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_EXPIRATION_FLAG;
	}
	if (flags & AMQP_BASIC_MESSAGE_ID_FLAG) {
		_messageId = AMQUtil::amqpBytesToString(&decoded->message_id);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_MESSAGE_ID_FLAG;
	}
	if (flags & AMQP_BASIC_TIMESTAMP_FLAG) {
		_timestamp = decoded->timestamp;
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_TIMESTAMP_FLAG;
	}
	if (flags & AMQP_BASIC_TYPE_FLAG) {
		_type = AMQUtil::amqpBytesToString(&decoded->type);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_TYPE_FLAG;
	}
	if (flags & AMQP_BASIC_USER_ID_FLAG) {
		_userId = AMQUtil::amqpBytesToString(&decoded->user_id);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_USER_ID_FLAG;
	}
	if (flags & AMQP_BASIC_APP_ID_FLAG) {
		_appId = AMQUtil::amqpBytesToString(&decoded->app_id);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_APP_ID_FLAG;
	}
	if (flags & AMQP_BASIC_CLUSTER_ID_FLAG) {
		_clusterId = AMQUtil::amqpBytesToString(&decoded->cluster_id);
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_CLUSTER_ID_FLAG;
	}
	_isInitialized = true;
}

bool BasicProperties::areHeadersAvailable() {
	return (_flags & AmqpContentHeaders::BASIC_PROPERTY_HEADERS_FLAG);
}

void BasicProperties::getAsApiProperties(amqp_basic_properties_t& properties) {
	CAF_CM_FUNCNAME_VALIDATE("getAsApiProperties");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	memset(&properties, 0, sizeof(properties));
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_CONTENT_TYPE_FLAG) {
		properties.content_type.len = _contentType.length();
		properties.content_type.bytes = const_cast<char*>(_contentType.c_str());
		properties._flags |= AMQP_BASIC_CONTENT_TYPE_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_CONTENT_ENCODING_FLAG) {
		properties.content_encoding.len = _contentEncoding.length();
		properties.content_encoding.bytes = const_cast<char*>(_contentEncoding.c_str());
		properties._flags |= AMQP_BASIC_CONTENT_ENCODING_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_HEADERS_FLAG) {
		AMQUtil::amqpTableObjToApiTable(_headers, properties.headers);
		properties._flags |= AMQP_BASIC_HEADERS_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_DEVLIVERY_MODE_FLAG) {
		properties.delivery_mode = _deliveryMode;
		properties._flags |= AMQP_BASIC_DELIVERY_MODE_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_PRIORITY_FLAG) {
		properties.priority = _priority;
		properties._flags |= AMQP_BASIC_PRIORITY_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_CORRELATION_ID_FLAG) {
		properties.correlation_id.len = _correlationId.length();
		properties.correlation_id.bytes = const_cast<char*>(_correlationId.c_str());
		properties._flags |= AMQP_BASIC_CORRELATION_ID_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_REPLY_TO_FLAG) {
		properties.reply_to.len = _replyTo.length();
		properties.reply_to.bytes = const_cast<char*>(_replyTo.c_str());
		properties._flags |= AMQP_BASIC_REPLY_TO_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_EXPIRATION_FLAG) {
		properties.expiration.len = _expiration.length();
		properties.expiration.bytes = const_cast<char*>(_expiration.c_str());
		properties._flags |= AMQP_BASIC_EXPIRATION_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_MESSAGE_ID_FLAG) {
		properties.message_id.len = _messageId.length();
		properties.message_id.bytes = const_cast<char*>(_messageId.c_str());
		properties._flags |= AMQP_BASIC_MESSAGE_ID_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_TIMESTAMP_FLAG) {
		properties.timestamp = _timestamp;
		properties._flags |= AMQP_BASIC_TIMESTAMP_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_TYPE_FLAG) {
		properties.type.len = _type.length();
		properties.type.bytes = const_cast<char*>(_type.c_str());
		properties._flags |= AMQP_BASIC_TYPE_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_USER_ID_FLAG) {
		properties.user_id.len = _userId.length();
		properties.user_id.bytes = const_cast<char*>(_userId.c_str());
		properties._flags |= AMQP_BASIC_USER_ID_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_APP_ID_FLAG) {
		properties.app_id.len = _appId.length();
		properties.app_id.bytes = const_cast<char*>(_appId.c_str());
		properties._flags |= AMQP_BASIC_APP_ID_FLAG;
	}
	if (_flags & AmqpContentHeaders::BASIC_PROPERTY_CLUSTER_ID_FLAG) {
		properties.cluster_id.len = _clusterId.length();
		properties.cluster_id.bytes = const_cast<char*>(_clusterId.c_str());
		properties._flags |= AMQP_BASIC_CLUSTER_ID_FLAG;
	}
}

uint16 BasicProperties::getClassId() {
	return AMQP_BASIC_CLASS;
}

std::string BasicProperties::getClassName() {
	return "basic";
}

uint64 BasicProperties::getBodySize() {
	return _bodySize;
}

uint32 BasicProperties::getFlags() {
	CAF_CM_FUNCNAME_VALIDATE("getFlags");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _flags;
}

std::string BasicProperties::getContentType() {
	CAF_CM_FUNCNAME_VALIDATE("getContentType");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_CONTENT_TYPE_FLAG,
		"contentType");
	return _contentType;
}

std::string BasicProperties::getContentEncoding() {
	CAF_CM_FUNCNAME_VALIDATE("getContentType");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_CONTENT_ENCODING_FLAG,
		"contentEncoding");
	return _contentEncoding;
}

SmartPtrTable BasicProperties::getHeaders() {
	CAF_CM_FUNCNAME_VALIDATE("getHeaders");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_HEADERS_FLAG,
		"headers");
	return _headers;
}

uint8 BasicProperties::getDeliveryMode() {
	CAF_CM_FUNCNAME_VALIDATE("getDeliveryMode");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_DEVLIVERY_MODE_FLAG,
		"deliveryMode");
	return _deliveryMode;
}

uint8 BasicProperties::getPriority() {
	CAF_CM_FUNCNAME_VALIDATE("getPriority");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_PRIORITY_FLAG,
		"priority");
	return _priority;
}

std::string BasicProperties::getCorrelationId() {
	CAF_CM_FUNCNAME_VALIDATE("getCorrelationId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_CORRELATION_ID_FLAG,
		"correlationId");
	return _correlationId;
}

std::string BasicProperties::getReplyTo() {
	CAF_CM_FUNCNAME_VALIDATE("getReplyTo");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_REPLY_TO_FLAG,
		"replyTo");
	return _replyTo;
}

std::string BasicProperties::getExpiration() {
	CAF_CM_FUNCNAME_VALIDATE("getExpiration");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_EXPIRATION_FLAG,
		"expiration");
	return _expiration;
}

std::string BasicProperties::getMessageId() {
	CAF_CM_FUNCNAME_VALIDATE("getMessageId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_MESSAGE_ID_FLAG,
		"messageId");
	return _messageId;
}

uint64 BasicProperties::getTimestamp() {
	CAF_CM_FUNCNAME_VALIDATE("getTimestamp");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_TIMESTAMP_FLAG,
		"timestamp");
	return _timestamp;
}

std::string BasicProperties::getType() {
	CAF_CM_FUNCNAME_VALIDATE("getType");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_TYPE_FLAG,
		"type");
	return _type;
}

std::string BasicProperties::getUserId() {
	CAF_CM_FUNCNAME_VALIDATE("getUserId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_USER_ID_FLAG,
		"userId");
	return _userId;
}

std::string BasicProperties::getAppId() {
	CAF_CM_FUNCNAME_VALIDATE("getAppId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_APP_ID_FLAG,
		"appId");
	return _appId;
}

std::string BasicProperties::getClusterId() {
	CAF_CM_FUNCNAME_VALIDATE("getClusterId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	ValidatePropertyIsSet(
		AmqpContentHeaders::BASIC_PROPERTY_CLUSTER_ID_FLAG,
		"clusterId");
	return _clusterId;
}

void BasicProperties::setContentType(const std::string& contentType) {
	_contentType = contentType;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_CONTENT_TYPE_FLAG;
}

void BasicProperties::setContentEncoding(const std::string& contentEncoding) {
	_contentEncoding = contentEncoding;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_CONTENT_ENCODING_FLAG;
}

void BasicProperties::setHeaders(const SmartPtrTable& headers) {
	if (headers) {
		_headers = headers;
		_flags |= AmqpContentHeaders::BASIC_PROPERTY_HEADERS_FLAG;
	}
}

void BasicProperties::setDeliveryMode(const uint8 deliveryMode) {
	_deliveryMode = deliveryMode;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_DEVLIVERY_MODE_FLAG;
}

void BasicProperties::setPriority(const uint8 priority) {
	_priority = priority;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_PRIORITY_FLAG;
}

void BasicProperties::setCorrelationId(const std::string& correlationId) {
	_correlationId = correlationId;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_CORRELATION_ID_FLAG;
}

void BasicProperties::setReplyTo(const std::string& replyTo) {
	_replyTo = replyTo;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_REPLY_TO_FLAG;
}

void BasicProperties::setExpiration(const std::string& expiration) {
	_expiration = expiration;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_EXPIRATION_FLAG;
}

void BasicProperties::setMessageId(const std::string& messageId) {
	_messageId = messageId;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_MESSAGE_ID_FLAG;
}

void BasicProperties::setTimestamp(const uint64 timestamp) {
	_timestamp = timestamp;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_TIMESTAMP_FLAG;
}

void BasicProperties::setType(const std::string& type) {
	_type = type;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_TYPE_FLAG;
}

void BasicProperties::setUserId(const std::string& userId) {
	_userId = userId;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_USER_ID_FLAG;
}

void BasicProperties::setAppId(const std::string& appId) {
	_appId = appId;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_APP_ID_FLAG;
}

void BasicProperties::setClusterId(const std::string& clusterId) {
	_clusterId = clusterId;
	_flags |= AmqpContentHeaders::BASIC_PROPERTY_CLUSTER_ID_FLAG;
}

void BasicProperties::ValidatePropertyIsSet(
	const uint32 flag,
	const char* propertyName) {
	CAF_CM_FUNCNAME("ValidatePropertyIsSet");
	if (!(_flags & flag)) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"The property '%s' is not set",
				propertyName);
	}
}
