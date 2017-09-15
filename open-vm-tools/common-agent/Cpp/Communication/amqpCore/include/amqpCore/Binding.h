/*
 *  Created on: Jun 13, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_BINDING_H_
#define AMQPINTEGRATIONCORE_BINDING_H_


#include "ICafObject.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @brief Simple container collecting information to describe a queue binding. Used in conjunction with RabbitAdmin.
 * <p>
 * Use #Caf::AmqpIntegration::createBinding to create a binding.
 */
struct __declspec(novtable) Binding : public ICafObject {
	CAF_DECL_UUID("A6067BE6-18C1-4D50-BB2F-AB9E49EA2111")

	/** @return the queue name */
	virtual std::string getQueue() const = 0;

	/** @return the exchange name */
	virtual std::string getExchange() const = 0;

	/** @return the routing key */
	virtual std::string getRoutingKey() const = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Binding);

}}

#endif /* AMQPINTEGRATIONCORE_BINDING_H_ */
