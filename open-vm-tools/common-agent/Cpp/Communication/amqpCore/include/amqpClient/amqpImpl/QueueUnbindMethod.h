/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEUNBINDMETHOD_H_
#define QUEUEUNBINDMETHOD_H_


#include "amqpClient/amqpImpl/IServerMethod.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/amqpClient.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.unbind
 */
class QueueUnbindMethod : public IServerMethod {
public:
	QueueUnbindMethod();
	virtual ~QueueUnbindMethod();

	/**
	 * @brief Initialize the method
	 * @param queue queue name
	 * @param exchange exchange name
	 * @param routingKey routing key
	 * @param arguments method arguments
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
	CAF_CM_DECLARE_NOCOPY(QueueUnbindMethod);

};
CAF_DECLARE_SMART_POINTER(QueueUnbindMethod);

}}

#endif /* QUEUEUNBINDMETHOD_H_ */
