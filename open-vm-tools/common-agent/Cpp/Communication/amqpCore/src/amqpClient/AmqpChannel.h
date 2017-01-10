/*
 *  Created on: Oct 7, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2014-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCORE_AMQPCHANNEL_H_
#define AMQPCORE_AMQPCHANNEL_H_

#include "AmqpCommon.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/CAmqpFrame.h"

namespace Caf { namespace AmqpClient {

/** AMQP channel state. */
typedef enum AMQPChannelState {
	AMQP_CHANNEL_OPEN = 0, AMQP_CHANNEL_WAITING_FOR_CLOSE_OK, AMQP_CHANNEL_CLOSED
} AMQPChannelState;

class AmqpChannel {
public:
	static AMQPStatus AMQP_ChannelClose(
			const SmartPtrCAmqpChannel& chan);

	static AMQPStatus AMQP_ChannelCloseOk(
			const SmartPtrCAmqpChannel& chan);

	static AMQPStatus AMQP_ChannelReceive(
			const SmartPtrCAmqpChannel& chan,
			SmartPtrCAmqpFrame& frame,
			const int32 timeout);

	static AMQPStatus AMQP_ChannelGetId(
			const SmartPtrCAmqpChannel& chan,
			uint16 *id);

private:
	CAF_CM_DECLARE_NOCREATE (AmqpChannel);
};

}
}

#endif /* AMQPCORE_CHANNEL_H_ */
