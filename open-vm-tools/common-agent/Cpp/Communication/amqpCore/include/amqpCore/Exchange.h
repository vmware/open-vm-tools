/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_EXCHANGE_H_
#define AMQPINTEGRATIONCORE_EXCHANGE_H_


#include "ICafObject.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @brief Constants representing the exchange types returned by #Caf::AmqpIntegration::Exchange::getType
 */
struct AMQPINTEGRATIONCORE_LINKAGE ExchangeTypes {
	/** @brief direct exchange */
	static const char *DIRECT;
	/** @brief topic exchange */
	static const char *TOPIC;
	/** @brief headers exchange */
	static const char *HEADERS;
	/** @brief fanoout exchange */
	static const char *FANOUT;
};

/**
 * @brief Simple container collecting information to describe an exchange. Used in conjunction with RabbitAdmin.
 * <p>
 * Use #Caf::AmqpIntegration::createDirectExchange, #Caf::AmqpIntegration::createTopicExchange,
 * #Caf::AmqpIntegration::createHeadersExchange or #Caf::AmqpIntegration::createFanoutExchange
 * to create an exchange.
 */
struct __declspec(novtable) Exchange : public ICafObject {
	CAF_DECL_UUID("0F93E16B-E12D-4D00-8DB8-163D57BE2078")

	/** @return the name of the exchange */
	virtual std::string getName() const = 0;

	/** @return the exchange type @see #Caf::AmqpIntegration::ExchangeTypes */
	virtual std::string getType() const = 0;

	/**
	 * @retval true the exchange is durable
	 * @retval false the exchange is not durable
	 */
	virtual bool isDurable() const = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Exchange);

}}

#endif /* AMQPINTEGRATIONCORE_EXCHANGE_H_ */
