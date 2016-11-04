/*
 *  Created on: Jun 15, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCOREFUNC_H_
#define AMQPINTEGRATIONCOREFUNC_H_



#include "amqpCore/Binding.h"
#include "amqpCore/Exchange.h"
#include "amqpCore/Queue.h"

namespace Caf {

/** @brief CAF AMQP Integration */
namespace AmqpIntegration {

/**
 * @brief Create a #Caf::AmqpIntegration::Queue object
 * <p>
 * <i>durable</i> will be set to <b>true</b>;
 * <i>exclusive</i> and <i>autoDelete</i> will be set to <b>false</b>.
 * @param name the name of the queue
 * @return the queue object
 */
SmartPtrQueue AMQPINTEGRATIONCORE_LINKAGE createQueue(
		const std::string& name);

/**
 * @brief Create a #Caf::AmqpIntegration::Queue object
 * <p>
 * <i>exclusive</i> and <i>autoDelete</i> will be set to <b>false</b>.
 * @param name the name of the queue
 * @param durable <b>true</b> to make the queue durable else <b>false</b>
 * @return the queue object
 */
SmartPtrQueue AMQPINTEGRATIONCORE_LINKAGE createQueue(
		const std::string& name,
		const bool durable);

/**
 * @brief Create a #Caf::AmqpIntegration::Queue object
 * @param name the name of the queue or blank for a server-generated name
 * @param durable <b>true</b> to make the queue durable else <b>false</b>
 * @param exclusive <b>true</b> to make the queue exclusive else <b>false</b>
 * @param autoDelete <b>true</b> to make the queue auto-delete else <b>false</b>
 * @return the queue object
 */
SmartPtrQueue AMQPINTEGRATIONCORE_LINKAGE createQueue(
		const std::string& name,
		const bool durable,
		const bool exclusive,
		const bool autoDelete);


/**
 * @brief Create a #Caf::AmqpIntegration::Exchange object representing a direct exchange
 * @param name the name of the exchange
 * @param durable <b>true</b> to make the exchange durable else <b>false</b>
 */
SmartPtrExchange AMQPINTEGRATIONCORE_LINKAGE createDirectExchange(
		const std::string& name,
		const bool durable = true);

/**
 * @brief Create a #Caf::AmqpIntegration::Exchange object representing a topic exchange
 * @param name the name of the exchange
 * @param durable <b>true</b> to make the exchange durable else <b>false</b>
 */
SmartPtrExchange AMQPINTEGRATIONCORE_LINKAGE createTopicExchange(
		const std::string& name,
		const bool durable = true);

/**
 * @brief Create a #Caf::AmqpIntegration::Exchange object representing a headers exchange
 * @param name the name of the exchange
 * @param durable <b>true</b> to make the exchange durable else <b>false</b>
 */
SmartPtrExchange AMQPINTEGRATIONCORE_LINKAGE createHeadersExchange(
		const std::string& name,
		const bool durable = true);

/**
 * @brief Create a #Caf::AmqpIntegration::Exchange object representing a fanout exchange
 * @param name the name of the exchange
 * @param durable <b>true</b> to make the exchange durable else <b>false</b>
 */
SmartPtrExchange AMQPINTEGRATIONCORE_LINKAGE createFanoutExchange(
		const std::string& name,
		const bool durable = true);

/**
 * @brief Create a #Caf::AmqpIntegration::Binding object
 * @param queue the queue name
 * @param exchange the exchange name
 * @param routingKey the routing key
 */
SmartPtrBinding AMQPINTEGRATIONCORE_LINKAGE createBinding(
		const std::string queue,
		const std::string exchange,
		const std::string routingKey);

}}

#endif
