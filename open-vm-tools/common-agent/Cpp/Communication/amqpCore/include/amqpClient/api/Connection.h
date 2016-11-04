/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_CONNECTION_H_
#define AMQPCLIENTAPI_CONNECTION_H_


#include "ICafObject.h"

#include "amqpClient/api/Channel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Interface to an AMQP connection.
 */
struct __declspec(novtable) Connection : public ICafObject {
	/**
	 * @brief Create a new channel
	 * @return a Channel
	 */
	virtual SmartPtrChannel createChannel() = 0;

	/**
	 * @brief Close a channel
	 * @param channel the Channel to close
	 */
	virtual void closeChannel(const SmartPtrChannel& channel) = 0;

	/**
	 * @brief Closes the connection and its channels
	 */
	virtual void close() = 0;

	/**
	 * @brief Return the state of the connection
	 * @retval true the connection is open
	 * @retval false the connection is closed
	 */
	virtual bool isOpen() = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(Connection);

}}

#endif
