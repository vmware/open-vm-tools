/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef ENVELOPEIMPL_H_
#define ENVELOPEIMPL_H_

#include "amqpClient/api/Envelope.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of Envelope
 */
class EnvelopeImpl : public Envelope {
public:
	EnvelopeImpl();
	virtual ~EnvelopeImpl();

	/**
	 * @brief Initialize the object
	 * @param deliveryTag delivery tag
	 * @param redelivered redelivered flag
	 * @param exchange exchange name
	 * @param routingKey routing key
	 */
	void init(
		const uint64 deliveryTag,
		const bool redelivered,
		const std::string& exchange,
		const std::string& routingKey);

public: // Envelope
	uint64 getDeliveryTag();
	bool getRedelivered();
	std::string getExchange();
	std::string getRoutingKey();

private:
	bool _isInitialized;
	uint64 _deliveryTag;
	bool _redelivered;
	std::string _exchange;
	std::string _routingKey;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(EnvelopeImpl);
};
CAF_DECLARE_SMART_POINTER(EnvelopeImpl);

}}

#endif /* ENVELOPEIMPL_H_ */
