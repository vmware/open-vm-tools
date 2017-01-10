/*
 *  Created on: May 3, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/amqpImpl/FieldImpl.h"
#include "amqpClient/api/amqpClient.h"
#include "Exception/CCafException.h"
#include "AMQUtil.h"

using namespace Caf::AmqpClient;
using namespace Caf::AmqpClient::AmqpExceptions;

void AMQUtil::checkAmqpStatus(
		const AMQPStatus status,
		const char* message) {
	CAF_CM_STATIC_FUNC("AMQUtil", "checkAmqpStatus");

	const char* exMsg = message ? message : "";

	switch (status) {
	case AMQP_ERROR_OK:
		break;
	case AMQP_ERROR_TIMEOUT:
		CAF_CM_EXCEPTIONEX_VA0(AmqpTimeoutException, 0, exMsg);
		break;
	case AMQP_ERROR_NO_MEMORY:
		CAF_CM_EXCEPTIONEX_VA0(AmqpNoMemoryException, 0, exMsg);
		break;
	case AMQP_ERROR_INVALID_HANDLE:
		CAF_CM_EXCEPTIONEX_VA0(AmqpInvalidHandleException, 0, exMsg);
		break;
	case AMQP_ERROR_INVALID_ARGUMENT:
		CAF_CM_EXCEPTIONEX_VA0(AmqpInvalidArgumentException, 0, exMsg);
		break;
	case AMQP_ERROR_WRONG_STATE:
		CAF_CM_EXCEPTIONEX_VA0(AmqpWrongStateException, 0, exMsg);
		break;
	case AMQP_ERROR_TOO_MANY_CHANNELS:
		CAF_CM_EXCEPTIONEX_VA0(AmqpTooManyChannelsException, 0, exMsg);
		break;
	case AMQP_ERROR_QUEUE_FULL:
		CAF_CM_EXCEPTIONEX_VA0(AmqpQueueFullException, 0, exMsg);
		break;
	case AMQP_ERROR_FRAME_TOO_LARGE:
		CAF_CM_EXCEPTIONEX_VA0(AmqpFrameTooLargeException, 0, exMsg);
		break;
	case AMQP_ERROR_IO_ERROR:
		CAF_CM_EXCEPTIONEX_VA0(AmqpIoErrorException, 0, exMsg);
		break;
	case AMQP_ERROR_PROTOCOL_ERROR:
		CAF_CM_EXCEPTIONEX_VA0(AmqpProtocolErrorException, 0, exMsg);
		break;
	case AMQP_ERROR_UNIMPLEMENTED:
		CAF_CM_EXCEPTIONEX_VA0(AmqpUnimplementedException, 0, exMsg);
		break;
	case AMQP_ERROR_IO_INTERRUPTED:
		CAF_CM_EXCEPTIONEX_VA0(AmqpIoInterruptedException, 0, exMsg);
		break;
	default:
		CAF_CM_EXCEPTIONEX_VA0(AmqpException, status, exMsg);
		break;
	}
}

std::string AMQUtil::amqpBytesToString(const amqp_bytes_t * const amqpBytes) {
	CAF_CM_STATIC_FUNC_VALIDATE("AMQUtil", "amqpBytesToString");
	CAF_CM_VALIDATE_PTR(amqpBytes);

	CDynamicCharArray buf;
	buf.allocateBytes(amqpBytes->len);
	buf.memCpy(amqpBytes->bytes, amqpBytes->len);
	return buf.getPtr();
}

SmartPtrTable AMQUtil::amqpApiTableToTableObj(const amqp_table_t * const amqpTable) {
	CAF_CM_STATIC_FUNC_LOG("AMQUtil", "amqpApiTableToTableObj");
	CAF_CM_VALIDATE_PTR(amqpTable);
	SmartPtrTable table;
	table.CreateInstance();
	for (int32 idx = 0; idx < amqpTable->num_entries; ++idx) {
		amqp_table_entry_t *entry = &amqpTable->entries[idx];
		CAF_CM_VALIDATE_PTR(entry);
		std::string key =	amqpBytesToString(&entry->key);
		SmartPtrFieldImpl field;
		field.CreateInstance();
		switch (entry->value.kind) {
		case AMQP_FIELD_KIND_BOOLEAN:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_BOOLEAN,
					g_variant_new_boolean(entry->value.value.boolean));
			break;
		case AMQP_FIELD_KIND_I8:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_I8,
					g_variant_new_byte(entry->value.value.i8));
			break;
		case AMQP_FIELD_KIND_U8:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_U8,
					g_variant_new_byte(entry->value.value.i8));
			break;
		case AMQP_FIELD_KIND_I16:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_I16,
					g_variant_new_int16(entry->value.value.i16));
			break;
		case AMQP_FIELD_KIND_U16:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_U16,
					g_variant_new_uint16(entry->value.value.u16));
			break;
		case AMQP_FIELD_KIND_I32:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_I32,
					g_variant_new_int32(entry->value.value.i32));
			break;
		case AMQP_FIELD_KIND_U32:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_U32,
					g_variant_new_uint32(entry->value.value.u32));
			break;
		case AMQP_FIELD_KIND_I64:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_I64,
					g_variant_new_int64(entry->value.value.i64));
			break;
		case AMQP_FIELD_KIND_U64:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_U64,
					g_variant_new_uint64(entry->value.value.u64));
			break;
		case AMQP_FIELD_KIND_F32:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_F32,
					g_variant_new_double(entry->value.value.f32));
			break;
		case AMQP_FIELD_KIND_F64:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_F32,
					g_variant_new_double(entry->value.value.f64));
			break;
		case AMQP_FIELD_KIND_UTF8:
			{
				const std::string value = amqpBytesToString(&entry->value.value.bytes);
				field->setTypeAndValue(
						Field::AMQP_FIELD_TYPE_UTF8,
						g_variant_new_string(value.c_str()));
			}
			break;
		case AMQP_FIELD_KIND_TIMESTAMP:
			field->setTypeAndValue(
					Field::AMQP_FIELD_TYPE_TIMESTAMP,
					g_variant_new_uint64(entry->value.value.u64));
			break;

			// Type not currently supported
		case AMQP_FIELD_KIND_ARRAY:
		case AMQP_FIELD_KIND_BYTES:
		case AMQP_FIELD_KIND_DECIMAL:
		case AMQP_FIELD_KIND_TABLE:
		case AMQP_FIELD_KIND_VOID:
			field = NULL;
			CAF_CM_LOG_ERROR_VA2(
					"AMQP field %s type %d is not supported",
					key.c_str(),
					entry->value.kind);
			break;
		default:
			field = NULL;
			CAF_CM_LOG_ERROR_VA2(
					"AMQP field %s type %d is unknown",
					key.c_str(),
					entry->value.kind);
			break;
		}

		if (field) {
			if (!table->insert(std::make_pair(key, field)).second) {
				CAF_CM_EXCEPTIONEX_VA1(
						DuplicateElementException,
						0,
						"Duplicate field '%s' detected",
						key.c_str());
			}
		}
	}
	return table;
}

void AMQUtil::amqpTableObjToApiTable(const SmartPtrTable& table, amqp_table_t& apiTable) {
	CAF_CM_STATIC_FUNC_LOG("AMQUtil", "amqpTableObjToApiTable");
	CAF_CM_VALIDATE_SMARTPTR(table);

	memset(&apiTable, 0, sizeof(apiTable));
	apiTable.num_entries = static_cast<int32>(table->size());
	apiTable.entries =
			reinterpret_cast<amqp_table_entry_t*>(AmqpCommon::AMQP_Calloc(
					apiTable.num_entries,
					sizeof(*apiTable.entries)));
	CAF_CM_VALIDATE_PTR(apiTable.entries);
	try {
		int32 idx = 0;
		for (TSmartConstMapIterator<Table> tableEntry(*table);
				tableEntry;
				tableEntry++, idx++) {
			amqp_table_entry_t *apiEntry = &apiTable.entries[idx];
			apiEntry->key = amqp_cstring_bytes(tableEntry.getKey().c_str());
			const SmartPtrField& field = *tableEntry;
			GVariant *variant = field->getValue();

			switch (field->getAmqpType()) {
			case Field::AMQP_FIELD_TYPE_NOTSET:
				CAF_CM_EXCEPTIONEX_VA1(
						IllegalStateException,
						0,
						"Table entry '%s' has a value type of AMQP_FIELD_TYPE_NOTSET",
						tableEntry.getKey().c_str());
				break;
			case Field::AMQP_FIELD_TYPE_BOOLEAN:
				apiEntry->value.kind = AMQP_FIELD_KIND_BOOLEAN;
				apiEntry->value.value.boolean = g_variant_get_boolean(variant);
				break;
			case Field::AMQP_FIELD_TYPE_I8:
				apiEntry->value.kind = AMQP_FIELD_KIND_I8;
				apiEntry->value.value.i8 = g_variant_get_byte(variant);
				break;
			case Field::AMQP_FIELD_TYPE_U8:
				apiEntry->value.kind = AMQP_FIELD_KIND_U8;
				apiEntry->value.value.u8 = g_variant_get_byte(variant);
				break;
			case Field::AMQP_FIELD_TYPE_I16:
				apiEntry->value.kind = AMQP_FIELD_KIND_I16;
				apiEntry->value.value.i16 = g_variant_get_int16(variant);
				break;
			case Field::AMQP_FIELD_TYPE_U16:
				apiEntry->value.kind = AMQP_FIELD_KIND_U16;
				apiEntry->value.value.u16 = g_variant_get_uint16(variant);
				break;
			case Field::AMQP_FIELD_TYPE_I32:
				apiEntry->value.kind = AMQP_FIELD_KIND_I32;
				apiEntry->value.value.i32 = g_variant_get_int32(variant);
				break;
			case Field::AMQP_FIELD_TYPE_U32:
				apiEntry->value.kind = AMQP_FIELD_KIND_U32;
				apiEntry->value.value.u32 = g_variant_get_uint32(variant);
				break;
			case Field::AMQP_FIELD_TYPE_I64:
				apiEntry->value.kind = AMQP_FIELD_KIND_I64;
				apiEntry->value.value.i64 = g_variant_get_int64(variant);
				break;
			case Field::AMQP_FIELD_TYPE_U64:
				apiEntry->value.kind = AMQP_FIELD_KIND_U64;
				apiEntry->value.value.u64 = g_variant_get_uint64(variant);
				break;
			case Field::AMQP_FIELD_TYPE_F32:
				apiEntry->value.kind = AMQP_FIELD_KIND_F32;
				apiEntry->value.value.f32 = static_cast<float>(g_variant_get_double(variant));
				break;
			case Field::AMQP_FIELD_TYPE_F64:
				apiEntry->value.kind = AMQP_FIELD_KIND_F64;
				apiEntry->value.value.f64 = g_variant_get_double(variant);
				break;
			case Field::AMQP_FIELD_TYPE_UTF8:
				apiEntry->value.kind = AMQP_FIELD_KIND_UTF8;
				apiEntry->value.value.bytes =
						amqp_cstring_bytes(g_variant_get_string(variant, NULL));
				break;
			case Field::AMQP_FIELD_TYPE_TIMESTAMP:
				apiEntry->value.kind = AMQP_FIELD_KIND_TIMESTAMP;
				apiEntry->value.value.u64 = g_variant_get_uint64(variant);
				break;
			case Field::AMQP_FIELD_TYPE_ARRAY:
			case Field::AMQP_FIELD_TYPE_BYTES:
			case Field::AMQP_FIELD_TYPE_DECIMAL:
			case Field::AMQP_FIELD_TYPE_TABLE:
			case Field::AMQP_FIELD_TYPE_VOID:
			default:
				CAF_CM_LOG_ERROR_VA2(
						"AMQP field %s type %d is not supported",
						tableEntry.getKey().c_str(),
						field->getAmqpType());
				break;
			}
		}
	}
	CAF_CM_CATCH_ALL;
	if (CAF_CM_ISEXCEPTION) {
		amqpFreeApiTable(&apiTable);
		memset(&apiTable, 0, sizeof(apiTable));
	}
	CAF_CM_THROWEXCEPTION;
}

void AMQUtil::amqpFreeApiTable(amqp_table_t *table) {
	if (table) {
		AmqpCommon::AMQP_Free(table->entries);
		memset(table, 0, sizeof(*table));
	}
}
