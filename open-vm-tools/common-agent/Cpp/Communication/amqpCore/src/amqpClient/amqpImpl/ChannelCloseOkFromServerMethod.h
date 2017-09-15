/*
 *  Created on: May 22, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CHANNELCLOSEOKFROMSERVERMETHOD_H_
#define CHANNELCLOSEOKFROMSERVERMETHOD_H_

namespace Caf { namespace AmqpClient {
/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP channel.close-ok (received from server)
 */
class ChannelCloseOkFromServerMethod :
	public TMethodImpl<ChannelCloseOkFromServerMethod>,
	public AmqpMethods::Channel::CloseOk {
	METHOD_DECL(
		AmqpMethods::Channel::CloseOk,
		AMQP_CHANNEL_CLOSE_OK_METHOD,
		"channel.close-ok",
		false)

public:
	ChannelCloseOkFromServerMethod();
	virtual ~ChannelCloseOkFromServerMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Channel::CloseOk

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(ChannelCloseOkFromServerMethod);
};
CAF_DECLARE_SMART_QI_POINTER(ChannelCloseOkFromServerMethod);

}}

#endif /* CHANNELCLOSEOKFROMSERVERMETHOD_H_ */
