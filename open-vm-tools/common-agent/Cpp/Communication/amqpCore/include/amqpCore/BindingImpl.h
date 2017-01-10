/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_BINDINGIMPL_H_
#define AMQPINTEGRATIONCORE_BINDINGIMPL_H_

namespace Caf { namespace AmqpIntegration {

/**
 * @brief Implementations of the #Caf::AmqpIntegration::Binding interface.
 */
class AMQPINTEGRATIONCORE_LINKAGE BindingImpl : public Binding {
public:
	BindingImpl();
	virtual ~BindingImpl();

	/**
	 * @brief initialize the object
	 * @param queue the name of the queue
	 * @param exchange the name of the exchange
	 * @param routingKey the routing key
	 */
	void init(
			const std::string queue,
			const std::string exchange,
			const std::string routingKey);

	std::string getQueue() const;

	std::string getExchange() const;

	std::string getRoutingKey() const;

private:
	std::string _queue;
	std::string _exchange;
	std::string _routingKey;
	CAF_CM_DECLARE_NOCOPY(BindingImpl);
};
CAF_DECLARE_SMART_POINTER(BindingImpl);

}}

#endif
