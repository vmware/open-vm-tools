/*
 *  Created on: Jun 6, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_AMQPHEADERMAPPER_H_
#define AMQPINTEGRATIONCORE_AMQPHEADERMAPPER_H_


#include "ICafObject.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "Integration/IIntMessage.h"
#include "amqpClient/api/Envelope.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @brief This interface is used by objects that map AMQP headers to integration message
 * headers and vice versa.
  */
struct __declspec(novtable) AMQPINTEGRATIONCORE_LINKAGE
AmqpHeaderMapper : public ICafObject {
	CAF_DECL_UUID("5A292DD4-C3CC-4556-9809-90027C13EFA5")

	/*
	 * @brief Return the collection of AMQP headers from an integration message.
	 * @param headers the integration message headers
	 * @return the AMQP headers
	 */
	virtual AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties
		fromHeaders(IIntMessage::SmartPtrCHeaders headers) = 0;

	/**
	 * @brief Return the collection of integration message headers from an AMQP message.
	 * @param properties the AMQP message properties (#Caf::AmqpClient::AmqpContentHeaders::BasicProperties).
	 * @param envelope the AMQP message envelope (#Caf::AmqpClient::Envelope).
	 * @return the filtered set of headers as integration message headers
	 */
	virtual IIntMessage::SmartPtrCHeaders
		toHeaders(
				AmqpClient::AmqpContentHeaders::SmartPtrBasicProperties properties,
				AmqpClient::SmartPtrEnvelope envelope) = 0;

	/**
	 * @brief Return the filtered collection of integration message headers from an integration message.
	 * @param headers the input set of message headers
	 * @return the filtered set of headers as integration message headers
	 */
	virtual IIntMessage::SmartPtrCHeaders
		filterHeaders(
				IIntMessage::SmartPtrCHeaders headers) = 0;

	/** @brief AMQP header prefix */
	static const std::string PREFIX;

	/** @brief AMQP application id header */
	static const std::string APP_ID;

	/** @brief AMQP cluster id header */
	static const std::string CLUSTER_ID;

	/** @brief AMQP content encoding header */
	static const std::string CONTENT_ENCODING;

	/** @brief AMQP content length hedaer */
	static const std::string CONTENT_LENGTH;

	/** @brief AMQP content type header */
	static const std::string CONTENT_TYPE;

	/** @brief AMQP correlation id header */
	static const std::string CORRELATION_ID;

	/** @brief AMQP delivery mode header */
	static const std::string DELIVERY_MODE;

	/** @brief AMQP delivery tag header */
	static const std::string DELIVERY_TAG;

	/** @brief AMQP expiration header */
	static const std::string EXPIRATION;

	/** @brief AMQP message count header */
	static const std::string MESSAGE_COUNT;

	/** @brief AMQP message id header */
	static const std::string MESSAGE_ID;

	/** @brief AMQP received exchange header */
	static const std::string RECEIVED_EXCHANGE;

	/** @brief AMQP routing key header */
	static const std::string RECEIVED_ROUTING_KEY;

	/** @brief AMQP redelivered header */
	static const std::string REDELIVERED;

	/** @brief AMQP reply to header */
	static const std::string REPLY_TO;

	/** @brief AMQP timestamp header */
	static const std::string TIMESTAMP;

	/** @brief AMQP type header */
	static const std::string TYPE;

	/** @brief AMQP user id header */
	static const std::string USER_ID;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(AmqpHeaderMapper);

}}

#endif
