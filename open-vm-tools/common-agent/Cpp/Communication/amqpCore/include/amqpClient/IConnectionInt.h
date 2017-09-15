/*
 *  Created on: May 8, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef ICONNECTIONINT_H_
#define ICONNECTIONINT_H_

#include "ICafObject.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/Channel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief This interface abstracts the calls to AMQP_ConnectionXXX c-api calls
 * <p>
 * AMQConnection objects pass weak references to themselves to AMQChannel objects
 * allowing them to make calls against the channel that require the connection handle.
 */
struct __declspec(novtable) IConnectionInt : public ICafObject {

	/**
	 * @brief Pass-through for the AMQP_ConnectionOpenChannel call
	 * @param channel the channel handle to be returned
	 * @return the AMQPStatus of the call.  If AMQP_ERROR_OK then
	 * <code><b>channel</b></code> will be set to a valid channel handle.
	 */
	virtual AMQPStatus amqpConnectionOpenChannel(SmartPtrCAmqpChannel& channel) = 0;

	/**
	 * @brief Callback to notify the connection that a channel has been closed.  This
	 * is in reponse to the server sending a channel.close method.
	 * @param channelNumber the channel number
	 */
	virtual void notifyChannelClosedByServer(const uint16 channelNumber) = 0;

	/**
	 * @brief Callback to notify the connection that a channel is being
	 * requested to close.
	 * @param channel Channel to close
	 */
	virtual void channelCloseChannel(Channel *channel) = 0;

};
CAF_DECLARE_SMART_INTERFACE_POINTER(IConnectionInt);

}}

#endif /* ICONNECTIONINT_H_ */
