/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef EXCHANGEDECLAREMETHOD_H_
#define EXCHANGEDECLAREMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/api/amqpClient.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP exchange.declare
 */
class ExchangeDeclareMethod :	public IServerMethod {
public:
	ExchangeDeclareMethod();
	virtual ~ExchangeDeclareMethod();

	/**
	 * @brief Initializes the method
	 * @param exchange exchange name
	 * @param type exchange type
	 * @param passive passive mode call
	 * @param durable request a durable exchange
	 * @param arguments declaration arguments
	 */
	void init(
		const std::string& exchange,
		const std::string& type,
		const bool passive,
		const bool durable,
		const SmartPtrTable& arguments);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	std::string _exchange;
	std::string _type;
	bool _passive;
	bool _durable;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(ExchangeDeclareMethod);
};
CAF_DECLARE_SMART_POINTER(ExchangeDeclareMethod);

}}
#endif /* EXCHANGEDECLAREMETHOD_H_ */
