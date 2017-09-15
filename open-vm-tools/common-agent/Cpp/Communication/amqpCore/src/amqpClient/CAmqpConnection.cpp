/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/CAmqpAuthMechanism.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/api/Address.h"
#include "amqpClient/api/CertInfo.h"
#include "amqpClient/CAmqpConnection.h"
#include "Exception/CCafException.h"

using namespace Caf::AmqpClient;

CAmqpConnection::CAmqpConnection() :
	_connectionState(NULL),
	_socket(NULL),
	_curChannel(0),
	_connectionStateEnum(AMQP_STATE_DISCONNECTED),
	_isConnectionLost(false),
	_lastStatus(0),
	_channelMax(0),
	_frameMax(0),
	_heartbeat(0),
	_retries(0),
	_secondsToWait(0),
	CAF_CM_INIT_LOG("CAmqpConnection") {
	CAF_CM_INIT_THREADSAFE;
}

CAmqpConnection::~CAmqpConnection() {
	CAF_CM_LOCK_UNLOCK;
	if (NULL != _connectionState) {
		closeConnection();
	}
}

AMQPStatus CAmqpConnection::connectionCreate(
		const SmartPtrAddress& address,
		const SmartPtrCAmqpAuthMechanism& auth,
		const SmartPtrCertInfo& certInfo,
		const uint16 channelMax,
		const uint32 frameMax,
		const uint16 heartbeat,
		const uint16 retries,
		const uint16 secondsToWait) {
	CAF_CM_FUNCNAME("connectionCreate");
	CAF_CM_VALIDATE_SMARTPTR(address);
	CAF_CM_VALIDATE_SMARTPTR(auth);

	CAF_CM_LOCK_UNLOCK;

	_address = address;
	_auth = auth;
	_certInfo = certInfo;
	_channelMax = channelMax;
	_frameMax = frameMax;
	_heartbeat = heartbeat;
	_channelFrames.CreateInstance();
	_retries = retries;
	_secondsToWait = secondsToWait;

	AMQPStatus rc = AMQP_ERROR_OK;
	switch(_address->getProtocol()) {
		case PROTOCOL_AMQP:
			rc = createConnection();
		break;
		case PROTOCOL_AMQPS:
			rc = createSslConnection();
		break;
		case PROTOCOL_TUNNEL:
			rc = createSslConnection();
		break;
		default:
			CAF_CM_EXCEPTION_VA1(E_FAIL,
					"Unknown protocol - %d", _address->getProtocol());
	}

	return rc;
}

AMQPStatus CAmqpConnection::connectionConnect(
		const int32 flags) {
	CAF_CM_LOCK_UNLOCK;
	return connectConnection();
}

AMQPStatus CAmqpConnection::connectionClose() {
	CAF_CM_LOCK_UNLOCK;
	return closeConnection();
}

AMQPStatus CAmqpConnection::connectionProcessIO() {
	//TODO: Is this call necessary anymore?
	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::connectionWaitForIO(
		int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("connectionWaitForIO");

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);

	AMQPStatus rc = AMQP_ERROR_TIMEOUT;
	if (isDataAvail(_connectionState)) {
		rc = AMQP_ERROR_OK;
	} else {
		if (timeout > 0) {
			{
				CAF_CM_UNLOCK_LOCK;
				CThreadUtils::sleep(timeout);
			}
			if (isDataAvail(_connectionState)) {
				rc = AMQP_ERROR_OK;
			}
		}
	}

	return rc;
}

AMQPStatus CAmqpConnection::connectionGetState(
		AMQPConnectionState *state) {
	CAF_CM_FUNCNAME_VALIDATE("connectionGetState");
	CAF_CM_VALIDATE_PTR(state);

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);

	*state = _connectionStateEnum;

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::connectionGetMaxChannels(
		uint16 *channels) {
	CAF_CM_FUNCNAME_VALIDATE("connectionGetMaxChannels");
	CAF_CM_VALIDATE_PTR(channels);

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);

	*channels = amqp_get_channel_max(_connectionState);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::connectionGetMaxFrameSize(
		uint32 *frameSize) {
	CAF_CM_FUNCNAME_VALIDATE("connectionGetMaxFrameSize");
	CAF_CM_VALIDATE_PTR(frameSize);

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);

	*frameSize = amqp_get_frame_max(_connectionState);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::connectionGetHeartbeatInterval(
		uint16 *interval) {
	CAF_CM_FUNCNAME_VALIDATE("connectionGetHeartbeatInterval");
	CAF_CM_VALIDATE_PTR(interval);

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);

	*interval = amqp_get_heartbeat(_connectionState);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::connectionGetLastError(
		const char **error) {
	CAF_CM_FUNCNAME_VALIDATE("connectionGetLastError");
	CAF_CM_VALIDATE_PTR(error);

	CAF_CM_LOCK_UNLOCK;

	*error = amqp_error_string2(_lastStatus);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::channelClose(
		const amqp_channel_t& channel) {
	CAF_CM_FUNCNAME_VALIDATE("channelClose");

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	_openChannels.erase(channel);

	return closeChannel(channel);
}

AMQPStatus CAmqpConnection::channelCloseOk(
		const amqp_channel_t& channel) {
	CAF_CM_FUNCNAME_VALIDATE("channelCloseOk");

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	amqp_channel_close_ok_t method = {};
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_CHANNEL_CLOSE_OK_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::receive(
		const amqp_channel_t& channel,
		SmartPtrCAmqpFrame& frame,
		const int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("receive");

	frame = SmartPtrCAmqpFrame();
	AMQPStatus rc = AMQP_ERROR_OK;

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	//amqp_maybe_release_buffers(_connectionState);

	int32 status = AMQP_STATUS_OK;
	CChannelFrames::iterator iter = _channelFrames->find(channel);
	if ((_channelFrames->end() == iter) || iter->second.empty()) {
		CAmqpFrames frames;
		SmartPtrCAmqpFrame frame;
		status = receiveFrame(_connectionState, frame);
		if ((AMQP_STATUS_TIMEOUT == status) && (timeout > 0)) {
			CAF_CM_UNLOCK_LOCK;
			CThreadUtils::sleep(timeout);
		}

		while (AMQP_STATUS_OK == status) {
			CAF_CM_VALIDATE_SMARTPTR(frame);
			frames.push_back(frame);
			status = receiveFrame(_connectionState, frame);
		}
		_lastStatus = status;

		addFrames(frames, _channelFrames);
	}

	switch (status) {
		case AMQP_STATUS_OK:
		case AMQP_STATUS_TIMEOUT: {
			iter = _channelFrames->find(channel);
			if ((_channelFrames->end() == iter) || iter->second.empty()) {
				rc = AMQP_ERROR_TIMEOUT;
			} else {
				frame = iter->second.front();
				iter->second.pop_front();
			}
		}
		break;

		case AMQP_STATUS_CONNECTION_CLOSED: {
			if (! _isConnectionLost) {
				CAF_CM_LOG_ERROR_VA1("Connection closed... restarting listener - %s",
					amqp_error_string2(status));
				_isConnectionLost = true;
				restartListener(amqp_error_string2(status));
			}
			rc = AMQP_ERROR_IO_INTERRUPTED;
		}
		break;

		case AMQP_STATUS_SOCKET_ERROR: { // Enhance the logic to restart listener after certain number of errors.
			if (! _isConnectionLost) {
				CAF_CM_LOG_ERROR_VA1("SOCKET_ERROR... restarting listener - %s",
					amqp_error_string2(status));
				_isConnectionLost = true;
				restartListener(amqp_error_string2(status));
			}
			rc = AMQP_ERROR_IO_INTERRUPTED;
		}
		break;

		default: {
			CAF_CM_LOG_ERROR_VA1("Received error status - %s",
				amqp_error_string2(status));
		}
		break;
	}

	if (! frame.IsNull()) {
		frame->log("Returned");
	}

	return rc;
}

AMQPStatus CAmqpConnection::channelOpen(
		SmartPtrCAmqpChannel& chan) {
	CAF_CM_FUNCNAME("channelOpen");

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);

	_curChannel++;
	const amqp_channel_t channel = _curChannel;

	CAF_CM_LOG_DEBUG_VA1("Calling amqp_channel_open - %d", channel);

	amqp_channel_open_t method = {};
	method.out_of_band = amqp_empty_bytes;
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_CHANNEL_OPEN_METHOD, &method);

	chan.CreateInstance();
	chan->initialize(this, channel);

	if (! _openChannels.insert(channel).second) {
		CAF_CM_EXCEPTION_VA1(E_FAIL,
				"Inserted duplicated channel - %d", channel);
	}

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::basicAck(
		const amqp_channel_t& channel,
		const uint64 deliveryTag,
		const bool multiple) {
	CAF_CM_FUNCNAME_VALIDATE("basicAck");

	CAF_CM_LOG_DEBUG_VA1(
			"Calling amqp_basic_ack - channel: %d", channel);

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	_lastStatus = AmqpCommon::validateStatus(
			"amqp_basic_ack",
			amqp_basic_ack(
					_connectionState,
					channel,
					deliveryTag,
					multiple ? TRUE : FALSE));

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::basicCancel(
		const amqp_channel_t& channel,
		const std::string& consumerTag,
		const bool noWait) {
	CAF_CM_FUNCNAME_VALIDATE("basicCancel");
	CAF_CM_VALIDATE_STRING(consumerTag);

	CAF_CM_LOG_DEBUG_VA2(
			"Calling amqp_basic_cancel - channel: %d, consumerTag: %s",
			channel, consumerTag.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	amqp_basic_cancel_t method = {};
	AmqpCommon::strToAmqpBytes(consumerTag, method.consumer_tag, _cachedStrings);
	AmqpCommon::boolToAmqpBool(noWait, method.nowait);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_BASIC_CANCEL_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::basicConsume(
		const amqp_channel_t& channel,
		const std::string& queue,
		const std::string& consumerTag,
		const bool noLocal,
		const bool noAck,
		const bool exclusive,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("basicConsume");
	CAF_CM_VALIDATE_STRING(queue);

	CAF_CM_LOG_DEBUG_VA3(
			"Calling amqp_basic_consume - channel: %d, queue: %s, consumerTag: %s",
			channel, queue.c_str(),
			(consumerTag.empty() ? "NULL" : consumerTag.c_str()));

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	amqp_basic_consume_t method = {};
	AmqpCommon::strToAmqpBytes(queue, method.queue, _cachedStrings);
	AmqpCommon::strToAmqpBytes(consumerTag, method.consumer_tag, _cachedStrings);
	AmqpCommon::boolToAmqpBool(noLocal, method.no_local);
	AmqpCommon::boolToAmqpBool(noAck, method.no_ack);
	AmqpCommon::boolToAmqpBool(exclusive, method.exclusive);
	AmqpCommon::boolToAmqpBool(noWait, method.nowait);
	AmqpCommon::cpTableSafely(arguments, method.arguments);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_BASIC_CONSUME_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::basicGet(
		const amqp_channel_t& channel,
		const std::string& queue,
		const bool noAck) {
	CAF_CM_FUNCNAME_VALIDATE("basicGet");
	CAF_CM_VALIDATE_STRING(queue);

	CAF_CM_LOG_DEBUG_VA2(
			"Calling amqp_basic_get - channel: %d, queue: %s",
			channel, queue.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	AmqpCommon::validateRpcReply(
			"amqp_basic_get",
			amqp_basic_get(
					_connectionState,
					channel,
					amqp_cstring_bytes(queue.c_str()),
					noAck ? TRUE : FALSE));

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::basicPublish(
		const amqp_channel_t& channel,
		const std::string& exchange,
		const std::string& routingKey,
		const bool mandatory,
		const bool immediate,
		const amqp_basic_properties_t *basicProps,
		const SmartPtrCDynamicByteArray& body) {
	CAF_CM_FUNCNAME_VALIDATE("basicPublish");
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(routingKey);
	CAF_CM_VALIDATE_PTR(basicProps);
	CAF_CM_VALIDATE_SMARTPTR(body);

	CAF_CM_LOG_DEBUG_VA3(
			"Calling amqp_basic_publish - channel: %d, exchange: %s, routingKey: %s",
			channel, exchange.c_str(), routingKey.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	amqp_bytes_t bodyRaw;
	bodyRaw.bytes = body->getNonConstPtr();
	bodyRaw.len = body->getByteCount();

	_lastStatus = AmqpCommon::validateStatus(
			"amqp_basic_publish",
			amqp_basic_publish(
					_connectionState,
					channel,
					amqp_cstring_bytes(exchange.c_str()),
					amqp_cstring_bytes(routingKey.c_str()),
					mandatory ? TRUE : FALSE,
					immediate ? TRUE : FALSE,
					basicProps,
					bodyRaw));

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::basicRecover(
		const amqp_channel_t& channel,
		const bool requeue) {
	CAF_CM_FUNCNAME_VALIDATE("basicRecover");

	CAF_CM_LOG_DEBUG_VA1(
			"Calling amqp_basic_recover - channel: %d", channel);

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	amqp_basic_recover_t method = {};
	AmqpCommon::boolToAmqpBool(requeue, method.requeue);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_BASIC_RECOVER_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::basicQos(
		const amqp_channel_t& channel,
		const uint32 prefetchSize,
		const uint16 prefetchCount,
		const bool global) {
	CAF_CM_FUNCNAME_VALIDATE("basicQos");

	CAF_CM_LOG_DEBUG_VA1(
			"Calling amqp_basic_qos - channel: %d", channel);

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	amqp_basic_qos_t method = {};
	method.prefetch_size = prefetchSize;
	method.prefetch_count = prefetchCount;
	AmqpCommon::boolToAmqpBool(global, method.global);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_BASIC_QOS_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::exchangeDeclare(
		const amqp_channel_t& channel,
		const std::string& exchange,
		const std::string& type,
		const bool passive,
		const bool durable,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("exchangeDeclare");
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(type);

	CAF_CM_LOG_DEBUG_VA3(
			"Calling amqp_exchange_declare - channel: %d, exchange: %s, type: %s",
			channel, exchange.c_str(), type.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	const bool autoDelete = false;

	amqp_exchange_declare_t method = {};
	AmqpCommon::strToAmqpBytes(exchange, method.exchange, _cachedStrings);
	AmqpCommon::strToAmqpBytes(type, method.type, _cachedStrings);
	AmqpCommon::boolToAmqpBool(passive, method.passive);
	AmqpCommon::boolToAmqpBool(durable, method.durable);
	AmqpCommon::boolToAmqpBool(noWait, method.nowait);
	AmqpCommon::cpTableSafely(arguments, method.arguments);
	AmqpCommon::boolToAmqpBool(autoDelete, method.auto_delete);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_EXCHANGE_DECLARE_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::exchangeDelete(
		const amqp_channel_t& channel,
		const std::string& exchange,
		const bool ifUnused,
		const bool noWait) {
	CAF_CM_FUNCNAME_VALIDATE("exchangeDelete");
	CAF_CM_VALIDATE_STRING(exchange);

	CAF_CM_LOG_DEBUG_VA2(
			"Calling amqp_exchange_delete - channel: %d, exchange: %s",
			channel, exchange.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	amqp_exchange_delete_t method = {};
	AmqpCommon::strToAmqpBytes(exchange, method.exchange, _cachedStrings);
	AmqpCommon::boolToAmqpBool(ifUnused, method.if_unused);
	AmqpCommon::boolToAmqpBool(noWait, method.nowait);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_EXCHANGE_DELETE_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::queueBind(
		const amqp_channel_t& channel,
		const std::string& queue,
		const std::string& exchange,
		const std::string& routingKey,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("queueBind");
	CAF_CM_VALIDATE_STRING(queue);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(routingKey);

	CAF_CM_LOG_DEBUG_VA4(
			"Calling amqp_queue_bind - channel: %d, queue: %s, exchange: %s, routingKey: %s",
			channel, queue.c_str(), exchange.c_str(), routingKey.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	CAF_CM_VALIDATE_BOOL(_address->getProtocol() != PROTOCOL_TUNNEL);
	validateOpenChannel(channel);

	amqp_queue_bind_t method = {};
	AmqpCommon::strToAmqpBytes(queue, method.queue, _cachedStrings);
	AmqpCommon::strToAmqpBytes(exchange, method.exchange, _cachedStrings);
	AmqpCommon::strToAmqpBytes(routingKey, method.routing_key, _cachedStrings);
	AmqpCommon::boolToAmqpBool(noWait, method.nowait);
	AmqpCommon::cpTableSafely(arguments, method.arguments);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_QUEUE_BIND_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::queueDeclare(
		const amqp_channel_t& channel,
		const std::string& queue,
		const bool passive,
		const bool durable,
		const bool exclusive,
		const bool autoDelete,
		const bool noWait,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("queueDeclare");
	CAF_CM_VALIDATE_STRING(queue);

	CAF_CM_LOG_DEBUG_VA2(
			"Calling amqp_queue_declare - channel: %d, queue: %s",
			channel, queue.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	CAF_CM_VALIDATE_BOOL(_address->getProtocol() != PROTOCOL_TUNNEL);
	validateOpenChannel(channel);

	amqp_queue_declare_t method = {};
	AmqpCommon::strToAmqpBytes(queue, method.queue, _cachedStrings);
	AmqpCommon::boolToAmqpBool(passive, method.passive);
	AmqpCommon::boolToAmqpBool(durable, method.durable);
	AmqpCommon::boolToAmqpBool(exclusive, method.exclusive);
	AmqpCommon::boolToAmqpBool(autoDelete, method.auto_delete);
	AmqpCommon::boolToAmqpBool(noWait, method.nowait);
	AmqpCommon::cpTableSafely(arguments, method.arguments);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_QUEUE_DECLARE_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::queueDelete(
		const amqp_channel_t& channel,
		const std::string& queue,
		const bool ifUnused,
		const bool ifEmpty,
		const bool noWait) {
	CAF_CM_FUNCNAME_VALIDATE("queueDelete");
	CAF_CM_VALIDATE_STRING(queue);

	CAF_CM_LOG_DEBUG_VA2(
			"Calling amqp_queue_delete - channel: %d, queue: %s",
			channel, queue.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	CAF_CM_VALIDATE_BOOL(_address->getProtocol() != PROTOCOL_TUNNEL);
	validateOpenChannel(channel);

	amqp_queue_delete_t method = {};
	AmqpCommon::strToAmqpBytes(queue, method.queue, _cachedStrings);
	AmqpCommon::boolToAmqpBool(ifUnused, method.if_unused);
	AmqpCommon::boolToAmqpBool(ifEmpty, method.if_empty);
	AmqpCommon::boolToAmqpBool(noWait, method.nowait);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_QUEUE_DELETE_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::queuePurge(
		const amqp_channel_t& channel,
		const std::string& queue,
		const bool noWait) {
	CAF_CM_FUNCNAME_VALIDATE("queuePurge");
	CAF_CM_VALIDATE_STRING(queue);

	CAF_CM_LOG_DEBUG_VA2(
			"Calling amqp_queue_purge - channel: %d, queue: %s",
			channel, queue.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	validateOpenChannel(channel);

	amqp_queue_purge_t method = {};
	AmqpCommon::strToAmqpBytes(queue, method.queue, _cachedStrings);
	AmqpCommon::boolToAmqpBool(noWait, method.nowait);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_QUEUE_PURGE_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::queueUnbind(
		const amqp_channel_t& channel,
		const std::string& queue,
		const std::string& exchange,
		const std::string& routingKey,
		const amqp_table_t *arguments) {
	CAF_CM_FUNCNAME_VALIDATE("queueUnbind");
	CAF_CM_VALIDATE_STRING(queue);
	CAF_CM_VALIDATE_STRING(exchange);
	CAF_CM_VALIDATE_STRING(routingKey);

	CAF_CM_LOG_DEBUG_VA4(
			"Calling amqp_queue_unbind - channel: %d, queue: %s, exchange: %s, routingKey: %s",
			channel, queue.c_str(), exchange.c_str(), routingKey.c_str());

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);
	CAF_CM_VALIDATE_BOOL(_address->getProtocol() != PROTOCOL_TUNNEL);
	validateOpenChannel(channel);

	amqp_queue_unbind_t method = {};
	AmqpCommon::strToAmqpBytes(queue, method.queue, _cachedStrings);
	AmqpCommon::strToAmqpBytes(exchange, method.exchange, _cachedStrings);
	AmqpCommon::strToAmqpBytes(routingKey, method.routing_key, _cachedStrings);
	AmqpCommon::cpTableSafely(arguments, method.arguments);
	AmqpCommon::sendMethod(_connectionState, channel,
			AMQP_QUEUE_UNBIND_METHOD, &method);

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::createConnection() {
	CAF_CM_FUNCNAME_VALIDATE("createConnection");
	CAF_CM_VALIDATE_NULLPTR(_connectionState);
	CAF_CM_VALIDATE_NULLPTR(_socket);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_DISCONNECTED);

	CAF_CM_LOG_DEBUG_VA0(
			"Calling amqp_new_connection/amqp_tcp_socket_new");

	_connectionState = amqp_new_connection();
	CAF_CM_VALIDATE_PTR(_connectionState);

	_socket = amqp_tcp_socket_new(_connectionState);
	CAF_CM_VALIDATE_PTR(_socket);

	_connectionStateEnum = AMQP_STATE_INITIALIZED;

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::createSslConnection() {
	CAF_CM_FUNCNAME_VALIDATE("createSslConnection");
	CAF_CM_VALIDATE_NULLPTR(_connectionState);
	CAF_CM_VALIDATE_NULLPTR(_socket);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_DISCONNECTED);
	CAF_CM_VALIDATE_SMARTPTR(_certInfo);

	CAF_CM_LOG_DEBUG_VA0(
			"Calling amqp_new_connection/amqp_ssl_socket_new");

	_connectionState = amqp_new_connection();
	CAF_CM_VALIDATE_PTR(_connectionState);

	_socket = amqp_ssl_socket_new(_connectionState);
	CAF_CM_VALIDATE_PTR(_socket);

	CAF_CM_LOG_DEBUG_VA0(
			"Disable peer verification (amqp_ssl_socket_set_verify_peer)");

	amqp_ssl_socket_set_verify_peer(_socket, false);

	CAF_CM_LOG_DEBUG_VA0(
			"Disable hostname verification (amqp_ssl_socket_set_verify_hostname)");

	amqp_ssl_socket_set_verify_hostname(_socket, false);

	CAF_CM_LOG_DEBUG_VA0(
			"Setting ssl protocol >= 1.2 (amqp_ssl_socket_set_ssl_versions)");

	amqp_ssl_socket_set_ssl_versions (_socket, AMQP_TLSv1_2, AMQP_TLSvLATEST);

	CAF_CM_LOG_DEBUG_VA1(
			"Calling amqp_ssl_socket_set_cacert - caCertPath: %s",
			_certInfo->getCaCertPath().c_str());

	_lastStatus = AmqpCommon::validateStatusRequired(
			"amqp_ssl_socket_set_cacert",
			amqp_ssl_socket_set_cacert(
					_socket,
					_certInfo->getCaCertPath().c_str()));

	CAF_CM_LOG_DEBUG_VA2(
			"Calling amqp_ssl_socket_set_key - clientCert: %s, clientKey: %s",
			_certInfo->getClientCertPath().c_str(), _certInfo->getClientKeyPath().c_str());

	_lastStatus = AmqpCommon::validateStatusRequired(
			"amqp_ssl_socket_set_key",
			amqp_ssl_socket_set_key(
					_socket,
					_certInfo->getClientCertPath().c_str(),
					_certInfo->getClientKeyPath().c_str()));

	_connectionStateEnum = AMQP_STATE_INITIALIZED;

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::connectConnection() {
	CAF_CM_FUNCNAME("connectConnection");
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_PTR(_socket);
	CAF_CM_VALIDATE_SMARTPTR(_address);
	CAF_CM_VALIDATE_SMARTPTR(_auth);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_INITIALIZED);

	CAF_CM_LOG_DEBUG_VA3(
			"Calling amqp_socket_open_noblock - protocol: %s, host: %s, port: %d",
			_address->getProtocolStr().c_str(), _address->getHost().c_str(),
			_address->getPort());

	struct timeval timeout;
	struct timeval *pTimeout = NULL;

	if (_secondsToWait) {
		memset(&timeout, 0, sizeof(timeval));
		timeout.tv_sec = _secondsToWait;
		pTimeout = &timeout;
	}

	uint16 retries = _retries;
	do {
		CAF_CM_LOG_DEBUG_VA2(
				"Calling amqp_socket_open_noblock - retries: %d, wait: %d",
				retries, _secondsToWait);
		if (retries == 1 ) {
			// Throw an exception if the last attempt fails
			_lastStatus = AmqpCommon::validateStatusRequired(
					"amqp_socket_open_noblock",
					amqp_socket_open_noblock(
							_socket,
							_address->getHost().c_str(),
							_address->getPort(),
							pTimeout));
		}
		else {
			_lastStatus = AmqpCommon::validateStatus(
					"amqp_socket_open_noblock",
					amqp_socket_open_noblock(
							_socket,
							_address->getHost().c_str(),
							_address->getPort(),
							pTimeout));
		}
	} while (_lastStatus != AMQP_STATUS_OK && --retries);

	CAF_CM_LOG_DEBUG_VA2(
			"Calling amqp_login - virtualHost: %s, username: %s",
			_address->getVirtualHost().c_str(), _auth->getUsername().c_str());

	_lastStatus = AmqpCommon::validateRpcReply(
			"amqp_login",
			amqp_login(
					_connectionState,
					_address->getVirtualHost().c_str(),
					_channelMax,
					_frameMax,
					_heartbeat,
					AMQP_SASL_METHOD_PLAIN,
					_auth->getUsername().c_str(),
					_auth->getPassword().empty() ? "" :_auth->getPassword().c_str()));

	CAF_CM_LOG_DEBUG_VA2(
			"Called amqp_login - virtualHost: %s, username: %s",
			_address->getVirtualHost().c_str(), _auth->getUsername().c_str());

	if (0 == _lastStatus) {
		_connectionStateEnum = AMQP_STATE_CONNECTED;
	} else {
		CAF_CM_EXCEPTION_VA3(E_FAIL,
				"Failed to login - error: %s, vhost: %s, username: %s",
				amqp_error_string2(_lastStatus),
				_address->getVirtualHost().c_str(),
				_auth->getUsername().c_str());
	}

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::closeConnection() {
	CAF_CM_FUNCNAME_VALIDATE("closeConnection");

	if ((_connectionStateEnum == AMQP_STATE_INITIALIZED) ||
			(_connectionStateEnum == AMQP_STATE_CONNECTING) ||
			(_connectionStateEnum == AMQP_STATE_CONNECTED)) {
		CAF_CM_VALIDATE_SMARTPTR(_address);
		CAF_CM_VALIDATE_PTR(_connectionState);

		CAF_CM_LOG_DEBUG_VA4(
				"Calling amqp_connection_close/amqp_destroy_connection - protocol: %s, host: %s, port: %d, virtualHost: %s",
				_address->getProtocolStr().c_str(), _address->getHost().c_str(),
				_address->getPort(), _address->getVirtualHost().c_str());

		for (COpenChannels::const_iterator iter = _openChannels.begin();
				iter != _openChannels.end(); iter++) {
			const amqp_channel_t channel = *iter;
			closeChannel(channel);
		}

		AmqpCommon::validateRpcReply(
				"amqp_connection_close",
				amqp_connection_close(_connectionState, AMQP_REPLY_SUCCESS));

		_lastStatus = AmqpCommon::validateStatus(
				"amqp_destroy_connection",
				amqp_destroy_connection(_connectionState));
	}

	_connectionState = NULL;
	_socket = NULL;
	_curChannel = 0;
	_connectionStateEnum = AMQP_STATE_DISCONNECTED;
	_channelFrames = NULL;
	_openChannels.clear();

	return AMQP_ERROR_OK;
}

AMQPStatus CAmqpConnection::closeChannel(
		const amqp_channel_t& channel) {
	CAF_CM_FUNCNAME_VALIDATE("closeChannel");
	CAF_CM_VALIDATE_PTR(_connectionState);
	CAF_CM_VALIDATE_BOOL(_connectionStateEnum == AMQP_STATE_CONNECTED);

	CAF_CM_LOG_DEBUG_VA1("Calling amqp_channel_close - channel: %d", channel);

	AmqpCommon::validateRpcReply(
			"amqp_channel_close",
			amqp_channel_close(
					_connectionState,
					channel,
					AMQP_REPLY_SUCCESS));

	return AMQP_ERROR_OK;
}

int32 CAmqpConnection::receiveFrame(
		const amqp_connection_state_t& connectionState,
		SmartPtrCAmqpFrame& frame) const {
	CAF_CM_FUNCNAME_VALIDATE("receiveFrame");
	CAF_CM_VALIDATE_PTR(connectionState);

	frame = SmartPtrCAmqpFrame();

	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	amqp_frame_t decoded_frame;
	int32 status = amqp_simple_wait_frame_noblock(
			connectionState,
			&decoded_frame,
			&tv);

	if (AMQP_STATUS_OK == status) {
		frame.CreateInstance();
		frame->initialize(decoded_frame);
		frame->log("Received");

		if (AMQP_FRAME_METHOD == frame->getFrameType()) {
			const amqp_method_t* amqpMethod = frame->getPayloadAsMethod();
			if (AMQP_CONNECTION_CLOSE_METHOD == amqpMethod->id) {
				status = AMQP_STATUS_CONNECTION_CLOSED;
			}
		}
	}

	return status;
}

void CAmqpConnection::addFrames(
		const CAmqpFrames& frames,
		const SmartPtrCChannelFrames& channelFrames) const {
	for (CAmqpFrames::const_iterator iter = frames.begin();
			iter != frames.end(); iter++) {
		const SmartPtrCAmqpFrame frame = *iter;

		CChannelFrames::iterator fndIter = channelFrames->find(frame->getChannel());
		if (channelFrames->end() == fndIter) {
			channelFrames->insert(std::make_pair(
					frame->getChannel(), CAmqpFrames()));
			fndIter = channelFrames->find(frame->getChannel());
		}
		fndIter->second.push_back(frame);
	}
}

void CAmqpConnection::validateOpenChannel(
		const amqp_channel_t& channel) const {
	CAF_CM_FUNCNAME("validateOpenChannel");

	if (_openChannels.end() == _openChannels.find(channel)) {
		CAF_CM_EXCEPTION_VA1(E_FAIL,
				"Channel not found - %d", channel);
	}
}

bool CAmqpConnection::isDataAvail(
		const amqp_connection_state_t& connectionState) const {
	CAF_CM_FUNCNAME_VALIDATE("isDataAvail");
	CAF_CM_VALIDATE_PTR(connectionState);

	return (amqp_frames_enqueued(connectionState)) ||
			(amqp_data_in_buffer(connectionState));
}

void CAmqpConnection::restartListener(
		const std::string& reason) const {
	CAF_CM_FUNCNAME_VALIDATE("restartListener");
	CAF_CM_VALIDATE_STRING(reason);

	const std::string monitorDir = AppConfigUtils::getRequiredString("monitor_dir");
	const std::string monitorDirExp = CStringUtils::expandEnv(monitorDir);

	FileSystemUtils::saveTextFile(monitorDirExp, "restartListener.txt", reason);
}
