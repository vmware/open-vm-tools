/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICACKMETHOD_H_
#define BASICACKMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.ack
 */
class BasicAckMethod :public IServerMethod {
public:
	BasicAckMethod();
	virtual ~BasicAckMethod();

	/**
	 * @brief Initialize the method
	 * @param deliveryTag delivery tag
	 * @param ackMultiple acknowledge multiple flag
	 */
	void init(
		const uint64 deliveryTag,
		const bool ackMultiple);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	uint64 _deliveryTag;
	bool _ackMultiple;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicAckMethod);
};
CAF_DECLARE_SMART_POINTER(BasicAckMethod);

}}

#endif /* BASICACKMETHOD_H_ */
