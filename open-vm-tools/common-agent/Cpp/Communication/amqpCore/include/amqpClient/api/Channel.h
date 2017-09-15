/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_CHANNEL_H_
#define AMQPCLIENTAPI_CHANNEL_H_


#include "ICafObject.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/api/AmqpMethods.h"
#include "amqpClient/api/Consumer.h"
#include "amqpClient/api/GetResponse.h"
#include "amqpClient/api/ReturnListener.h"
#include "amqpClient/api/amqpClient.h"
#include "amqpClient/api/AmqpContentHeaders.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Interface to an AMQP channel
 */
struct __declspec(novtable) Channel : public ICafObject {

	/** @return the channel number */
	virtual uint16 getChannelNumber() = 0;

	/** @brief Closes the channel */
	virtual void close() = 0;

	/**
	 * @brief Returns the state of the channel
	 * @retval true the Channel is open
	 * @retval false the Channel is closed
	 */
	virtual bool isOpen() = 0;

	/**
	 * @brief Acknowledge on or more messages
	 * <p>
	 * Acknowledges one or more messages delivered via the Deliver or
	 * Get-Ok methods. The client can ask to confirm a single message or a set of
	 * messages up to and including a specific message.
	 *
	 * @param deliveryTag the message's delivery tag
	 * @param ackMultiple acknowledge multiple messages<p>
	 * If set to <code><b>true</b></code>, the delivery tag is treated as "up to and
	 * including", so that the client can acknowledge multiple messages with a single
	 * method. If set to <code><b>false</b></code>, the delivery tag refers to a single
	 * message. If the multiple field is <code><b>true</b></code>, and the delivery tag
	 * is <code><b>0</b></code>, the server will acknowledge all outstanding messages.
	 */
	virtual void basicAck(
		const uint64 deliveryTag,
		const bool ackMultiple) = 0;

	/**
	 * @brief Provides a direct access to the messages in a queue
	 * <p>
	 * This method provides a direct access to the messages in a queue using a
	 * synchronous dialogue that is designed for specific types of application where
	 * synchronous functionality is more important than performance.
	 *
	 * @param queue the queue name
	 * @param noAck no acknowledgment needed. If <code><b>true</b></code>, the server does not
	 * expect acknowledgements for messages. That is, when a message is delivered to the client
	 * the server assumes the delivery will succeed and immediately dequeues it. This
	 * functionality may increase performance but at the cost of reliability. Messages can get
	 * lost if a client dies before they are delivered to the application.
	 * @return message as a GetResponse object or
	 * a <code><b>null</b></code> object if there is no message available.
	 */
	virtual SmartPtrGetResponse basicGet(
		const std::string& queue,
		const bool noAck) = 0;

	/**
	 * @brief Publishes a message to a specific exchange
	 * <p>
	 * This method publishes a message to a specific exchange. The message will be routed
	 * to queues as defined by the exchange configuration and distributed to any active
	 * consumers when the transaction, if any, is committed.
	 * @param exchange specifies the name of the exchange to publish to. The exchange
	 * name can be empty, meaning the default exchange.
	 * @param routingKey specifies the routing key for the message.
	 * @param properties specifies a #Caf::AmqpClient::AmqpContentHeaders::BasicProperties
	 * object containing properties and headers to publish with the message.
	 * @param body specifies the message body in raw bytes.
	 */
	virtual void basicPublish(
		const std::string& exchange,
		const std::string& routingKey,
		const AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body) = 0;

	/**
	 * @brief Publishes a message to a specific exchange
	 * <p>
	 * This method publishes a message to a specific exchange with control over the
	 * <code><i>mandatory</i></code> and <code><i>immediate</i></code> bits.
	 * The message will be routed to queues as defined by the exchange configuration and
	 * distributed to any active consumers when the transaction, if any, is committed.
	 * @param exchange specifies the name of the exchange to publish to. The exchange
	 * name can be empty, meaning the default exchange.
	 * @param routingKey specifies the routing key for the message.
	 * @param mandatory specifies how the server is to react if the message cannot be routed
	 * to a queue. If <code><b>true</b></code>, the server will return an unroutable message
	 * with a Return method. If <code><b>false</b></code>, the server silently drops the
	 * message.
	 * @param immediate specifies how the server is to react if the message cannot be routed
	 * to a queue consumer immediately. If <code><b>true</b></code>, the server will return an
	 * undeliverable message with a Return method. If <code><b>false</b></code>,
	 * the server will queue the message, but with no guarantee that it will ever be
	 * consumed.
	 * @param properties specifies a #Caf::AmqpClient::AmqpContentHeaders::BasicProperties
	 * object containing properties and headers to publish with the message.
	 * @param body specifies the message body in raw bytes.
	 */
	virtual void basicPublish(
		const std::string& exchange,
		const std::string& routingKey,
		const bool mandatory,
		const bool immediate,
		const AmqpContentHeaders::SmartPtrBasicProperties& properties,
		const SmartPtrCDynamicByteArray& body) = 0;

	/**
	 * @brief Starts a queue consumer
	 * <p>
	 * This method asks the server to start a 'consumer', which is a transient request for
	 * messages from a specific queue.  Consumers last as int32 as the channel they were
	 * declared on, or until the client cancels them.<br>
	 * The arguments <code><i>noAck</i></code>, <code><i>noLocal</i></code> and
	 * <code><i>exclusive</i></code> are <code><b>false</b></code> and the server will
	 * genearate the consumer tag.
	 * @param queue queue name
	 * @param consumer an interface to the consumer object
	 * @return a #Caf::AmqpClient::AmqpMethods::Basic::ConsumeOk object containing
	 * the results of the call if successful
	 */
	virtual AmqpMethods::Basic::SmartPtrConsumeOk basicConsume(
			const std::string& queue,
			const SmartPtrConsumer& consumer) = 0;

	/**
	 * @brief Starts a queue consumer
	 * <p>
	 * This method asks the server to start a 'consumer', which is a transient request for
	 * messages from a specific queue.  Consumers last as int32 as the channel they were
	 * declared on, or until the client cancels them.<br>
	 * The arguments <code><i>noLocal</i></code> and <code><i>exclusive</i></code> are
	 * <code><b>false</b></code> and the server will genearate the consumer tag.
	 * @param queue queue name
	 * @param noAck no acknowledgment needed. If <code><b>true</b></code>, the server does not
	 * expect acknowledgements for messages. That is, when a message is delivered to the client
	 * the server assumes the delivery will succeed and immediately dequeues it. This
	 * functionality may increase performance but at the cost of reliability. Messages can get
	 * lost if a client dies before they are delivered to the application.
	 * @param consumer an interface to the consumer object
	 * @return a #Caf::AmqpClient::AmqpMethods::Basic::ConsumeOk object containing
	 * the results of the call if successful
	 */
	virtual AmqpMethods::Basic::SmartPtrConsumeOk basicConsume(
			const std::string& queue,
			const bool noAck,
			const SmartPtrConsumer& consumer) = 0;

	/**
	 * @brief Starts a queue consumer
	 * <p>
	 * This method asks the server to start a 'consumer', which is a transient request for
	 * messages from a specific queue.  Consumers last as int32 as the channel they were
	 * declared on, or until the client cancels them.
	 * @param queue queue name
	 * @param consumerTag consumer tag (or blank to specify server-generated tag)
	 * @param noAck acknowledgement flag
	 * @param noLocal do not send messages to the connection that published them
	 * @param exclusive request exclusive consumer access to the queue
	 * @param consumer an interface to the consumer object
	 * @param arguments a set of arguments for the declaration. The syntax of these
	 * arguments depends on the server implementation.
	 * @return a #Caf::AmqpClient::AmqpMethods::Basic::ConsumeOk object containing
	 * the results of the call if successful
	 */
	virtual AmqpMethods::Basic::SmartPtrConsumeOk basicConsume(
			const std::string& queue,
			const std::string& consumerTag,
			const bool noAck,
			const bool noLocal,
			const bool exclusive,
			const SmartPtrConsumer& consumer,
			const SmartPtrTable& arguments = SmartPtrTable()) = 0;

	/**
	 * @brief Cancels a consumer
	 * <p>
	 * This method cancels a consumer. This does not affect already delivered messages,
	 * but it does mean the server will not send any more messages for that consumer.
	 * The client may receive an arbitrary number of messages in between sending the cancel
	 * method and receiving the cancel.ok reply.
	 * @param consumerTag consumer tag to cancel
	 * @return a #Caf::AmqpClient::AmqpMethods::Basic::CancelOk object containing
	 * the results of the call if successful
	 */
	virtual AmqpMethods::Basic::SmartPtrCancelOk basicCancel(
			const std::string& consumerTag) = 0;

	/**
	 * @brief Redeliver unacknowledged messages
	 * <p>
	 * This method asks the server to redeliver all unacknowledged message on the channel.
	 * Zero or more messages may be redelivered.
	 * @param requeue if <code><b>false</b></code> then the message will be redelivered to
	 * the original receipient.  If <code><b>true</b></code> then the server will attempt to
	 * requeue the message, potentially delivering it to an alternate subscriber.
	 * @return a #Caf::AmqpClient::AmqpMethods::Basic::RecoverOk object containing the
	 * results of the call if successful
	 */
	virtual AmqpMethods::Basic::SmartPtrRecoverOk basicRecover(
			const bool requeue) = 0;

	/**
	 * @brief Specifies quality of service
	 * <p>
	 * This method requests a specific quality of service.  The QoS can be specified for
	 * the current channel or for all channels on the connection. The particular properties
	 * and semantics of a qos method always depend on the content class semantics.
	 * @param prefetchSize prefetch window in octets. The client can request that messages
	 * be sent in advance so that when the client finishes processing a message,
	 * the following message is already help locally, rather than needing to be sent down
	 * the channel. Prefetching gives a performance improvement. This field specifies the
	 * prefetch window size in octets. The server will send a message in advance if it is
	 * equal to or smaller in size than the available prefetch size (and also falls into
	 * other prefetch limits). May be set to zero, meaning 'no specific limit', although
	 * other prefetch limits may still apply.  The prefetch-size is ignored if the no-ack
	 * option is set.
	 * @param prefetchCount prefetch window in messages. This field may be used in combination
	 * with the <i>prefetchSize</i> field; a message will only be sent in advance if both
	 * prefetch windows (and those at the channel and connection level) allow it. The
	 * prefetch-count is ignored if the no-ack option is set.
	 * @param global apply to entire connection
	 * @return
	 */
	virtual AmqpMethods::Basic::SmartPtrQosOk basicQos(
			const uint32 prefetchSize,
			const uint32 prefetchCount,
			const bool global) = 0;

	/**
	 * @brief Reject an incoming message
	 * <p>
	 * This method allows a client to reject a message. It can be used to interrupt and
	 * cancel large incoming messages, or return un-treatable messages to their original
	 * queue.
	 * @param deliveryTag the delivery tag of the message
	 * @param requeue if <code><b>true</b></code>, the server will attempt to requeue
	 * the message.  If <code><b>false</b></code> or the requeue attempt fails the
	 * message is discarded or dead-lettered.
	 */
	virtual void basicReject(
			const uint64 deliveryTag,
			const bool requeue) = 0;

	/**
	 * @brief Creates an exchange
	 * <p>
	 * This method creates an exchange if it does not already exist, and if the exchange
	 * exists, verifies that it is of the correct and expected class.
	 * @param exchange exchange name
	 * @param type exchange type
	 * @param durable request a durable exchange
	 * @param arguments a set of agrguments for the declaration. The syntax of these
	 * arguments depends on the server implementation.
	 * @return a #Caf::AmqpClient::AmqpMethods::Exchange::DeclareOk object containing
	 * the results of the call if successful
	 */
	virtual AmqpMethods::Exchange::SmartPtrDeclareOk exchangeDeclare(
		const std::string& exchange,
		const std::string& type,
		const bool durable = false,
		const SmartPtrTable& arguments = SmartPtrTable()) = 0;

	/**
	 * @brief Deletes an exchange
	 * <p>
	 * This method deletes an exchange. When an exchange is deleted all queue bindings on
	 * the exchange are cancelled.
	 * @param exchange exchange name
	 * @param ifUnused delete only if unused. If <code><b>true</b></code>, the server
	 * will only delete the exchange if it has no queue bindings. If the exchange has
	 * queue bindings the server does not delete it but raises a channel exception
	 * instead.
	 * @return a #Caf::AmqpClient::AmqpMethods::Exchange::DeleteOk object containing
	 * the results of the call if successful
	 */
	virtual AmqpMethods::Exchange::SmartPtrDeleteOk exchangeDelete(
		const std::string& exchange,
		const bool ifUnused) = 0;

	/**
	 * @brief Creates a queue using default parameters
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
	 * @return a #Caf::AmqpClient::AmqpMethods::Queue::DeclareOk object containing
	 * the results of the call if successful. This object must be examined to retrieve
	 * the name of the queue generated by the server.
	 */
	virtual AmqpMethods::Queue::SmartPtrDeclareOk queueDeclare() = 0;

	/**
	 * @brief Creates or checks a queue
	 * <p>
	 * @param queue queue name. If blank the server will generate a name.
	 * @param durable request a durable queue
	 * @param exclusive request an exclusive queue
	 * @param autoDelete request that the queue be deleted when no longer in use
	 * @param arguments a set of agrguments for the declaration. The syntax of these
	 * arguments depends on the server implementation.
	 * @return a #Caf::AmqpClient::AmqpMethods::Queue::DeclareOk object containing
	 * the results of the call if successful. This object must be examined to retrieve
	 * the name of the queue generated by the server if the queue name was blank in
	 * the call.
	 */
	virtual AmqpMethods::Queue::SmartPtrDeclareOk queueDeclare(
		const std::string& queue,
		const bool durable,
		const bool exclusive,
		const bool autoDelete,
		const SmartPtrTable& arguments = SmartPtrTable()) = 0;

	/**
	 * @brief Declare a queue passively; i.e. check if it exists.
	 * <p>
	 * @param queue queue name.
	 */
	virtual AmqpMethods::Queue::SmartPtrDeclareOk queueDeclarePassive(
			const std::string& queue) = 0;

	/**
	 * @brief Deletes a queue
	 * <p>
	 * This method deletes a queue. When a queue is deleted any pending messages are
	 * sent to a dead¬≠-letter queue if this is defined in the server configuration,
	 * and all consumers on the queue are cancelled.
	 * @param queue queue name
	 * @param ifUnused delete only if unused. If <code><b>true</b></code>, the server
	 * will only delete the queue if it has no consumers. If the queue has consumers the
	 * server does does not delete it but raises a channel exception instead.
	 * @param ifEmpty delete only if empty. If <code><b>true</b></code>, the server will
	 * only delete the queue if it has no messages. If the queue has messages the
	 * server does does not delete it but raises a channel exception instead.
	 * @return a #Caf::AmqpClient::AmqpMethods::Queue::DeleteOk object containing the
	 * result of the call if successful.
	 */
	virtual AmqpMethods::Queue::SmartPtrDeleteOk queueDelete(
		const std::string& queue,
		const bool ifUnused,
		const bool ifEmpty) = 0;

	/**
	 * @brief Purges a queue
	 * <p>
	 * This method removes all messages from a queue which are not awaiting
	 * acknowledgment.
	 * @param queue queue name
	 * @return a #Caf::AmqpClient::AmqpMethods::Queue::PurgeOk containing the result
	 * of the call if successful.
	 */
	virtual AmqpMethods::Queue::SmartPtrPurgeOk queuePurge(
		const std::string& queue) = 0;

	/**
	 * @brief Binds a queue to an exchange
	 * <p>
	 * This method binds a queue to an exchange. Until a queue is bound it will not
	 * receive any messages.
	 * @param queue queue name
	 * @param exchange exchange name
	 * @param routingKey message routing key
	 * @param arguments a set of agrguments for the binding. The syntax of these
	 * arguments depends on the server implementation.
	 * @return a #Caf::AmqpClient::AmqpMethods::Queue::BindOk containing the result
	 * of the call if successful.
	 */
	virtual AmqpMethods::Queue::SmartPtrBindOk queueBind(
		const std::string& queue,
		const std::string& exchange,
		const std::string& routingKey,
		const SmartPtrTable& arguments = SmartPtrTable()) = 0;

	/**
	 * @brief Unbinds a queue from an exchange
	 * @param queue queue name
	 * @param exchange exchange name
	 * @param routingKey message routing key
	 * @param arguments a set of agrguments for the binding. The syntax of these
	 * arguments depends on the server implementation.
	 * @return a #Caf::AmqpClient::AmqpMethods::Queue::UnbindOk containing the result
	 * of the call if successful.
	 */
	virtual AmqpMethods::Queue::SmartPtrUnbindOk queueUnbind(
		const std::string& queue,
		const std::string& exchange,
		const std::string& routingKey,
		const SmartPtrTable& arguments = SmartPtrTable()) = 0;

	/**
	 * @brief Adds a {@link ReturnListener} to the channel
	 * @param listener the {@link ReturnListener} object to add
	 */
	virtual void addReturnListener(
			const SmartPtrReturnListener& listener) = 0;

	/**
	 * @brief Removes a {@link ReturnListener} from the channel
	 * @param listener the {@link ReturnListener} to remove
	 */
	virtual bool removeReturnListener(
			const SmartPtrReturnListener& listener) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Channel);

}}

#endif
