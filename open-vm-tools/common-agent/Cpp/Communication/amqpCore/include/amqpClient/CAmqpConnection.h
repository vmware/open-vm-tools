/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENT_CAMQPCONNECTION_H_
#define AMQPCLIENT_CAMQPCONNECTION_H_


#include "amqpClient/CAmqpChannel.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/CAmqpAuthMechanism.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/api/Address.h"
#include "amqpClient/api/CertInfo.h"

namespace Caf { namespace AmqpClient {

/** Connection states. */
typedef enum {
	AMQP_STATE_INITIALIZED = 0, /*!< New connection. */
	AMQP_STATE_CONNECTING, /*!< Connection in progress. */
	AMQP_STATE_CONNECTED, /*!< Connected. */
	AMQP_STATE_DISCONNECTING, /*!< Disconnection in process. */
	AMQP_STATE_DISCONNECTED, /*!< Disconnected. */
} AMQPConnectionState;

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Manages a set of channels for a connection.
 * Channels are indexed by channel number (<code><b>1.._channelMax</b></code>).
 */
class CAmqpConnection {
private:
	typedef std::deque<SmartPtrCAmqpFrame> CAmqpFrames;
	typedef std::map<amqp_channel_t, CAmqpFrames> CChannelFrames;
	CAF_DECLARE_SMART_POINTER(CChannelFrames);

	typedef std::set<amqp_channel_t> COpenChannels;
	CAF_DECLARE_SMART_POINTER(COpenChannels);

public:
	CAmqpConnection();
	virtual ~CAmqpConnection();

public:
	AMQPStatus connectionCreate(
			const SmartPtrAddress& address,
			const SmartPtrCAmqpAuthMechanism& auth,
			const SmartPtrCertInfo& certInfo,
			const uint16 channelMax,
			const uint32 frameMax,
			const uint16 heartbeat,
			const uint16 retries,
			const uint16 secondsToWait);

	AMQPStatus connectionConnect(
			const int32 flags);

	AMQPStatus connectionClose();

	AMQPStatus connectionProcessIO();

	AMQPStatus connectionWaitForIO(
			const int32 timeout);

	AMQPStatus connectionGetState(
			AMQPConnectionState *state);

	AMQPStatus connectionGetMaxChannels(
			uint16 *channels);

	AMQPStatus connectionGetMaxFrameSize(
			uint32 *frameSize);

	AMQPStatus connectionGetHeartbeatInterval(
			uint16 *interval);

	AMQPStatus connectionGetLastError(
			const char **error);

	AMQPStatus channelOpen(
			SmartPtrCAmqpChannel& chan);

	AMQPStatus channelClose(
			const amqp_channel_t& channel);

	AMQPStatus channelCloseOk(
			const amqp_channel_t& channel);

	AMQPStatus receive(
			const amqp_channel_t& channel,
			SmartPtrCAmqpFrame& frame,
			const int32 timeout);

	AMQPStatus basicAck(
			const amqp_channel_t& channel,
			const uint64 deliveryTag,
			const bool multiple);

	AMQPStatus basicCancel(
			const amqp_channel_t& channel,
			const std::string& consumerTag,
			const bool noWait);

	AMQPStatus basicConsume(
			const amqp_channel_t& channel,
			const std::string& queue,
			const std::string& consumerTag,
			const bool noLocal,
			const bool noAck,
			const bool exclusive,
			const bool noWait,
			const amqp_table_t *arguments);

	AMQPStatus basicGet(
			const amqp_channel_t& channel,
			const std::string& queue,
			const bool noAck);

	AMQPStatus basicPublish(
			const amqp_channel_t& channel,
			const std::string& exchange,
			const std::string& routingKey,
			const bool mandatory,
			const bool immediate,
			const amqp_basic_properties_t *basicProps,
			const SmartPtrCDynamicByteArray& body);

	AMQPStatus basicRecover(
			const amqp_channel_t& channel,
			const bool requeue);

	AMQPStatus basicQos(
			const amqp_channel_t& channel,
			const uint32 prefetchSize,
			const uint16 prefetchCount,
			const bool global);

	AMQPStatus exchangeDeclare(
			const amqp_channel_t& channel,
			const std::string& exchange,
			const std::string& type,
			const bool passive,
			const bool durable,
			const bool noWait,
			const amqp_table_t *arguments);

	AMQPStatus exchangeDelete(
			const amqp_channel_t& channel,
			const std::string& exchange,
			const bool ifUnused,
			const bool noWait);

	AMQPStatus queueBind(
			const amqp_channel_t& channel,
			const std::string& queue,
			const std::string& exchange,
			const std::string& routingKey,
			const bool noWait,
			const amqp_table_t *arguments);

	AMQPStatus queueDeclare(
			const amqp_channel_t& channel,
			const std::string& queue,
			const bool passive,
			const bool durable,
			const bool exclusive,
			const bool autoDelete,
			const bool noWait,
			const amqp_table_t *arguments);

	AMQPStatus queueDelete(
			const amqp_channel_t& channel,
			const std::string& queue,
			const bool ifUnused,
			const bool ifEmpty,
			const bool noWait);

	AMQPStatus queuePurge(
			const amqp_channel_t& channel,
			const std::string& queue,
			const bool noWait);

	AMQPStatus queueUnbind(
			const amqp_channel_t& channel,
			const std::string& queue,
			const std::string& exchange,
			const std::string& routingKey,
			const amqp_table_t *arguments);

private:
	AMQPStatus createConnection();
	AMQPStatus createSslConnection();

	AMQPStatus connectConnection();

	AMQPStatus closeConnection();

	AMQPStatus closeChannel(
			const amqp_channel_t& channel);

	int32 receiveFrame(
			const amqp_connection_state_t& connectionState,
			SmartPtrCAmqpFrame& frame) const;

	void addFrames(
			const CAmqpFrames& frames,
			const SmartPtrCChannelFrames& channelFrames) const;

	bool isDataAvail(
			const amqp_connection_state_t& connectionState) const;

	void validateOpenChannel(
			const amqp_channel_t& channel) const;

	void restartListener(
			const std::string& reason) const;

private:
	amqp_connection_state_t _connectionState;
	amqp_socket_t* _socket;
	amqp_channel_t _curChannel;
	AMQPConnectionState _connectionStateEnum;
	bool _isConnectionLost;
	int32 _lastStatus;

	SmartPtrCAmqpAuthMechanism _auth;
	uint16 _channelMax;
	uint32 _frameMax;
	uint16 _heartbeat;
	uint16 _retries;
	uint16 _secondsToWait;
	SmartPtrAddress _address;
	SmartPtrCertInfo _certInfo;
	SmartPtrCChannelFrames _channelFrames;

	Csetstr _cachedStrings;
	COpenChannels _openChannels;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CAmqpConnection);
};

}}

#endif /* AMQPCLIENT_CAMQPCONNECTION_H_ */
