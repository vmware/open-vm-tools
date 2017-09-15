/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_AMQPADMIN_H_
#define AMQPINTEGRATIONCORE_AMQPADMIN_H_



#include "ICafObject.h"

#include "amqpCore/Binding.h"
#include "amqpCore/Exchange.h"
#include "amqpCore/Queue.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @brief Specifies a basic set of AMQP administrative operations for AMQP > 0.8
 */
struct __declspec(novtable) AmqpAdmin : public ICafObject {
	CAF_DECL_UUID("B10A94BC-0CC7-476F-A38A-2794CF98D74C")

	/**
	 * @brief Declare an exchange
	 * @param exchange the exchange (#Caf::AmqpIntegration::Exchange) to declare
	 */
	virtual void declareExchange(SmartPtrExchange exchange) = 0;

	/**
	 * @brief Delete an exchange
	 * @param exchange the name of the exchange
	 * @retval true the exchange existed and was deleted
	 * @retval false the exchange did not exists or could not be deleted
	 */
	virtual bool deleteExchange(const std::string& exchange) = 0;

	/**
	 * @brief Declare a queue whose name is automatically generated.
	 * <p>
	 * The queue is created with durable=false, exclusive=true and auto-delete=true.
	 * @return the created queue. Call #Caf::AmqpIntegration::Queue::getName to
	 * get the server-generated name of the queue.
	 */
	virtual SmartPtrQueue declareQueue() = 0;

	/**
	 * @brief Declare a queue
	 * @param queue the queue to declare
	 */
	virtual void declareQueue(SmartPtrQueue queue) = 0;

	/**
	 * @brief Delete a queue without regard for whether it is in use or has messages in it
	 * @param queue the name of the queue
	 * @retval true the queue existed and was deleted
	 * @retval false the queue did not exist or could not be deleted
	 */
	virtual bool deleteQueue(const std::string& queue) = 0;

	/**
	 * @brief Delete a queue
	 * @param queue the name of the queue
	 * @param unused <b>true</b> if the queue should be deleted only if not in use
	 * @param empty <b>true</b> if the queue shoudl be deleted only if empty
	 */
	virtual void deleteQueue(
			const std::string& queue,
			const bool unused,
			const bool empty) = 0;

	/**
	 * @brief Purges the contents of a queue
	 * @param queue the name of the queue
	 */
	virtual void purgeQueue(const std::string& queue) = 0;

	/**
	 * @brief Declare a binding of a queue to an exchange
	 * @param binding the binding information
	 */
	virtual void declareBinding(SmartPtrBinding binding) = 0;

	/**
	 * @brief Remove a binding of a queue to an exchange
	 * @param binding the binding information
	 */
	virtual void removeBinding(SmartPtrBinding binding) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(AmqpAdmin);

}}

#endif
