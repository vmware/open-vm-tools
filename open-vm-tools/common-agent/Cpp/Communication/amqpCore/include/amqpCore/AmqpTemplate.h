/*
 *  Created on: May 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_AMQPTEMPLATE_H_
#define AMQPINTEGRATIONCORE_AMQPTEMPLATE_H_



#include "ICafObject.h"

#include "Integration/IIntMessage.h"
#include "amqpClient/api/Channel.h"
#include "amqpCore/AmqpHeaderMapper.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @brief Specifies a basic set of AMQP operations.
 * <p>
 * Provides synchronous send and receive methods as well as a generic executor
 * through a callback to a provided object.
 */
struct __declspec(novtable) AmqpTemplate : public ICafObject {
	CAF_DECL_UUID("B79DDF8E-B302-4576-9D96-DC413C76392C")

	/**
	 * @brief Send a message to a default exchange with a default routing key.
	 * @param message a message to send
	 * @param headerMapper optional header mapper to use in place of the standard mapper
	 */
	virtual void send(
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper()) = 0;

	/**
	 * @brief Send a message to a default exchange with a routing key.
	 * @param routingKey the routing key
	 * @param message a message to send
	 * @param headerMapper optional header mapper to use in place of the standard mapper
	 */
	virtual void send(
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper()) = 0;

	/**
	 * @brief Send a message to an exchange with a routing key.
	 * @param exchange the name of the exchange
	 * @param routingKey the routing key
	 * @param message a message to send
	 * @param headerMapper optional header mapper to use in place of the standard mapper
	 */
	virtual void send(
			const std::string& exchange,
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper()) = 0;

	/**
	 * @brief Receive a message if there is one from a default queue.
	 * <p>
	 * Returns immediately, possibly with a null value.
	 * @param headerMapper optional header mapper to use in place of the standard mapper
	 */
	virtual SmartPtrIIntMessage receive(
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper()) = 0;

	/**
	 * @brief Receive a message if there is one from a specific queue.
	 * <p>
	 * Returns immediately, possibly with a null value.
	 * @param queueName the name of the queue
	 * @param headerMapper optional header mapper to use in place of the standard mapper
	 */
	virtual SmartPtrIIntMessage receive(
			const std::string& queueName,
			SmartPtrAmqpHeaderMapper headerMapper = SmartPtrAmqpHeaderMapper()) = 0;

	/**
	 * @brief Basic RPC pattern.
	 * <p>
	 * Send a message to a default exchange with a default routing key and attempt to
	 * receive a response. The implementation will create a temporary anonymous queue
	 * to receive the response and will use the repy-to header to notify the recipient
	 * of the response routing.
	 * @param message a message to send
	 * @param requestHeaderMapper optional header mapper to use in place of the standard mapper on the outgoing message
	 * @param responseHeaderMapper optional header mapper to use in place of the standard mapper on the incoming message
	 */
	virtual SmartPtrIIntMessage sendAndReceive(
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper()) = 0;

	/**
	 * @brief Basic RPC pattern.
	 * <p>
	 * Send a message to a default exchange with a specific routing key and attempt to
	 * receive a response. The implementation will create a temporary anonymous queue
	 * to receive the response and will use the repy-to header to notify the recipient
	 * of the response routing.
	 * @param routingKey the routing key
	 * @param message a message to send
	 * @param requestHeaderMapper optional header mapper to use in place of the standard mapper on the outgoing message
	 * @param responseHeaderMapper optional header mapper to use in place of the standard mapper on the incoming message
	 */
	virtual SmartPtrIIntMessage sendAndReceive(
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper()) = 0;

	/**
	 * @brief Basic RPC pattern.
	 * <p>
	 * Send a message to a specific exchange with a specific routing key and attempt to
	 * receive a response. The implementation will create a temporary anonymous queue
	 * to receive the response and will use the repy-to header to notify the recipient
	 * of the response routing.
	 * @param exchange the name of the exchange
	 * @param routingKey the routing key
	 * @param message a message to send
	 * @param requestHeaderMapper optional header mapper to use in place of the standard mapper on the outgoing message
	 * @param responseHeaderMapper optional header mapper to use in place of the standard mapper on the incoming message
	 */
	virtual SmartPtrIIntMessage sendAndReceive(
			const std::string& exchange,
			const std::string& routingKey,
			SmartPtrIIntMessage message,
			SmartPtrAmqpHeaderMapper requestHeaderMapper = SmartPtrAmqpHeaderMapper(),
			SmartPtrAmqpHeaderMapper responseHeaderMapper = SmartPtrAmqpHeaderMapper()) = 0;

	/**
	 * @brief Interface to objects used to execute arbitrary AMQP commands
	 * <p>
	 * This interface provides a mechanism to execute arbitrary AMQP commands. Implement
	 * a class based on this interface with the logic required to support the desired
	 * AMQP operation.  The template will call this object with a channel on which
	 * to execute the command.
	 * @param channel the AMQP channel (Caf::AmqpClient::Channel in the
	 * CAF AMQP Client Library API documentation.)
	 * @param data user-specific data required to perform the operation
	 * @return a pointer to a user-specified object representing the result of the
	 * operation.
	 */
	struct Executor : public ICafObject {
		virtual gpointer execute(AmqpClient::SmartPtrChannel channel, gpointer data) = 0;
	};
	CAF_DECLARE_SMART_POINTER(Executor);

	/**
	 * @brief Execute an arbitrary AMQP operation
	 * @see #Caf::AmqpIntegration::AmqpTemplate::Executor
	 * @param executor the object used to execute the AMQP operation
	 * @param data user-specific data required to perform the operation
	 * @return a pointer to a user-specified object representing the result of the
	 * operation.
	 */
	virtual gpointer execute(SmartPtrExecutor executor, gpointer data) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(AmqpTemplate);

}}

#endif /* AMQPINTEGRATIONCORE_AMQPTEMPLATE_H_ */
