/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "AmqpCommon.h"

using namespace Caf::AmqpClient;

void* AmqpCommon::AMQP_Calloc(
		size_t nmemb,
		size_t size) {
	return calloc(nmemb, size);
}

void AmqpCommon::AMQP_Free(
		void *ptr) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpCommon", "AMQP_Free");
	CAF_CM_VALIDATE_PTR(ptr);
	free(ptr);
}

int32 AmqpCommon::sendMethod(
		const amqp_connection_state_t& connectionState,
		const amqp_channel_t& channel,
		const amqp_method_number_t& methodId,
		void *decodedMethod) {
	return validateStatus(
			"amqp_send_method - " + CStringConv::toString<amqp_method_number_t>(methodId),
			amqp_send_method(
					connectionState,
					channel,
					methodId,
					decodedMethod));
}

int32 AmqpCommon::validateStatusRequired(
		const std::string& msg,
		const int32 status) {
	CAF_CM_STATIC_FUNC_LOG("AmqpCommon", "validateStatus");
	CAF_CM_VALIDATE_STRING(msg);

	if (status < 0) {
		CAF_CM_EXCEPTION_VA2(E_FAIL, "%s: %s",
			msg.c_str(), amqp_error_string2(status));
	}

	return status;
}

int32 AmqpCommon::validateStatus(
		const std::string& msg,
		const int32 status) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("AmqpCommon", "validateStatus");
	CAF_CM_VALIDATE_STRING(msg);

	if (status < 0) {
		CAF_CM_LOG_WARN_VA2("%s: %s",
			msg.c_str(), amqp_error_string2(status));
	}

	return status;
}

int32 AmqpCommon::validateRpcReply(
		const std::string& msg,
		const amqp_rpc_reply_t& rpcReply) {
	CAF_CM_STATIC_FUNC_LOG("AmqpCommon", "validateRpcReply");
	CAF_CM_VALIDATE_STRING(msg);

	int32 status = 0;
	switch (rpcReply.reply_type) {
		case AMQP_RESPONSE_NORMAL:
		break;

		case AMQP_RESPONSE_NONE:
			CAF_CM_EXCEPTION_VA1(E_FAIL, "%s: missing RPC reply type!", msg.c_str());
		break;

		case AMQP_RESPONSE_LIBRARY_EXCEPTION:
			status = rpcReply.library_error;
			CAF_CM_LOG_WARN_VA2("%s: %s", msg.c_str(),
				amqp_error_string2(rpcReply.library_error));
		break;

		case AMQP_RESPONSE_SERVER_EXCEPTION:
			switch (rpcReply.reply.id) {
				case AMQP_CONNECTION_CLOSE_METHOD: {
					amqp_connection_close_t *reply =
						reinterpret_cast<amqp_connection_close_t *>(rpcReply.reply.decoded);

					CAF_CM_EXCEPTION_VA4(E_FAIL,
						"%s: server connection error %d, message: %.*s", msg.c_str(),
						reply->reply_code, reply->reply_text.len,
						static_cast<char *>(reply->reply_text.bytes));

					break;
				}
				case AMQP_CHANNEL_CLOSE_METHOD: {
					amqp_connection_close_t *reply =
							reinterpret_cast<amqp_connection_close_t *>(rpcReply.reply.decoded);

					CAF_CM_EXCEPTION_VA4(E_FAIL,
						"%s: server channel error %d, message: %.*s", msg.c_str(),
						reply->reply_code, reply->reply_text.len,
						static_cast<char *>(reply->reply_text.bytes));
					break;
				}
				default:
					CAF_CM_EXCEPTION_VA2(E_FAIL,
						"%s: unknown server error, method id 0x%08X", msg.c_str(),
						rpcReply.reply.id);
				break;
		}
		break;
	}

	return status;
}

void AmqpCommon::strToAmqpBytes(
		const std::string& src,
		amqp_bytes_t& dest,
		Csetstr& cachedStrings) {
	if (src.empty()) {
		dest.len = 0;
		dest.bytes = NULL;
	} else {
		dest = amqp_cstring_bytes(getString(src, cachedStrings).c_str());
	}
}

const std::string& AmqpCommon::getString(
		const std::string& src,
		Csetstr& cachedStrings) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("AmqpCommon", "getString");
	CAF_CM_VALIDATE_STRING(src);

	if (cachedStrings.end() == cachedStrings.find(src)) {
		cachedStrings.insert(src);
	}

	return *(cachedStrings.find(src));
}

void AmqpCommon::boolToAmqpBool(
		const bool src,
		amqp_boolean_t& dest) {
	dest = src ? TRUE : FALSE;
}

void AmqpCommon::cpTableSafely(
		const amqp_table_t* src,
		amqp_table_t& dest) {
	dest = (NULL == src ? amqp_empty_table : *src);
}

void AmqpCommon::dumpMessageBody(
	const void *buffer,
	const size_t bufferLen) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("AmqpCommon", "dumpMessageBody");
	CAF_CM_VALIDATE_PTR(buffer);

	unsigned char *buf = (unsigned char *) buffer;
	int32 count = 0;
	int32 numinrow = 0;
	int32 chs[16];
	int32 oldchs[16] = {0};
	int32 showed_dots = 0;
	size_t i;

	for (i = 0; i < bufferLen; i++) {
		int32 ch = buf[i];

		if (numinrow == 16) {
			int32 i;

			if (rowsEqual(oldchs, chs)) {
				if (!showed_dots) {
					showed_dots = 1;
					CAF_CM_LOG_DEBUG_VA0(
						"          .. .. .. .. .. .. .. .. : .. .. .. .. .. .. .. ..");
				}
			} else {
				showed_dots = 0;
				dumpRow(count, numinrow, chs);
			}

			for (i=0; i<16; i++) {
				oldchs[i] = chs[i];
			}

			numinrow = 0;
		}

		count++;
		chs[numinrow++] = ch;
	}

	dumpRow(count, numinrow, chs);

	if (numinrow != 0) {
		printf("%08dX:\n", count);
	}
}

bool AmqpCommon::rowsEqual(
	int32 *row1,
	int32 *row2) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("AmqpCommon", "rowsEqual");
	CAF_CM_VALIDATE_PTR(row1);
	CAF_CM_VALIDATE_PTR(row2);

	int32 i = 0;
	for (; (i < 16) && (row1[i] == row2[i]); i++) {}
	return (i == 15);
}

void AmqpCommon::dumpRow(
	const int32 count,
	const int32 numinrow,
	const int32 *chs) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("AmqpCommon", "dumpRow");
	CAF_CM_VALIDATE_PTR(chs);

	static const int32 BUFF_SIZE = 100;
	char buff[BUFF_SIZE];
	std::string msg;

#ifdef WIN32
	sprintf_s(buff, BUFF_SIZE, "%08lX:", count - numinrow);
#else
	snprintf(buff, BUFF_SIZE, "%08dX:", count - numinrow);
#endif
	msg += buff;

	if (numinrow > 0) {
		for (int32 i = 0; i < numinrow; i++) {
			if (i == 8) {
				msg += " :";
			}
#ifdef WIN32
			sprintf_s(buff, BUFF_SIZE, " %02X", chs[i]);
#else
			snprintf(buff, BUFF_SIZE, " %02X", chs[i]);
#endif
			msg += buff;
		}
		for (int32 i = numinrow; i < 16; i++) {
			if (i == 8) {
				msg += " :";
			}
			msg += "   ";
		}
		msg += "  ";
		for (int32 i = 0; i < numinrow; i++) {
			if (isprint(chs[i])) {
#ifdef WIN32
			   sprintf_s(buff, BUFF_SIZE, "%c", chs[i]);
#else
			   snprintf(buff, BUFF_SIZE, "%c", chs[i]);
#endif
				msg += buff;
			} else {
				msg += ".";
			}
		}
	}

	CAF_CM_LOG_DEBUG_VA1("row: %s", msg.c_str());
}
