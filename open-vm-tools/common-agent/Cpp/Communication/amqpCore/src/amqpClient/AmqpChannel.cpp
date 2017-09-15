/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/CAmqpFrame.h"
#include "AmqpChannel.h"
#include "AmqpUtil.h"

using namespace Caf::AmqpClient;

AMQPStatus AmqpChannel::AMQP_ChannelClose(
		const SmartPtrCAmqpChannel& channel) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpChannel", "AMQP_ChannelClose");
	CAF_CM_VALIDATE_SMARTPTR(channel);

	return channel->close();
}

AMQPStatus AmqpChannel::AMQP_ChannelCloseOk(
		const SmartPtrCAmqpChannel& channel) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpChannel", "AMQP_ChannelCloseOk");
	CAF_CM_VALIDATE_SMARTPTR(channel);

	return channel->closeOk();
}

AMQPStatus AmqpChannel::AMQP_ChannelReceive(
		const SmartPtrCAmqpChannel& channel,
		SmartPtrCAmqpFrame& frame,
		const int32 timeout) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpChannel", "AMQP_ChannelReceive");
	CAF_CM_VALIDATE_SMARTPTR(channel);

	return channel->receive(frame, timeout);
}

AMQPStatus AmqpChannel::AMQP_ChannelGetId(
		const SmartPtrCAmqpChannel& channel,
		uint16 *id) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpChannel", "AMQP_ChannelGetId");
	CAF_CM_VALIDATE_SMARTPTR(channel);
	CAF_CM_VALIDATE_PTR(id);

	return channel->getId(id);
}
