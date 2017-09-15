/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_EXCHANGEIMPL_H_
#define AMQPINTEGRATIONCORE_EXCHANGEIMPL_H_

namespace Caf { namespace AmqpIntegration {

/**
 * @brief Base class for implementations of the #Caf::AmqpIntegration::Exchange interface
 */
class AMQPINTEGRATIONCORE_LINKAGE AbstractExchange : public Exchange {
public:
	AbstractExchange();
	virtual ~AbstractExchange();

	/**
	 * @brief initalize the object
	 * @param name exchange name
	 * @param isDurable <b>true</b> if the exchange is durable else <b>false</b>
	 */
	void init(
			const std::string& name,
			const bool isDurable);

	virtual std::string getName() const;
	virtual std::string getType() const = 0;
	virtual bool isDurable() const;

private:
	std::string _name;
	bool _isDurable;
	CAF_CM_DECLARE_NOCOPY(AbstractExchange);
};

/**
 * @brief Implementation of the #Caf::AmqpIntegration::Exchange interface for direct exchanges
 */
class DirectExchange : public AbstractExchange {
public:
	DirectExchange();

	/**
	 * @brief initalize the object
	 * @param name exchange name
	 * @param durable <b>true</b> if the exchange is durable else <b>false</b>
	 */
	void init(
			const std::string name,
			const bool durable);

	virtual std::string getType() const;
	CAF_CM_DECLARE_NOCOPY(DirectExchange);
};
CAF_DECLARE_SMART_POINTER(DirectExchange);

/**
 * @brief Implementation of the #Caf::AmqpIntegration::Exchange interface for topic exchanges
 */
class TopicExchange : public AbstractExchange {
public:
	TopicExchange();

	/**
	 * @brief initalize the object
	 * @param name exchange name
	 * @param durable <b>true</b> if the exchange is durable else <b>false</b>
	 */
	void init(
			const std::string name,
			const bool durable);

	virtual std::string getType() const;
	CAF_CM_DECLARE_NOCOPY(TopicExchange);
};
CAF_DECLARE_SMART_POINTER(TopicExchange);

/**
 * @brief Implementation of the #Caf::AmqpIntegration::Exchange interface for headers exchanges
 */
class HeadersExchange : public AbstractExchange {
public:
	HeadersExchange();

	/**
	 * @brief initalize the object
	 * @param name exchange name
	 * @param durable <b>true</b> if the exchange is durable else <b>false</b>
	 */
	void init(
			const std::string name,
			const bool durable);

	virtual std::string getType() const;
	CAF_CM_DECLARE_NOCOPY(HeadersExchange);
};
CAF_DECLARE_SMART_POINTER(HeadersExchange);

/**
 * @brief Implementation of the #Caf::AmqpIntegration::Exchange interface for fanout exchanges
 */
class FanoutExchange : public AbstractExchange {
public:
	FanoutExchange();

	/**
	 * @brief initalize the object
	 * @param name exchange name
	 * @param durable <b>true</b> if the exchange is durable else <b>false</b>
	 */
	void init(
			const std::string name,
			const bool durable);

	virtual std::string getType() const;
	CAF_CM_DECLARE_NOCOPY(FanoutExchange);
};
CAF_DECLARE_SMART_POINTER(FanoutExchange);

}}

#endif
