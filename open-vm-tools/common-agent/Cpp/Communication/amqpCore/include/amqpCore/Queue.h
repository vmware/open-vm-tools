/*
 *  Created on: Jun 13, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_QUEUE_H_
#define AMQPINTEGRATIONCORE_QUEUE_H_


#include "ICafObject.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @brief Simple container collecting information to describe a queue. Used in conjunction with RabbitAdmin.
 * <p>
 * Use #Caf::AmqpIntegration::createQueue to create a queue.
 */
struct __declspec(novtable) Queue : public ICafObject {
	CAF_DECL_UUID("082088AD-ECA3-4591-986F-8D9AFEDDEE7D")

	/** @return the name of the queue */
	virtual std::string getName() const = 0;

	/**
	 * @retval true the queue is durable
	 * @retval false the queue is not durable
	 */
	virtual bool isDurable() const = 0;

	/**
	 * @retval true the queue is exclusive to the connection
	 * @retval false the queue is not exclusive to the connection
	 */
	virtual bool isExclusive() const = 0;

	/**
	 * @retval true the server should delete the queue when it is no longer in use
	 * @retval false the server should not delete the queue when it is no longer in use
	 */
	virtual bool isAutoDelete() const = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Queue);

}}

#endif /* AMQPINTEGRATIONCORE_QUEUE_H_ */
