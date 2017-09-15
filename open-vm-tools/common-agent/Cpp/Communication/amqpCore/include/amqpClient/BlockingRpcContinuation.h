/*
 *  Created on: May 15, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BLOCKINGRPCCONTINUATION_H_
#define BLOCKINGRPCCONTINUATION_H_



#include "amqpClient/IRpcContinuation.h"

#include "Exception/CCafException.h"
#include "amqpClient/AMQCommand.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @brief A IRpcContinuation that blocks until the response is received.
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 */
class BlockingRpcContinuation : public IRpcContinuation {
public:
	BlockingRpcContinuation();
	virtual ~BlockingRpcContinuation();

	/**
	 * @brief Initialize the object
	 */
	void init();

	/**
	 * @brief Waits indefinately for a response to an AMQP method call
	 * @retval null if an error occured. In this case call getException to get the exception
	 * @retval non-null AMQCommand response
	 */
	SmartPtrAMQCommand getReply();

	/**
	 * @brief Waits for a time for a response to an AMQP method call
	 * @param timeout time in milliseconds to wait for a response
	 * @retval null if an error occured. In this case call getException to get the exception
	 * @retval non-null AMQCommand response
	 */
	SmartPtrAMQCommand getReply(uint32 timeout);

	/**
	 * @brief Returns the exception that occured if getReply returns <code><i>null</i></code>.
	 * @return the exception. The returned exception will have its reference count increased.
	 */
	SmartPtrCCafException getException();

public: // IRpcContinuation
	void handleCommand(const SmartPtrAMQCommand& command);

	void handleAbort(SmartPtrCCafException exception);

private:
	bool _isInitialized;
	TBlockingCell<SmartPtrAMQCommand> _blocker;
	SmartPtrCCafException _exception;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BlockingRpcContinuation);
};
CAF_DECLARE_SMART_POINTER(BlockingRpcContinuation);

}}

#endif /* BLOCKINGRPCCONTINUATION_H_ */
