/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEBINDOKMETHOD_H_
#define QUEUEBINDOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.bind-ok
 */
class QueueBindOkMethod :
	public TMethodImpl<QueueBindOkMethod>,
	public AmqpMethods::Queue::BindOk {
	METHOD_DECL(
		AmqpMethods::Queue::BindOk,
		AMQP_QUEUE_BIND_OK_METHOD,
		"queue.bind-ok",
		false)

public:
	QueueBindOkMethod();
	virtual ~QueueBindOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Queue::BindOk

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueueBindOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(QueueBindOkMethod);

}}

#endif /* QUEUEBINDOKMETHOD_H_ */
