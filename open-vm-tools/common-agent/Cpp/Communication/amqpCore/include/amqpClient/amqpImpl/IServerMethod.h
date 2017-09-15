/*
 *  Created on: May 15, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef ISERVERMETHOD_H_
#define ISERVERMETHOD_H_


#include "ICafObject.h"

#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Base interface for server methods
 */
struct __declspec(novtable) IServerMethod : public ICafObject {
	CAF_DECL_UUID("ce585a8d-8b49-4312-b356-6f612142b154")

	/**
	 * Return the method name
	 * @return the method name
	 */
	virtual std::string getMethodName() const = 0;

	/**
	 * Sends the command to the server
	 * @param channel AMQP channel
	 * @return the c-api AMQPStatus of the call
	 */
	virtual AMQPStatus send(const SmartPtrCAmqpChannel& channel) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(IServerMethod);

}}

#endif /* ISERVERMETHOD_H_ */
