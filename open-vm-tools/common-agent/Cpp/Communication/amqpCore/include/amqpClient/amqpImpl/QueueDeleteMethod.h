/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEDELETEMETHOD_H_
#define QUEUEDELETEMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.delete
 */
class QueueDeleteMethod : public IServerMethod {
public:
	QueueDeleteMethod();
	virtual ~QueueDeleteMethod();

	/**
	 * @brief Initialize the method
	 * @param queue queue name
	 * @param ifUnused delete if queue is not in use flag
	 * @param ifEmpty delete is queue is empty flag
	 */
	void init(
		const std::string& queue,
		const bool ifUnused,
		const bool ifEmpty);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	std::string _queue;
	bool _ifUnused;
	bool _ifEmpty;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueueDeleteMethod);

};
CAF_DECLARE_SMART_POINTER(QueueDeleteMethod);

}}

#endif /* QUEUEDELETEMETHOD_H_ */
