/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEDECLAREOKMETHOD_H_
#define QUEUEDECLAREOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.declare-ok
 */
class QueueDeclareOkMethod :
	public TMethodImpl<QueueDeclareOkMethod>,
	public AmqpMethods::Queue::DeclareOk {
	METHOD_DECL(
		AmqpMethods::Queue::DeclareOk,
		AMQP_QUEUE_DECLARE_OK_METHOD,
		"queue.declare-ok",
		false)

public:
	QueueDeclareOkMethod();
	virtual ~QueueDeclareOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Queue::DeclareOk
	std::string getQueueName();
	uint32 getMessageCount();
	uint32 getConsumerCount();

private:
	std::string _queueName;
	uint32 _messageCount;
	uint32 _consumerCount;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueueDeclareOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(QueueDeclareOkMethod);

}}

#endif /* QUEUEDECLAREOKMETHOD_H_ */
