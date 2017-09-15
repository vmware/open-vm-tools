/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/CAmqpAuthMechanism.h"
#include "amqpClient/CAmqpConnection.h"
#include "amqpClient/api/Address.h"
#include "amqpClient/api/CertInfo.h"
#include "AmqpConnection.h"

using namespace Caf::AmqpClient;

AMQPStatus AmqpConnection::AMQP_ConnectionCreate(
		SmartPtrCAmqpConnection& conn,
		const SmartPtrAddress& address,
		const SmartPtrCAmqpAuthMechanism& auth,
		const SmartPtrCertInfo& certInfo,
		const uint16 channelMax,
		const uint32 frameMax,
		const uint16 heartbeat,
		const uint16 retries,
		const uint16 secondsToWait) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionCreate");
	CAF_CM_VALIDATE_SMARTPTR(address);
	CAF_CM_VALIDATE_SMARTPTR(auth);

	conn.CreateInstance();
	return conn->connectionCreate(address, auth, certInfo,
			channelMax, frameMax, heartbeat, retries, secondsToWait);
}

AMQPStatus AmqpConnection::AMQP_ConnectionConnect(
		const SmartPtrCAmqpConnection& conn,
		const int32 flags) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionConnect");
	CAF_CM_VALIDATE_SMARTPTR(conn);

	return conn->connectionConnect(flags);
}

AMQPStatus AmqpConnection::AMQP_ConnectionOpenChannel(
		const SmartPtrCAmqpConnection& conn,
		SmartPtrCAmqpChannel& chan) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionOpenChannel");
	CAF_CM_VALIDATE_SMARTPTR(conn);

	return conn->channelOpen(chan);
}

AMQPStatus AmqpConnection::AMQP_ConnectionClose(
		const SmartPtrCAmqpConnection& conn) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionClose");
	CAF_CM_VALIDATE_SMARTPTR(conn);

	return conn->connectionClose();
}

AMQPStatus AmqpConnection::AMQP_ConnectionProcessIO(
		const SmartPtrCAmqpConnection& conn) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionProcessIO");
	CAF_CM_VALIDATE_SMARTPTR(conn);

	return conn->connectionProcessIO();
}

AMQPStatus AmqpConnection::AMQP_ConnectionWaitForIO(
		const SmartPtrCAmqpConnection& conn,
		const int32 timeout) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionWaitForIO");
	CAF_CM_VALIDATE_SMARTPTR(conn);

	return conn->connectionWaitForIO(timeout);
}

AMQPStatus AmqpConnection::AMQP_ConnectionGetState(
		const SmartPtrCAmqpConnection& conn,
		AMQPConnectionState *state) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionGetState");
	CAF_CM_VALIDATE_SMARTPTR(conn);
	CAF_CM_VALIDATE_PTR(state);

	return conn->connectionGetState(state);
}

AMQPStatus AmqpConnection::AMQP_ConnectionGetMaxChannels(
		const SmartPtrCAmqpConnection& conn,
		uint16 *channels) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionGetMaxChannels");
	CAF_CM_VALIDATE_SMARTPTR(conn);
	CAF_CM_VALIDATE_PTR(channels);

	return conn->connectionGetMaxChannels(channels);
}

AMQPStatus AmqpConnection::AMQP_ConnectionGetMaxFrameSize(
		const SmartPtrCAmqpConnection& conn,
		uint32 *frameSize) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionGetMaxFrameSize");
	CAF_CM_VALIDATE_SMARTPTR(conn);
	CAF_CM_VALIDATE_PTR(frameSize);

	return conn->connectionGetMaxFrameSize(frameSize);
}

AMQPStatus AmqpConnection::AMQP_ConnectionGetHeartbeatInterval(
		const SmartPtrCAmqpConnection& conn,
		uint16 *interval) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionGetHeartbeatInterval");
	CAF_CM_VALIDATE_SMARTPTR(conn);
	CAF_CM_VALIDATE_PTR(interval);

	return conn->connectionGetHeartbeatInterval(interval);
}

AMQPStatus AmqpConnection::AMQP_ConnectionGetLastError(
		const SmartPtrCAmqpConnection& conn,
		const char **error) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpConnection", "AMQP_ConnectionGetLastError");
	CAF_CM_VALIDATE_SMARTPTR(conn);
	CAF_CM_VALIDATE_PTR(error);

	return conn->connectionGetLastError(error);
}
