/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/amqpImpl/FieldImpl.h"
#include "amqpClient/api/amqpClient.h"
#include "Exception/CCafException.h"

using namespace Caf;
using namespace Caf::AmqpClient;

void tableAddField(
	const std::string key,
	const AmqpClient::Field::AmqpFieldType type,
	GVariant* variant,
	AmqpClient::SmartPtrTable& table) {
	CAF_CM_STATIC_FUNC_LOG("AmqpClient", "tableAddField");
	CAF_CM_VALIDATE_STRING(key);
	CAF_CM_VALIDATE_PTR(variant);
	CAF_CM_VALIDATE_SMARTPTR(table);

	AmqpClient::SmartPtrFieldImpl field;
	field.CreateInstance();
	field->setTypeAndValue(type, variant);
	if (!table->insert(std::make_pair(key, field)).second) {
		g_variant_unref(variant);
		CAF_CM_EXCEPTIONEX_VA1(
				DuplicateElementException,
				0,
				"Duplicate table entry '%s'",
				key.c_str());
	}
}

void AmqpClient::tableAddBoolean(
	const std::string key,
	const bool val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_BOOLEAN,
			g_variant_new_boolean(val),
			table);
}

void AmqpClient::tableAddInt8(
	const std::string key,
	const int8 val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_I8,
			g_variant_new_byte(val),
			table);
}

void AmqpClient::tableAddUint8(
	const std::string key,
	const uint8 val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_U8,
			g_variant_new_byte(val),
			table);
}

void AmqpClient::tableAddInt16(
	const std::string key,
	const int16 val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_I16,
			g_variant_new_int16(val),
			table);
}

void AmqpClient::tableAddUint16(
	const std::string key,
	const uint16 val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_U16,
			g_variant_new_uint16(val),
			table);
}

void AmqpClient::tableAddInt32(
	const std::string key,
	const int32 val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_I32,
			g_variant_new_int32(val),
			table);
}

void AmqpClient::tableAddUint32(
	const std::string key,
	const uint32 val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_U32,
			g_variant_new_uint32(val),
			table);
}

void AmqpClient::tableAddInt64(
	const std::string key,
	const int64 val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_I64,
			g_variant_new_int64(val),
			table);
}

void AmqpClient::tableAddUint64(
	const std::string key,
	const uint64 val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_U64,
			g_variant_new_uint64(val),
			table);
}

void AmqpClient::tableAddFloat(
	const std::string key,
	const float val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_F32,
			g_variant_new_double(val),
			table);
}

void AmqpClient::tableAddDouble(
	const std::string key,
	const double val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_F64,
			g_variant_new_double(val),
			table);
}

void AmqpClient::tableAddUtf8(
	const std::string key,
	const std::string& val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_UTF8,
			g_variant_new_string(val.c_str()),
			table);
}

void AmqpClient::tableAddTimestamp(
	const std::string key,
	const uint64 val,
	SmartPtrTable& table) {
	tableAddField(
			key,
			AmqpClient::Field::AMQP_FIELD_TYPE_TIMESTAMP,
			g_variant_new_uint64(val),
			table);
}
