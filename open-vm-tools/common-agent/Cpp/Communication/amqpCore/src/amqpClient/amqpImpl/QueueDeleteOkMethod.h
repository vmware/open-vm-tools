/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEDELETEOKMETHOD_H_
#define QUEUEDELETEOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.delete-ok
 */
class QueueDeleteOkMethod :
	public TMethodImpl<QueueDeleteOkMethod>,
	public AmqpMethods::Queue::DeleteOk {
	METHOD_DECL(
		AmqpMethods::Queue::DeleteOk,
		AMQP_QUEUE_DELETE_OK_METHOD,
		"queue.delete-ok",
		false)

public:
	QueueDeleteOkMethod();
	virtual ~QueueDeleteOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Queue::DeclareOk
	uint32 getMessageCount();

private:
	uint32 _messageCount;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueueDeleteOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(QueueDeleteOkMethod);

}}

#endif /* QUEUEDELETEOKMETHOD_H_ */
