/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEPURGEMETHOD_H_
#define QUEUEPURGEMETHOD_H_


#include "amqpClient/amqpImpl/IServerMethod.h"

#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.purge
 */
class QueuePurgeMethod : public IServerMethod {
public:
	QueuePurgeMethod();
	virtual ~QueuePurgeMethod();

	/**
	 * @brief Initialize the method
	 * @param queue queue name
	 */
	void init(
		const std::string& queue);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	std::string _queue;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueuePurgeMethod);

};
CAF_DECLARE_SMART_POINTER(QueuePurgeMethod);

}}

#endif /* QUEUEPURGEMETHOD_H_ */
