/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_QUEUEIMPL_H_
#define AMQPINTEGRATIONCORE_QUEUEIMPL_H_

namespace Caf { namespace AmqpIntegration {

/**
 * @brief Implementation of the #Caf::AmqpIntegration::Queue interface.
 */
class AMQPINTEGRATIONCORE_LINKAGE QueueImpl : public Queue {
public:
	QueueImpl();
	virtual ~QueueImpl();

	/**
	 * @brief initialize the object
	 * <p>
	 * <i>durable</i> will be set to <b>true</b>;
	 * <i>exclusive</i> and <i>autoDelete</i> will be set to <b>false</b>.
	 * @param name queue name
	 */
	void init(const std::string& name);

	/**
	 * @brief initialize the object
	 * <p>
	 * <i>exclusive</i> and <i>autoDelete</i> will be set to <b>false</b>.
	 * @param name queue name
	 * @param durable <b>true</b> to make the queue durable else <b>false</b>
	 */
	void init(
			const std::string& name,
			const bool durable);

	/**
	 * @brief initialize the object
	 * @param name queue name
	 * @param durable <b>true</b> to make the queue durable else <b>false</b>
	 * @param exclusive <b>true</b> to make the queue exclusive else <b>false</b>
	 * @param autoDelete <b>true</b> to make the queue auto-delete else <b>false</b>
	 */
	void init(
			const std::string& name,
			const bool durable,
			const bool exclusive,
			const bool autoDelete);

	std::string getName() const;

	bool isDurable() const;

	bool isExclusive() const;

	bool isAutoDelete() const;

private:
	std::string _name;
	bool _durable;
	bool _exclusive;
	bool _autoDelete;
	CAF_CM_DECLARE_NOCOPY(QueueImpl);
};
CAF_DECLARE_SMART_POINTER(QueueImpl);

}}

#endif
