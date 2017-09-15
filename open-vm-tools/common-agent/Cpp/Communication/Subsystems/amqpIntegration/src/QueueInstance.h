/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEINSTANCE_H_
#define QUEUEINSTANCE_H_

#include "Integration/IIntegrationObject.h"
#include "Integration/IDocument.h"
#include "amqpCore/Queue.h"
#include "amqpCore/QueueInternal.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObj
 * @brief An Integration Object implementing Caf::AmqpIntegration::Queue
 * <p>
 * Example context file declarations:
 * <p>
 * Named queue:
 * <pre>
 * <rabbit-queue
 *    id="inboundQueue"
 *    name="myapp.inq" />
 * </pre>
 * Anonymous queue:
 * <pre>
 * <rabbit-queue
 * 	id="inboundQueue" />
 * </pre>
 * <p>
 * XML attribute definitions:
 * <table border="1">
 * <tr><th>Attribute</th><th>Description</th></tr>
 * <tr><td>id</td><td><b>required</b> The id of the integration object.
 * All integration objects that reference queues must do so by their id.</td></tr>
 * <tr><td>name</td>
 * <td><i>optional</i> The AMQP name of the queue.  This is the name that
 * will be sent in the Queue.Declare AMQP method.  It does not need to match the
 * <i>id</i>.  To declare an anonymous (server-named) queue, leave this attribute
 * out of the declaration. <b>Do not set <i>name</i> to blank</b>.</td></tr>
 * <tr><td>durable</td>
 * <td><i>optional</i> <i>true</i> to declare a durable queue else <i>false</i>.</td></tr>
 * <tr><td>exclusive</td>
 * <td><i>optional</i> <i>true</i> to declare the queue exclusive to the connection
 * else <i>false</i>.</td></tr>
 * <tr><td>auto-delete</td>
 * <td><i>optional</i> <i>true</i> to declare that the queue be deleted when it is no
 * longer in use else <i>false</i>.</td></tr>
 * </table>
 * <b>NOTE:</b> Anonymous queues are automatically declared as non-durable, exclusive
 * and auto-delete.  They cannot be declared otherwise.
 */
class QueueInstance :
	public IIntegrationObject,
	public QueueInternal,
	public Queue
{
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(QueueInternal)
		CAF_QI_ENTRY(Queue)
	CAF_END_QI()

public:
	QueueInstance();
	virtual ~QueueInstance();

public: // IIntegrationObject
	void initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection);

	std::string getId() const;

public: // QueueInternal
	void setQueueInternal(SmartPtrQueue queue);

public: // Queue
	std::string getName() const;
	bool isDurable() const;
	bool isExclusive() const;
	bool isAutoDelete() const;

private:
	std::string _id;
	SmartPtrQueue _queue;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueueInstance);
};
CAF_DECLARE_SMART_QI_POINTER(QueueInstance);

}}

#endif /* QUEUEINSTANCE_H_ */
