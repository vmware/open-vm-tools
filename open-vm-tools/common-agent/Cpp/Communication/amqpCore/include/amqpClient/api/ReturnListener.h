/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef RETURNLISTENER_H_
#define RETURNLISTENER_H_

#include "ICafObject.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/AmqpContentHeaders.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Interface for objects that will be notified of failed message deliveries
 * <p>
 * When {@link Channel#basicPublish} is used with the <i>mandatory</i> and/or <i>immediate</i>
 * flags set and the message cannot be delivered, the server will response with a
 * <code><b>basic.return</b></code> method call.  {@link ReturnListener}s can monitor
 * these failed messages.
 */
struct __declspec(novtable) ReturnListener : public ICafObject {
	CAF_DECL_UUID("FEB38A27-6338-4BDB-AA0E-527322A2393B")

	/**
	 * @brief Callback receiving the failed message
	 * @param replyCode server reply code
	 * @param replyText server reply text
	 * @param exchange exchange on which error occured
	 * @param routingKey routing key for the message that failed
	 * @param properties original message properties
	 * @param body original message body
	 */
	virtual void handleReturn(
			const uint16 replyCode,
			const std::string& replyText,
			const std::string& exchange,
			const std::string& routingKey,
			const AmqpContentHeaders::SmartPtrBasicProperties& properties,
			const SmartPtrCDynamicByteArray& body) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ReturnListener);

}}

#endif /* RETURNLISTENER_H_ */
