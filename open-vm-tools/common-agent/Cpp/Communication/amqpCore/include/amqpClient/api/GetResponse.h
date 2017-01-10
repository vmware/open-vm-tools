/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_GETRESPONSE_H_
#define AMQPCLIENTAPI_GETRESPONSE_H_

#include "ICafObject.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/Envelope.h"
#include "amqpClient/api/AmqpContentHeaders.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Interface representing the bundling of basic.get messages into a nicer form
 */
struct __declspec(novtable) GetResponse : public ICafObject {
	CAF_DECL_UUID("c8bda284-7eea-46e1-b9c3-791310d69b04")

	/** @return the message envelope information (#Caf::AmqpClient::Envelope) */
	virtual SmartPtrEnvelope getEnvelope() = 0;

	/** @return the message properties (#Caf::AmqpClient::AmqpContentHeaders::BasicProperties) */
	virtual AmqpContentHeaders::SmartPtrBasicProperties getProperties() = 0;

	/** @return the message body raw bytes */
	virtual SmartPtrCDynamicByteArray getBody() = 0;

	/** @return the number of messages in the queue */
	virtual uint32 getMessageCount() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(GetResponse);

}}

#endif
