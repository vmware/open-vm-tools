/*
 *  Created on: Jun 6, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_DEFAULTAMQPHEADERMAPPER_H_
#define AMQPINTEGRATIONCORE_DEFAULTAMQPHEADERMAPPER_H_

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Common/CCafRegex.h"
#include "Integration/IIntMessage.h"
#include "amqpClient/api/Envelope.h"
#include "amqpCore/AmqpHeaderMapper.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @brief This class is used to map AMQP headers to integration message
 * headers and vice versa.
 * <p>
 * This class maps headers between the AMQP message protocol and the internal
 * integration object message protocol.  By default, standard AMQP headers are mapped and
 * internal and user-defined headers are not. An optional regular expression can be
 * supplied to allow user-defined headers to be mapped from integration messages
 * to AMQP.
 */
class AMQPINTEGRATIONCORE_LINKAGE DefaultAmqpHeaderMapper : public AmqpHeaderMapper {
public:
	DefaultAmqpHeaderMapper();
	virtual ~DefaultAmqpHeaderMapper();

	/**
	 * @brief Object initializer
	 * @param userHeaderRegex the regular expression to apply to the integration
	 * message header keys. Those that match are mapped to the AMQP message headers.
	 */
	void init(const std::string& userHeaderRegex = std::string());

	virtual AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties
		fromHeaders(IIntMessage::SmartPtrCHeaders headers);

	virtual IIntMessage::SmartPtrCHeaders
		toHeaders(
				AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties properties,
				AmqpClient::SmartPtrEnvelope envelope);

	virtual IIntMessage::SmartPtrCHeaders
		filterHeaders(
				IIntMessage::SmartPtrCHeaders headers);

private:
	bool _isInitialized;
	SmartPtrCCafRegex _userHeaderRegex;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(DefaultAmqpHeaderMapper);
};
CAF_DECLARE_SMART_POINTER(DefaultAmqpHeaderMapper);

}}

#endif /* AMQPINTEGRATIONCORE_DEFAULTAMQPHEADERMAPPER_H_ */
