/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CONSUMER_H_
#define CONSUMER_H_

#include "ICafObject.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Exception/CCafException.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/Envelope.h"
#include "amqpClient/api/AmqpContentHeaders.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Interface for application callback objects to receive notifications and messages
 * from a queue by subscription.
 * <p>
 * The methods of this interface are invoked in a dispatch thread which is separate from the
 * {@link Connection}'s thread.  This allows {@link Consumer}s to call {@link Channel} or
 * {@link Connection} methods without causing a deadlock.
 */
struct __declspec(novtable) Consumer : public ICafObject {

	/**
	 * @brief Called when the consumer is registered by a call to any of the
	 * {@link Channel#basicConsume} methods
	 * @param consumerTag the <i>consumer tag</i> associated with the consumer
	 */
	virtual void handleConsumeOk(
			const std::string& consumerTag) = 0;

	/**
	 * @brief Called when the consumer is cancelled by a call to {@link Channel#basicCancel}.
	 * @param consumerTag the <i>consumer tag</i> associated with the consumer
	 */
	virtual void handleCancelOk(
			const std::string& consumerTag) = 0;

	/**
	 * @brief Called when a <code><b>basic.recover-ok</b></code> is received.
	 * @param consumerTag the <i>consumer tag</i> associated with the consumer
	 */
	virtual void handleRecoverOk(
			const std::string& consumerTag) = 0;

	/**
	 * @brief Called when a <code><b>basic.deliver</b></code> is received for this consumer.
	 * @param consumerTag the <i>consumer tag</i> associated with the consumer
	 * @param envelope message envelope
	 * @param properties message properties and headers
	 * @param body message body
	 */
	virtual void handleDelivery(
			const std::string& consumerTag,
			const SmartPtrEnvelope& envelope,
			const AmqpContentHeaders::SmartPtrBasicProperties& properties,
			const SmartPtrCDynamicByteArray& body) = 0;

	/**
	 * @brief Called when the channel has been shut down.
	 * @param consumerTag the <i>consumer tag</i> associated with the consumer
	 * @param reason the reason for the shut down
	 */
	virtual void handleShutdown(
			const std::string& consumerTag,
			SmartPtrCCafException& reason) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Consumer);

}}

#endif /* CONSUMER_H_ */
