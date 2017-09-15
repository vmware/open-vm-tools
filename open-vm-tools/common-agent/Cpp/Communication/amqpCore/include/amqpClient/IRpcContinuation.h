/*
 *  Created on: May 15, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef IRPCCONTINUATION_H_
#define IRPCCONTINUATION_H_


#include "ICafObject.h"

#include "Exception/CCafException.h"
#include "amqpClient/AMQCommand.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Interface for RPC worker objects
 * <p>
 * The channel worker thread will assemble incoming AMQP frames an then process
 * the compiled frames as an AMQCommand object.  If the command object belongs
 * to an outstanding RPC call, that call will receive the command for processing.
 */
struct __declspec(novtable) IRpcContinuation : public ICafObject {
	/**
	 * @brief Process the received AMQP command
	 * @param command the AMQCommand command object
	 */
	virtual void handleCommand(const SmartPtrAMQCommand& command) = 0;

	/**
	 * @brief Abort the command for the reason supplied
	 * @param exception the reason as an exception
	 */
	virtual void handleAbort(SmartPtrCCafException exception) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(IRpcContinuation);

}}


#endif /* IRPCCONTINUATION_H_ */
