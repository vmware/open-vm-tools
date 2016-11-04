/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_ENVELOPE_H_
#define AMQPCLIENTAPI_ENVELOPE_H_


#include "ICafObject.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief An interface to objects that group together basic.get-ok message properties
 */
struct __declspec(novtable) Envelope : public ICafObject {
	CAF_DECL_UUID("ce68d68a-6973-49e2-a003-cb4474624f5c")

	/** @return the delivery tag */
	virtual uint64 getDeliveryTag() = 0;

	/**
	 * @retval <code><b>true</b></code> if the message was redelivered
	 * @retval <code><b>false</b></code> if the message has not been redelivered
	 */
	virtual bool getRedelivered() = 0;

	/** @return the name of the exchange supplying the message */
	virtual std::string getExchange() = 0;

	/** @return the message's routing key */
	virtual std::string getRoutingKey() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Envelope);

}}

#endif
