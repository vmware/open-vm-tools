/*
 *  Created on: Jun 6, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Common/CVariant.h"
#include "Integration/IIntMessage.h"
#include "amqpClient/api/Envelope.h"
#include "amqpClient/api/amqpClient.h"
#include "amqpCore/DefaultAmqpHeaderMapper.h"
#include "Integration/Core/CIntMessageHeaders.h"
#include "Exception/CCafException.h"
#include "HeaderUtils.h"

using namespace Caf::AmqpIntegration;

DefaultAmqpHeaderMapper::DefaultAmqpHeaderMapper() :
	_isInitialized(false),
	CAF_CM_INIT("DefaultAmqpHeaderMapper") {
}

DefaultAmqpHeaderMapper::~DefaultAmqpHeaderMapper() {
}

void DefaultAmqpHeaderMapper::init(const std::string& userHeaderRegex) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	if (!userHeaderRegex.empty()) {
		_userHeaderRegex.CreateInstance();
		_userHeaderRegex->initialize(userHeaderRegex);
	}

	_isInitialized = true;
}

AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties
DefaultAmqpHeaderMapper::fromHeaders(
		IIntMessage::SmartPtrCHeaders headers) {
	CAF_CM_FUNCNAME("fromHeaders");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties properties;
	properties = AmqpClient::AmqpContentHeaders::createBasicProperties();

	SmartPtrCVariant variant;
	variant = HeaderUtils::getHeaderString(headers, APP_ID);
	if (variant) {
		properties->setAppId(g_variant_get_string(variant->get(), NULL));
	}
	variant = HeaderUtils::getHeaderString(headers, CONTENT_ENCODING);
	if (variant) {
		properties->setContentEncoding(g_variant_get_string(variant->get(), NULL));
	} else {
		properties->setContentEncoding("UTF8");
	}
	variant = HeaderUtils::getHeaderString(headers, CONTENT_TYPE);
	if (variant) {
		properties->setContentType(g_variant_get_string(variant->get(), NULL));
	} else {
		properties->setContentType("text/plain");
	}
	variant = HeaderUtils::getHeaderString(headers, CORRELATION_ID);
	if (variant) {
		properties->setCorrelationId(g_variant_get_string(variant->get(), NULL));
	}
	variant = HeaderUtils::getHeaderUint8(headers, DELIVERY_MODE);
	if (variant) {
		properties->setDeliveryMode(g_variant_get_byte(variant->get()));
	}
	variant = HeaderUtils::getHeaderString(headers, EXPIRATION);
	if (variant) {
		properties->setExpiration(g_variant_get_string(variant->get(), NULL));
	}
	variant = HeaderUtils::getHeaderString(headers, MESSAGE_ID);
	if (variant) {
		properties->setMessageId(g_variant_get_string(variant->get(), NULL));
	}
	variant = HeaderUtils::getHeaderString(headers, REPLY_TO);
	if (variant) {
		properties->setReplyTo(g_variant_get_string(variant->get(), NULL));
	}
	variant = HeaderUtils::getHeaderUint64(headers, TIMESTAMP);
	if (variant) {
		properties->setTimestamp(g_variant_get_uint64(variant->get()));
	}
	variant = HeaderUtils::getHeaderString(headers, TYPE);
	if (variant) {
		properties->setType(g_variant_get_string(variant->get(), NULL));
	}
	variant = HeaderUtils::getHeaderString(headers, USER_ID);
	if (variant) {
		properties->setUserId(g_variant_get_string(variant->get(), NULL));
	}

	// map user-defined headers
	if (_userHeaderRegex) {
		AmqpClient::SmartPtrTable propertyHeaders;
		propertyHeaders.CreateInstance();
		for (IIntMessage::CHeaders::const_iterator headerIter = headers->begin();
			headerIter != headers->end();
			headerIter++) {
			const std::string headerName = headerIter->first;
			if (_userHeaderRegex->isMatched(headerName)) {
				const SmartPtrIVariant headerValue = headerIter->second.first;
				CAF_CM_VALIDATE_SMARTPTR(headerValue);
				GVariant *headerVal = headerValue->get();
				if (g_variant_is_of_type(headerVal, G_VARIANT_TYPE_STRING)) {
					AmqpClient::tableAddUtf8(
							headerName,
							headerValue->toString(),
							propertyHeaders);
				} else if (g_variant_is_of_type(headerVal, G_VARIANT_TYPE_BOOLEAN)) {
					AmqpClient::tableAddBoolean(
							headerName,
							g_variant_get_boolean(headerVal),
							propertyHeaders);
				} else if (g_variant_is_of_type(headerVal, G_VARIANT_TYPE_BYTE)) {
					AmqpClient::tableAddUint8(
							headerName,
							g_variant_get_byte(headerVal),
							propertyHeaders);
				} else if (g_variant_is_of_type(headerVal, G_VARIANT_TYPE_INT16)) {
					AmqpClient::tableAddInt16(
							headerName,
							g_variant_get_int16(headerVal),
							propertyHeaders);
				} else if (g_variant_is_of_type(headerVal, G_VARIANT_TYPE_UINT16)) {
					AmqpClient::tableAddUint16(
							headerName,
							g_variant_get_uint16(headerVal),
							propertyHeaders);
				} else if (g_variant_is_of_type(headerVal, G_VARIANT_TYPE_INT32)) {
					AmqpClient::tableAddInt32(
							headerName,
							g_variant_get_int32(headerVal),
							propertyHeaders);
				} else if (g_variant_is_of_type(headerVal, G_VARIANT_TYPE_UINT32)) {
					AmqpClient::tableAddUint32(
							headerName,
							g_variant_get_uint32(headerVal),
							propertyHeaders);
				} else if (g_variant_is_of_type(headerVal, G_VARIANT_TYPE_INT64)) {
					AmqpClient::tableAddInt64(
							headerName,
							g_variant_get_int64(headerVal),
							propertyHeaders);
				} else if (g_variant_is_of_type(headerVal, G_VARIANT_TYPE_UINT64)) {
					AmqpClient::tableAddUint64(
							headerName,
							g_variant_get_uint64(headerVal),
							propertyHeaders);
				} else {
					CAF_CM_EXCEPTIONEX_VA2(
							InvalidArgumentException,
							0,
							"Unsupported GVariant conversion. [name='%s'][type='%s']",
							headerName.c_str(),
							g_variant_get_type_string(headerVal));
				}
			}
		}
		if (propertyHeaders->size()) {
			properties->setHeaders(propertyHeaders);
		}
	}
	return properties;
}

IIntMessage::SmartPtrCHeaders
	DefaultAmqpHeaderMapper::toHeaders(
			AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties properties,
			AmqpClient::SmartPtrEnvelope envelope) {
	CAF_CM_FUNCNAME_VALIDATE("toHeaders");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CIntMessageHeaders messageHeaders;
	messageHeaders.insertUint64(DELIVERY_TAG, envelope->getDeliveryTag());
	messageHeaders.insertString(RECEIVED_ROUTING_KEY, envelope->getRoutingKey());
	messageHeaders.insertBool(REDELIVERED, envelope->getRedelivered());

	// The exchange name can be empty, meaning the default exchange
	messageHeaders.insertStringOpt(RECEIVED_EXCHANGE, envelope->getExchange());

	const uint32 flags = properties->getFlags();
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_APP_ID_FLAG) {
		messageHeaders.insertString(APP_ID, properties->getAppId());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_CONTENT_ENCODING_FLAG) {
		messageHeaders.insertString(CONTENT_ENCODING, properties->getContentEncoding());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_CONTENT_TYPE_FLAG) {
		messageHeaders.insertString(CONTENT_TYPE, properties->getContentType());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_CORRELATION_ID_FLAG) {
		messageHeaders.insertString(CORRELATION_ID, properties->getCorrelationId());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_DEVLIVERY_MODE_FLAG) {
		messageHeaders.insertUint8(DELIVERY_MODE, properties->getDeliveryMode());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_EXPIRATION_FLAG) {
		messageHeaders.insertString(EXPIRATION, properties->getExpiration());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_MESSAGE_ID_FLAG) {
		messageHeaders.insertString(MESSAGE_ID, properties->getMessageId());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_REPLY_TO_FLAG) {
		messageHeaders.insertString(REPLY_TO, properties->getReplyTo());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_TIMESTAMP_FLAG) {
		messageHeaders.insertUint64(TIMESTAMP, properties->getTimestamp());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_TYPE_FLAG) {
		messageHeaders.insertString(TYPE, properties->getType());
	}
	if (flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_USER_ID_FLAG) {
		messageHeaders.insertString(USER_ID, properties->getUserId());
	}
	if ((flags & AmqpClient::AmqpContentHeaders::BASIC_PROPERTY_HEADERS_FLAG) &&
			_userHeaderRegex) {
		AmqpClient::SmartPtrTable table = properties->getHeaders();
		for (TSmartConstMapIterator<AmqpClient::Table> field(*table);
				field;
				field++) {
			const std::string fieldName = field.getKey();
			if (_userHeaderRegex->isMatched(fieldName)) {
				GVariant *fieldVar = field->getValue();
				switch (field->getAmqpType()) {
				case AmqpClient::Field::AMQP_FIELD_TYPE_UTF8:
					messageHeaders.insertString(fieldName, g_variant_get_string(fieldVar, NULL));
					break;
				case AmqpClient::Field::AMQP_FIELD_TYPE_BOOLEAN:
					messageHeaders.insertBool(fieldName, g_variant_get_boolean(fieldVar));
					break;
				case AmqpClient::Field::AMQP_FIELD_TYPE_I8:
				case AmqpClient::Field::AMQP_FIELD_TYPE_U8:
					messageHeaders.insertUint8(fieldName, g_variant_get_byte(fieldVar));
					break;
				case AmqpClient::Field::AMQP_FIELD_TYPE_I16:
					messageHeaders.insertInt16(fieldName, g_variant_get_int16(fieldVar));
					break;
				case AmqpClient::Field::AMQP_FIELD_TYPE_U16:
					messageHeaders.insertUint16(fieldName, g_variant_get_uint16(fieldVar));
					break;
				case AmqpClient::Field::AMQP_FIELD_TYPE_I32:
					messageHeaders.insertInt32(fieldName, g_variant_get_int32(fieldVar));
					break;
				case AmqpClient::Field::AMQP_FIELD_TYPE_U32:
					messageHeaders.insertUint32(fieldName, g_variant_get_uint32(fieldVar));
					break;
				case AmqpClient::Field::AMQP_FIELD_TYPE_I64:
					messageHeaders.insertInt64(fieldName, g_variant_get_int64(fieldVar));
					break;
				case AmqpClient::Field::AMQP_FIELD_TYPE_U64:
				case AmqpClient::Field::AMQP_FIELD_TYPE_TIMESTAMP:
					messageHeaders.insertUint64(fieldName, g_variant_get_uint64(fieldVar));
					break;
				default:
					break;
				}
			}
		}
	}

	return messageHeaders.getHeaders();
}

IIntMessage::SmartPtrCHeaders DefaultAmqpHeaderMapper::filterHeaders(
			IIntMessage::SmartPtrCHeaders headers) {
	CAF_CM_FUNCNAME_VALIDATE("filterHeaders");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	IIntMessage::SmartPtrCHeaders filteredHeaders;
	filteredHeaders.CreateInstance();

	if (_userHeaderRegex) {
		for (IIntMessage::CHeaders::const_iterator headerIter = headers->begin();
			headerIter != headers->end();
			headerIter++) {
			const std::string headerName = headerIter->first;
			if (_userHeaderRegex->isMatched(headerName)) {
				filteredHeaders->insert(
						IIntMessage::CHeaders::value_type(
								headerName,
								std::make_pair(headerIter->second.first, headerIter->second.second)));
			}
		}
	}
	return filteredHeaders;
}
