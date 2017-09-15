/*
 *  Created on: Oct 7, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2014-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCORE_AMQPCOMMON_H_
#define AMQPCORE_AMQPCOMMON_H_

namespace Caf { namespace AmqpClient {

/** AMQP status codes. */
typedef enum {
	AMQP_ERROR_OK = 0, /**< AMQP_ERROR_OK */
	AMQP_ERROR_TIMEOUT, /**< AMQP_ERROR_TIMEOUT */
	AMQP_ERROR_NO_MEMORY, /**< AMQP_ERROR_NO_MEMORY */
	AMQP_ERROR_INVALID_HANDLE, /**< AMQP_ERROR_INVALID_HANDLE */
	AMQP_ERROR_INVALID_ARGUMENT, /**< AMQP_ERROR_INVALID_ARGUMENT */
	AMQP_ERROR_WRONG_STATE, /**< AMQP_ERROR_WRONG_STATE */
	AMQP_ERROR_TOO_MANY_CHANNELS, /**< AMQP_ERROR_TOO_MANY_CHANNELS */
	AMQP_ERROR_QUEUE_FULL, /**< AMQP_ERROR_QUEUE_FULL */
	AMQP_ERROR_FRAME_TOO_LARGE, /**< AMQP_ERROR_FRAME_TOO_LARGE */
	AMQP_ERROR_IO_ERROR, /**< AMQP_ERROR_IO_ERROR */
	AMQP_ERROR_PROTOCOL_ERROR, /**< AMQP_ERROR_PROTOCOL_ERROR */
	AMQP_ERROR_UNIMPLEMENTED, /**< AMQP_ERROR_UNIMPLEMENTED */
	AMQP_ERROR_IO_INTERRUPTED, /**< AMQP_ERROR_IO_INTERRUPTED */
	AMQP_ERROR_MAX, /**< AMQP_ERROR_MAX */
} AMQPStatus;

class AmqpCommon {
public:
	static void *AMQP_Calloc(
			size_t nmemb, size_t size);

	static void AMQP_Free(void *ptr);

	static int32 sendMethod(
			const amqp_connection_state_t& connectionState,
			const amqp_channel_t& channel,
			const amqp_method_number_t& methodId,
			void *decodedMethod);

	static int32 validateStatusRequired(
			const std::string& msg,
			const int32 status);

	static int32 validateStatus(
			const std::string& msg,
			const int32 status);

	static int32 validateRpcReply(
			const std::string& msg,
			const amqp_rpc_reply_t& rpcReply);

	static void strToAmqpBytes(
			const std::string& src,
			amqp_bytes_t& dest,
			Csetstr& cachedStrings);

	static void boolToAmqpBool(
			const bool src,
			amqp_boolean_t& dest);

	static void cpTableSafely(
			const amqp_table_t* src,
			amqp_table_t& dest);

	static void dumpMessageBody(
		const void *buffer,
		const size_t bufferLen);

private:
	static bool rowsEqual(
		int32 *row1,
		int32 *row2);

	static void dumpRow(
		const int32 count,
		const int32 numinrow,
		const int32 *chs);

	static const std::string& getString(
			const std::string& src,
			Csetstr& cachedStrings);

private:
	CAF_CM_DECLARE_NOCREATE (AmqpCommon);
};

}}

#endif /* AMQPCORE_AMQPCOMMON_H_ */
