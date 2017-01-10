/*
 *  Created on: Jan 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_ICHANNELINTERCEPTOR_H_
#define _IntegrationContracts_ICHANNELINTERCEPTOR_H_


#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"

namespace Caf {

struct __declspec(novtable)
	IChannelInterceptor : public ICafObject
{
	CAF_DECL_UUID("5002EA10-769B-44A0-AA6B-18ED91B57655")

	/**
	 * Invoked before the message is sent to the channel.
	 * The message may be modified if necessary.
	 * If this method returns null then the
	 * actual send invocation will not occur.
	 */
	virtual SmartPtrIIntMessage& preSend(
			SmartPtrIIntMessage& message,
			SmartPtrIMessageChannel& channel) = 0;

	/**
	 * Invoked immediately after the send invocation. The
	 * boolean value argument represents the return value of
	 * that invocation.
	 */
	virtual void postSend(
			SmartPtrIIntMessage& message,
			SmartPtrIMessageChannel& channel,
			bool sent) = 0;

	/**
	 * Invoked as soon as receive is called and before a message
	 * is actually retrieved. If the return value is 'false' then
	 * no message will be retrieved. This only applies to PollableChannels.
	 */
	virtual bool preReceive(
			SmartPtrIMessageChannel& channel) = 0;

	/**
	 * Invoked immediately after a message has been retrieved but
	 * before it is returned to the caller. The message may be modified
	 * if necessary. This only applies to PollableChannels.
	 */
	virtual SmartPtrIIntMessage& postReceive(
			SmartPtrIIntMessage& message,
			SmartPtrIMessageChannel& channel) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IChannelInterceptor);

}

#endif /* _IntegrationContracts_ICHANNELINTERCEPTOR_H_ */
