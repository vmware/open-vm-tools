/*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CHANNELOPENOKMETHOD_H_
#define CHANNELOPENOKMETHOD_H_


#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf { namespace AmqpClient {
/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP channel.open-ok
 */
class ChannelOpenOkMethod :
	public TMethodImpl<ChannelOpenOkMethod>,
	public AmqpMethods::Channel::OpenOk {
	METHOD_DECL(
		AmqpMethods::Channel::OpenOk,
		AMQP_CHANNEL_OPEN_OK_METHOD,
		"channel.open-ok",
		false)

public:
	ChannelOpenOkMethod();
	virtual ~ChannelOpenOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Channel::OpenOk
	SmartPtrCDynamicByteArray getChannelId();

private:
	SmartPtrCDynamicByteArray _channelId;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(ChannelOpenOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(ChannelOpenOkMethod);

}}

#endif /* CHANNELOPENOKMETHOD_H_ */
