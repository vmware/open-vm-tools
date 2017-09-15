/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICDELIVERMETHOD_H_
#define BASICDELIVERMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.deliver
 */
class BasicDeliverMethod :
	public TMethodImpl<BasicDeliverMethod>,
	public AmqpMethods::Basic::Deliver {
	METHOD_DECL(
		AmqpMethods::Basic::Deliver,
		AMQP_BASIC_DELIVER_METHOD,
		"basic.deliver",
		true)

public:
	BasicDeliverMethod();
	virtual ~BasicDeliverMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Basic::Deliver
	std::string getConsumerTag();
	uint64 getDeliveryTag();
	std::string getExchange();
	bool getRedelivered();
	std::string getRoutingKey();

private:
	std::string _consumerTag;
	uint64 _deliveryTag;
	std::string _exchange;
	bool _redelivered;
	std::string _routingKey;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicDeliverMethod);
};
CAF_DECLARE_SMART_QI_POINTER(BasicDeliverMethod);

}}

#endif /* BASICDELIVERMETHOD_H_ */
