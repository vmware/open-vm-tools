/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef EXCHANGEDELETEOKMETHOD_H_
#define EXCHANGEDELETEOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP exchange.delete-ok
 */
class ExchangeDeleteOkMethod :
	public TMethodImpl<ExchangeDeleteOkMethod>,
	public AmqpMethods::Exchange::DeleteOk {
	METHOD_DECL(
		AmqpMethods::Exchange::DeleteOk,
		AMQP_EXCHANGE_DELETE_OK_METHOD,
		"exchange.delete-ok",
		false)

public:
	ExchangeDeleteOkMethod();
	virtual ~ExchangeDeleteOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Queue::DeclareOk

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(ExchangeDeleteOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(ExchangeDeleteOkMethod);

}}

#endif /* EXCHANGEDELETEOKMETHOD_H_ */
