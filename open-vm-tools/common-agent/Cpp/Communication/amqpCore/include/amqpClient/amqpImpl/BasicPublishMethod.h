/*
 *  Created on: May 15, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICPUBLISHMETHOD_H_
#define BASICPUBLISHMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.publish
 */
class BasicPublishMethod : public IServerMethod {
public:
	BasicPublishMethod();
	virtual ~BasicPublishMethod();

	/**
	 * @brief Initialize the method
	 * @param exchange exchange name
	 * @param routingKey routing key
	 * @param mandatory mandatory delivery flag
	 * @param immediate immediate delivery flag
	 * @param properties message properties
	 * @param body message body raw bytes
	 */
	void init(
		const std::string& exchange,
		const std::string& routingKey,
		bool mandatory,
		bool immediate,
		const AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	std::string _exchange;
	std::string _routingKey;
	bool _mandatory;
	bool _immediate;
	AmqpContentHeaders::SmartPtrBasicProperties _properties;
	SmartPtrCDynamicByteArray _body;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicPublishMethod);
};
CAF_DECLARE_SMART_POINTER(BasicPublishMethod);

}}

#endif /* BASICPUBLISHMETHOD_H_ */
