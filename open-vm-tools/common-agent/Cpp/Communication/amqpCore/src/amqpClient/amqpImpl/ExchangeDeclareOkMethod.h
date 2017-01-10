/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef EXCHANGEDECLAREOKMETHOD_H_
#define EXCHANGEDECLAREOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP exchange.declare-ok
 */
class ExchangeDeclareOkMethod :
	public TMethodImpl<ExchangeDeclareOkMethod>,
	public AmqpMethods::Exchange::DeclareOk {
	METHOD_DECL(
		AmqpMethods::Exchange::DeclareOk,
		AMQP_EXCHANGE_DECLARE_OK_METHOD,
		"exchange.declare-ok",
		false)

public:
	ExchangeDeclareOkMethod();
	virtual ~ExchangeDeclareOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Queue::DeclareOk

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(ExchangeDeclareOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(ExchangeDeclareOkMethod);

}}

#endif /* EXCHANGEDECLAREOKMETHOD_H_ */
