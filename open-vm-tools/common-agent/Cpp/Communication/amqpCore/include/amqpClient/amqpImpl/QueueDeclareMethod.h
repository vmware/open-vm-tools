/*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef QUEUEDECLAREMETHOD_H_
#define QUEUEDECLAREMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/amqpClient.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP queue.declare
 */
class QueueDeclareMethod :
	public IServerMethod {
public:
	QueueDeclareMethod();
	virtual ~QueueDeclareMethod();

	/**
	 * @brief Initialize the method using defaults
	 * <p>
	 * The defaults are:
	 * <table border="1">
	 * <tr>
	 * <th>Parameter</th><th>Value</th>
	 * </tr>
	 * <tr><td>queue</td><td>blank - the server will generate a queue name</td></tr>
	 * <tr><td>durable</td><td>false - the queue will not be durable</td></tr>
	 * <tr><td>exclusive</td><td>true - the queue will be exclusive to this conenction</td></tr>
	 * <tr><td>autoDelete</td><td>true - the queue will be deleted when no longer used</td></tr>
	 * </table>
	 */
	void init();

	/**
	 * @brief Initialize the method
	 * @param queue queue name
	 * @param durable durable queue flag
	 * @param exclusive exclusive queue flag
	 * @param autoDelete delete when no longer in use flag
	 * @param arguments method arguments
	 */
	void init(
		const std::string& queue,
		bool durable,
		bool exclusive,
		bool autoDelete,
		const SmartPtrTable& arguments);

	/**
	 * @brief Initialize the method in passive mode
	 * @param queue queue name
	 */
	void initPassive(
			const std::string& queue);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	std::string _queue;
	bool _passive;
	bool _durable;
	bool _exclusive;
	bool _autoDelete;
	bool _noWait;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(QueueDeclareMethod);
};
CAF_DECLARE_SMART_POINTER(QueueDeclareMethod);

}}

#endif /* QUEUEDECLAREMETHOD_H_ */
