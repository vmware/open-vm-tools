/*
 *  Created on: May 3, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_H_
#define AMQPCLIENTAPI_H_

#include "amqpClient/AmqpClientLink.h"
#include "ICafObject.h"

/**
 * @defgroup AmqpApi AMQP API
 * Documentaiton for the classes, methods and constants that can be
 * used directly by applications to work with the AMQP protocol as a client.
 */
namespace Caf { namespace AmqpClient {

/** Default user name */
extern const char* DEFAULT_USER;

/** Default password */
extern const char* DEFAULT_PASS;

/** Default virtual host */
extern const char* DEFAULT_VHOST;

/** Default protocol */
extern const char* DEFAULT_PROTOCOL;

/** Default host */
extern const char* DEFAULT_HOST;

/** Default maximum channel number; zero for maximum */
static const uint32 DEFAULT_CHANNEL_MAX = 0;

/** Default maximum frame size; zero for maximum */
static const uint32 DEFAULT_FRAME_MAX = 131072;

/** Default maximum heartbeat rate; zero for none */
static const uint32 DEFAULT_HEARTBEAT = 0;

/** 'Use the default port' port */
static const uint32 USE_DEFAULT_PORT = UINT_MAX;

/** The default non-ssl port */
static const uint32 DEFAULT_AMQP_PORT = 5672;

/** The default ssl port */
static const uint32 DEFAULT_AMQP_SSL_PORT = 5671;

/** The default connection timeout; zero means wait indefinately */
static const uint32 DEFAULT_CONNECTION_TIMEOUT = 0;

/** The default number of connection consumer threads */
static const uint32 DEFAULT_CONSUMER_THREAD_COUNT = 10;

/** The default number of basic.deliver messages to process in a single run of a channel consumer thread */
static const uint32 DEFAULT_CONSUMER_THREAD_MAX_DELIVERY_COUNT = 100;

/** The default number of times a connection open will be retried*/
static const uint16 DEFAULT_CONNECTION_RETRIES = 5;

/** The default number of  seconds we will wait for each connection open attempt. 0 means wait indefinitely*/
static const uint16 DEFAULT_CONNECTION_SECONDS_TO_WAIT = 30;

/**
 * @author mdonahue
 * @brief Object that maps a c-api AMQP field into a lifetime-managed GVariant
 */
struct __declspec(novtable) Field : public ICafObject {
	/**
	 * @brief Field value types
	 */
	typedef enum {
		/** @brief internal value representing Not Set */
		AMQP_FIELD_TYPE_NOTSET,
		/** @brief boolean */
		AMQP_FIELD_TYPE_BOOLEAN,
		/** @brief signed 8-bit integer */
		AMQP_FIELD_TYPE_I8,
		/** @brief unsigned 8-bit integer */
		AMQP_FIELD_TYPE_U8,
		/** @brief signed 16-bit integer */
		AMQP_FIELD_TYPE_I16,
		/** @brief unsigned 16-bit integer */
		AMQP_FIELD_TYPE_U16,
		/** @brief signed 32-bit integer */
		AMQP_FIELD_TYPE_I32,
		/** @brief unsigned 32-bit integer */
		AMQP_FIELD_TYPE_U32,
		/** @brief signed 64-bit integer */
		AMQP_FIELD_TYPE_I64,
		/** @brief unsigned 64-bit integer */
		AMQP_FIELD_TYPE_U64,
		/** @brief 32-bit float */
		AMQP_FIELD_TYPE_F32,
		/** @brief 64-bit double */
		AMQP_FIELD_TYPE_F64,
		/** @brief UTF8-encoded text */
		AMQP_FIELD_TYPE_UTF8,
		/** @brief NOT SUPPORTED */
		AMQP_FIELD_TYPE_ARRAY,
		/** @brief NOT SUPPORTED */
		AMQP_FIELD_TYPE_BYTES,
		/** @brief NOT SUPPORTED */
		AMQP_FIELD_TYPE_DECIMAL,
		/** @brief NOT SUPPORTED */
		AMQP_FIELD_TYPE_TIMESTAMP,
		/** @brief NOT SUPPORTED */
		AMQP_FIELD_TYPE_TABLE,
		/** @brief NOT SUPPORTED */
		AMQP_FIELD_TYPE_VOID
	} AmqpFieldType;

	/**
	 * @return the field type
	 */
	virtual AmqpFieldType getAmqpType() const = 0;
	/**
	 * @return the field value as a GVariant
	 */
	virtual GVariant* getValue() const = 0;
	/**
	 * @brief Set the field type and value
	 * @param type field type
	 * @param value field value. <code><b>DO NOT increment the reference count
	 * before calling this method.</b></code> This object will take ownership of
	 * the GVariant and will call g_variant_unref upon value reassignment or destruction.
	 */
	virtual void setTypeAndValue(AmqpFieldType type, GVariant *value) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Field);
/** @brief A map of field names to field objects */
typedef std::map<std::string, SmartPtrField> Table;
CAF_DECLARE_SMART_POINTER(Table);

/**
 * @brief Add a boolean value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddBoolean(
	const std::string key,
	const bool val,
	SmartPtrTable& table);

/**
 * @brief Add a signed 8-bit value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddInt8(
	const std::string key,
	const int8 val,
	SmartPtrTable& table);

/**
 * @brief Add a unsigned 8-bit value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddUint8(
	const std::string key,
	const uint8 val,
	SmartPtrTable& table);

/**
 * @brief Add a signed 16-bit value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddInt16(
	const std::string key,
	const int16 val,
	SmartPtrTable& table);

/**
 * @brief Add a unsigned 16-bit value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddUint16(
	const std::string key,
	const uint16 val,
	SmartPtrTable& table);

/**
 * @brief Add a signed 32-bit value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddInt32(
	const std::string key,
	const int32 val,
	SmartPtrTable& table);

/**
 * @brief Add a unsigned 32-bit value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddUint32(
	const std::string key,
	const uint32 val,
	SmartPtrTable& table);

/**
 * @brief Add a signed 64-bit value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddInt64(
	const std::string key,
	const int64 val,
	SmartPtrTable& table);

/**
 * @brief Add a unsigned 64-bit value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddUint64(
	const std::string key,
	const uint64 val,
	SmartPtrTable& table);

/**
 * @brief Add a float value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddFloat(
	const std::string key,
	const float val,
	SmartPtrTable& table);

/**
 * @brief Add a double value to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddDouble(
	const std::string key,
	const double val,
	SmartPtrTable& table);

/**
 * @brief Add UTF8-encoded text to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddUtf8(
	const std::string key,
	const std::string& val,
	SmartPtrTable& table);

/**
 * @brief Add a time stamp (unsigned 64-bit integer) to a field table
 * @param key the field key
 * @param val the value
 * @param table the table to insert the field into
 */
void AMQPCLIENT_LINKAGE tableAddTimestamp(
	const std::string key,
	const uint64 val,
	SmartPtrTable& table);
}}

#include "amqpClient/api/AMQExceptions.h"

#endif
