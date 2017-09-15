/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEUNBINDOKMETHOD_H_
#define QUEUEUNBINDOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.ubind-ok
 */
class QueueUnbindOkMethod :
	public TMethodImpl<QueueUnbindOkMethod>,
	public AmqpMethods::Queue::UnbindOk {
	METHOD_DECL(
		AmqpMethods::Queue::UnbindOk,
		AMQP_QUEUE_UNBIND_OK_METHOD,
		"queue.unbind-ok",
		false)

public:
	QueueUnbindOkMethod();
	virtual ~QueueUnbindOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Queue::UnbindOk

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueueUnbindOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(QueueUnbindOkMethod);

}}

#endif /* QUEUEUNBINDOKMETHOD_H_ */
