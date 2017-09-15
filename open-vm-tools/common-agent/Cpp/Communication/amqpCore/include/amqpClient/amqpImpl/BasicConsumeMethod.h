/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICCONSUMEMETHOD_H_
#define BASICCONSUMEMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/amqpClient.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.consume
 */
class BasicConsumeMethod : public IServerMethod {
public:
	BasicConsumeMethod();
	virtual ~BasicConsumeMethod();

	/**
	 * @brief Initialize the method
	 * @param queue queue name
	 * @param consumerTag consumer tag (or blank for server-generated tag)
	 * @param noLocal do not send messages to the connection that published them
	 * @param noAck no acknowledgement needed
	 * @param exclusive request exclusive consumer access to the queue
	 * @param arguments additional call arguments
	 */
	void init(
		const std::string& queue,
		const std::string& consumerTag,
		const bool noLocal,
		const bool noAck,
		const bool exclusive,
		const SmartPtrTable& arguments);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	std::string _queue;
	std::string _consumerTag;
	bool _noLocal;
	bool _noAck;
	bool _exclusive;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicConsumeMethod);
};
CAF_DECLARE_SMART_POINTER(BasicConsumeMethod);

}}

#endif /* BASICCONSUMEMETHOD_H_ */
