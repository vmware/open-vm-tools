/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICGETOKMETHOD_H_
#define BASICGETOKMETHOD_H_

#include "amqpClient/api/AmqpMethods.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.get-ok
 */
class BasicGetOkMethod :
	public TMethodImpl<BasicGetOkMethod>,
	public AmqpMethods::Basic::GetOk {
	METHOD_DECL(
		AmqpMethods::Basic::GetOk,
		AMQP_BASIC_GET_OK_METHOD,
		"basic.get-ok",
		true)

public:
	BasicGetOkMethod();
	virtual ~BasicGetOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Basic::GetOk
	uint64 getDeliveryTag();
	std::string getExchange();
	uint32 getMessageCount();
	bool getRedelivered();
	std::string getRoutingKey();

private:
	uint64 _deliveryTag;
	std::string _exchange;
	uint32 _messageCount;
	bool _redelivered;
	std::string _routingKey;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicGetOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(BasicGetOkMethod);

}}

#endif /* BASICGETOKMETHOD_H_ */
