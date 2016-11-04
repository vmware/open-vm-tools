/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef GETRESPONSEIMPL_H_
#define GETRESPONSEIMPL_H_

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/Envelope.h"
#include "amqpClient/api/GetResponse.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Impelementation of the GetResponse interface
 * <p>
 * A class to bundle together the information from a received message.  The content is
 * packaged into an Envelope, BasicProperties and the body for easier consumption as
 * a single unit.
 */
class GetResponseImpl : public GetResponse {
public:
	GetResponseImpl();
	virtual ~GetResponseImpl();

	/**
	 * @brief Object initializer
	 * @param envelope the message envelope
	 * @param properties the messsage properties
	 * @param body the message body in raw bytes
	 * @param messageCount the number of messages remaining in the queue
	 */
	void init(
		const SmartPtrEnvelope& envelope,
		const AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body,
		const uint32 messageCount);

public: // GetResponse
	SmartPtrEnvelope getEnvelope();
	AmqpContentHeaders::SmartPtrBasicProperties getProperties();
	SmartPtrCDynamicByteArray getBody();
	uint32 getMessageCount();

private:
	bool _isInitialized;
	SmartPtrEnvelope _envelope;
	AmqpContentHeaders::SmartPtrBasicProperties _properties;
	SmartPtrCDynamicByteArray _body;
	uint32 _messageCount;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(GetResponseImpl);
};
CAF_DECLARE_SMART_POINTER(GetResponseImpl);

}}

#endif /* GETRESPONSEIMPL_H_ */
