/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEBINDMETHOD_H_
#define QUEUEBINDMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/amqpClient.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.bind
 */
class QueueBindMethod : public IServerMethod {
public:
	QueueBindMethod();
	virtual ~QueueBindMethod();

	/**
	 * @brief Initialize the method
	 * @param queue queue name
	 * @param exchange exchange name
	 * @param routingKey routing key
	 * @param arguments binding arguments
	 */
	void init(
		const std::string& queue,
		const std::string& exchange,
		const std::string& routingKey,
		const SmartPtrTable& arguments);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	std::string _queue;
	std::string _exchange;
	std::string _routingKey;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueueBindMethod);
};
CAF_DECLARE_SMART_POINTER(QueueBindMethod);

}}

#endif /* QUEUEBINDMETHOD_H_ */
