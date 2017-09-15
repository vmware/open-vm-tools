/*
 *  Created on: May 24, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_CONNECTION_H_
#define AMQPINTEGRATIONCORE_CONNECTION_H_


#include "ICafObject.h"

#include "amqpClient/api/Channel.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief An interface for Caf::AmqpIntegration connection objects
 */
struct __declspec(novtable) Connection : public ICafObject {

	/**
	 * @brief Create a {@link AmqpClient::Channel Channel}.
	 */
	virtual AmqpClient::SmartPtrChannel createChannel() = 0;

	/**
	 * @brief Close the connection
	 */
	virtual void close() = 0;

	/**
	 * @brief Check the connection's status
	 * @retval true the connection is open
	 * @retval false the connection is closed
	 */
	virtual bool isOpen() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Connection);

}}

#endif /* AMQPINTEGRATIONCORE_CONNECTION_H_ */
