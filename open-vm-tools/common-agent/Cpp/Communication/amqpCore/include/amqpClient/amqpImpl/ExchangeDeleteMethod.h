/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef EXCHANGEDELETEMETHOD_H_
#define EXCHANGEDELETEMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP exchange.delete
 */
class ExchangeDeleteMethod : public IServerMethod {
public:
	ExchangeDeleteMethod();
	virtual ~ExchangeDeleteMethod();

	/**
	 * @brief Initializes the method
	 * @param exchange exchange name
	 * @param ifUnused delete only if unused flag
	 */
	void init(
		const std::string& exchange,
		const bool ifUnused);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	std::string _exchange;
	bool _ifUnused;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(ExchangeDeleteMethod);

};
CAF_DECLARE_SMART_POINTER(ExchangeDeleteMethod);

}}

#endif /* EXCHANGEDELETEMETHOD_H_ */
