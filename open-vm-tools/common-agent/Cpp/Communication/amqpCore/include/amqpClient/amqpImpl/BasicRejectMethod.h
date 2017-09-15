/*
 *  Created on: Aug 1, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICREJECTMETHOD_H_
#define BASICREJECTMETHOD_H_


#include "amqpClient/amqpImpl/IServerMethod.h"

#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.Reject
 */
class BasicRejectMethod : public IServerMethod {
public:
	BasicRejectMethod();
	virtual ~BasicRejectMethod();

	/**
	 * @brief Initialize the method
	 * @param deliveryTag delivery tag
	 * @param requeue requeue flag
	 */
	void init(
		const uint64 deliveryTag,
		const bool requeue);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	uint64 _deliveryTag;
	bool _requeue;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicRejectMethod);
};
CAF_DECLARE_SMART_POINTER(BasicRejectMethod);

}}

#endif /* BASICREJECTMETHOD_H_ */
