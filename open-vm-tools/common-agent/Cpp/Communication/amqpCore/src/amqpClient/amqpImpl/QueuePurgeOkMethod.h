/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QueuePurgeOKMETHOD_H_
#define QueuePurgeOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.purge-ok
 */
class QueuePurgeOkMethod :
	public TMethodImpl<QueuePurgeOkMethod>,
	public AmqpMethods::Queue::PurgeOk {
	METHOD_DECL(
		AmqpMethods::Queue::PurgeOk,
		AMQP_QUEUE_PURGE_OK_METHOD,
		"queue.purge-ok",
		false)

public:
	QueuePurgeOkMethod();
	virtual ~QueuePurgeOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Queue::PurgeOk
	uint32 getMessageCount();

private:
	uint32 _messageCount;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueuePurgeOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(QueuePurgeOkMethod);

}}

#endif /* QueuePurgeOKMETHOD_H_ */
