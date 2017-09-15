/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICRECOVERMETHOD_H_
#define BASICRECOVERMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.recover
 */
class BasicRecoverMethod : public IServerMethod {
public:
	BasicRecoverMethod();
	virtual ~BasicRecoverMethod();

	/**
	 * @brief Initialize the method
	 * @param requeue if <code><b>false</b></code>, the message will be redelivered to the
	 * original receipient.  If <code><b>true</b></code>, the server will attempt to requeue
	 * the message, potentially delivering it to an alternative subscriber.
	 */
	void init(
		const bool requeue);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	bool _requeue;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicRecoverMethod);

};
CAF_DECLARE_SMART_POINTER(BasicRecoverMethod);

}}

#endif /* BASICRECOVERMETHOD_H_ */
