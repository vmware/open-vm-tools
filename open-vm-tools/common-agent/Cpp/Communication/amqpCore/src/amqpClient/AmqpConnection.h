/*
 *  Created on: Oct 7, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2014-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCORE_AMQPCONNECTION_H_
#define AMQPCORE_AMQPCONNECTION_H_

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/CAmqpAuthMechanism.h"
#include "amqpClient/CAmqpConnection.h"
#include "amqpClient/api/Address.h"
#include "amqpClient/api/CertInfo.h"

namespace Caf { namespace AmqpClient {

/** Default maximum number of channels. */
static const uint16 AMQP_CHANNEL_MAX_DEFAULT = 0; /* Unlimited. */

/** Default maximum frame size. */
static const uint32 AMQP_FRAME_MAX_DEFAULT = 131072; /* 128kB */

/** Default heartbeat frequency. */
static const uint16 AMQP_HEARTBEAT_DEFAULT = 0; /* No heartbeat. */

/** AMQP connection internal state details. */
typedef enum AMQPConnectionInternalState {
	AMQP_CONNECTION_INITIALIZED = 0,
	AMQP_CONNECTION_CONNECTING,
	AMQP_CONNECTION_WAITING_FOR_START,
	AMQP_CONNECTION_WAITING_FOR_SECURE,
	AMQP_CONNECTION_WAITING_FOR_TUNE,
	AMQP_CONNECTION_WAITING_FOR_OPEN_OK,
	AMQP_CONNECTION_OPEN,
	AMQP_CONNECTION_WAITING_FOR_CLOSE_OK,
	AMQP_CONNECTION_SENT_CLOSE_OK,
	AMQP_CONNECTION_CLOSED
} AMQPConnectionInternalState;

/** Close the socket when the connection is closed. */
#define AMQP_CONNECTION_FLAG_CLOSE_SOCKET (1 << 0)
/** Don't lock the connection against multi-threaded applications. */
#define AMQP_CONNECTION_FLAG_NO_LOCK      (1 << 1)
/** Don't retry I/O when interrupted by signals. */
#define AMQP_CONNECTION_FLAG_NO_IO_RETRY  (1 << 2)

/** Mode to poll the socket. */
typedef enum {
	AMQP_WANT_READ = 0x1, /*!< Poll socket for readability. */
	AMQP_WANT_WRITE = 0x2, /*!< Poll socket for writability. */
	AMQP_POLL_NO_IO_RETRY = 0x4, /*!< Don't retry I/O on EINTR. */
} AMQPPollFlags;

class AmqpConnection {
public:
	static AMQPStatus AMQP_ConnectionCreate(
		SmartPtrCAmqpConnection& pConn,
		const SmartPtrAddress& address,
		const SmartPtrCAmqpAuthMechanism& auth,
		const SmartPtrCertInfo& certInfo,
		const uint16 channelMax,
		const uint32 frameMax,
		const uint16 heartbeat,
		const uint16 retries,
		const uint16 secondsToWait);

	static AMQPStatus AMQP_ConnectionConnect(
			const SmartPtrCAmqpConnection& conn,
			const int32 flags);

	static AMQPStatus AMQP_ConnectionOpenChannel(
			const SmartPtrCAmqpConnection& conn,
			SmartPtrCAmqpChannel& chan);

	static AMQPStatus AMQP_ConnectionClose(
			const SmartPtrCAmqpConnection& conn);

	static AMQPStatus AMQP_ConnectionProcessIO(
			const SmartPtrCAmqpConnection& conn);

	static AMQPStatus AMQP_ConnectionWaitForIO(
			const SmartPtrCAmqpConnection& conn,
			const int32 timeout);

	static AMQPStatus AMQP_ConnectionGetState(
			const SmartPtrCAmqpConnection& conn,
			AMQPConnectionState *state);

	static AMQPStatus AMQP_ConnectionGetMaxChannels(
			const SmartPtrCAmqpConnection& conn,
			uint16 *channels);

	static AMQPStatus AMQP_ConnectionGetMaxFrameSize(
			const SmartPtrCAmqpConnection& conn,
			uint32 *frameSize);

	static AMQPStatus AMQP_ConnectionGetHeartbeatInterval(
			const SmartPtrCAmqpConnection& conn,
			uint16 *interval);

	static AMQPStatus AMQP_ConnectionGetLastError(
			const SmartPtrCAmqpConnection& conn,
			const char **error);

private:
	CAF_CM_DECLARE_NOCREATE (AmqpConnection);
};

}
}

#endif /* AMQPCORE_AMQPCONNECTION_H_ */
