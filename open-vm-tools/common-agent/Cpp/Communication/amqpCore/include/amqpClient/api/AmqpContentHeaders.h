/*
 *  Created on: May 14, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_AMQPCONTENTHEADERS_H_
#define AMQPCLIENTAPI_AMQPCONTENTHEADERS_H_

#include "amqpClient/api/amqpClient.h"
#include "amqpClient/amqpImpl/BasicProperties.h"

namespace Caf { namespace AmqpClient {

/**
 * @ingroup AmqpApi
 * @brief AMQP content header objects */
namespace AmqpContentHeaders {

/** @brief content type property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_CONTENT_TYPE_FLAG;
/** @brief content encoding property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_CONTENT_ENCODING_FLAG;
/** @brief headers are present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_HEADERS_FLAG;
/** @brief delivery mode property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_DEVLIVERY_MODE_FLAG;
/** @brief priority property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_PRIORITY_FLAG;
/** @brief correlation id property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_CORRELATION_ID_FLAG;
/** @brief reply to property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_REPLY_TO_FLAG;
/** @brief expiration property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_EXPIRATION_FLAG;
/** @brief message id property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_MESSAGE_ID_FLAG;
/** @brief timestamp property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_TIMESTAMP_FLAG;
/** @brief type property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_TYPE_FLAG;
/** @brief user id property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_USER_ID_FLAG;
/** @brief app id property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_APP_ID_FLAG;
/** @brief cluster id property is present */
extern AMQPCLIENT_LINKAGE const uint32 BASIC_PROPERTY_CLUSTER_ID_FLAG;

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Basic properties class
 * <p>
 * Review the AMQP protocol documentation for more information.
 */
struct __declspec(novtable) BasicProperties : public ContentHeader {
	CAF_DECL_UUID("A6DEE271-36C7-4B46-8EA8-F1F0E3493FC4")

	/** @return are the headers available? */
	virtual bool areHeadersAvailable() = 0;

	/**
	 * @remark
	 * Use the binary 'and' (<code><b>&</b></code>) to test the flags against the
	 * BASIC_PROPERTY_XXXXX_FLAGS
	 * @return the bits representing the properties present in the object
	 */
	virtual uint32 getFlags() = 0;

	/** @return the content type */
	virtual std::string getContentType() = 0;

	/** @brief Set the content type */
	virtual void setContentType(const std::string& contentType) = 0;

	/** @return the content encoding */
	virtual std::string getContentEncoding() = 0;

	/** @brief Set the content encoding */
	virtual void setContentEncoding(const std::string& contentEncoding) = 0;

	/** @return the message headers */
	virtual SmartPtrTable getHeaders() = 0;

	/** @brief Set the headers */
	virtual void setHeaders(const SmartPtrTable& headers) = 0;

	/** @return the delivery mode */
	virtual uint8 getDeliveryMode() = 0;

	/** @brief Set the delivery mode */
	virtual void setDeliveryMode(const uint8 deliveryMode) = 0;

	/** @return the priority */
	virtual uint8 getPriority() = 0;

	/** @brief Set the priority */
	virtual void setPriority(const uint8 priority) = 0;

	/** @return the correlation id */
	virtual std::string getCorrelationId() = 0;

	/** @brief Set the correlation id */
	virtual void setCorrelationId(const std::string& correlationId) = 0;

	/** @return the reply to */
	virtual std::string getReplyTo() = 0;

	/** @brief Set the reply to */
	virtual void setReplyTo(const std::string& replyTo) = 0;

	/** @return the expiration */
	virtual std::string getExpiration() = 0;

	/** @brief Set the expiration */
	virtual void setExpiration(const std::string& expiration) = 0;

	/** @return the message id */
	virtual std::string getMessageId() = 0;

	/** @brief Set the message id */
	virtual void setMessageId(const std::string& messageId) = 0;

	/** @return the timestamp */
	virtual uint64 getTimestamp() = 0;

	/** @brief Set the timestamp */
	virtual void setTimestamp(const uint64 timestamp) = 0;

	/** @return the type */
	virtual std::string getType() = 0;

	/** @brief set the type */
	virtual void setType(const std::string& type) = 0;

	/** @return the user id */
	virtual std::string getUserId() = 0;

	/** @brief Set the user id */
	virtual void setUserId(const std::string& userId) = 0;

	/** @return the app id */
	virtual std::string getAppId() = 0;

	/** @brief Set the app id */
	virtual void setAppId(const std::string& appId) = 0;

	/** @return the cluster id */
	virtual std::string getClusterId() = 0;

	/** @brief Set the cluster id */
	virtual void setClusterId(const std::string& clusterId) = 0;
};
CAF_DECLARE_SMART_POINTER(BasicProperties);

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Create a #Caf::AmqpClient::AmqpContentHeaders::BasicProperties object to be
 * sent with a message.
 * <p>
 * The object is created with no fields set.  You must call the <code>setXXX</code>
 * methods before publishing the message.
 * @return an unpopulated basic properties object
 */
SmartPtrBasicProperties AMQPCLIENT_LINKAGE createBasicProperties();

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Create an object containing #Caf::AmqpClient::AmqpContentHeaders::BasicProperties
 * to be sent with a message
 * <p>
 * Set the <code><i>flags</i></code> property to the binary 'or' (<code><b>|</b></code>) of
 * BASIC_PROPERTY_XXXX_FLAG constants representing the fields present.
 * <p>
 * Use <code><b>0</b></code>, <code><b>std::string()</b></code> or
 * <code><b>SmartPtrTable()</b></code> to skip initialization of fields not included
 * in the object.
 * @return a basic properties object
 */
SmartPtrBasicProperties AMQPCLIENT_LINKAGE createBasicProperties(
	const uint32 flags,
	const std::string& contentType,
	const std::string& contentEncoding,
	const SmartPtrTable& headers,
	const uint8 deliveryMode,
	const uint8 priority,
	const std::string& correlationId,
	const std::string& replyTo,
	const std::string& expiration,
	const std::string& messageId,
	const uint64 timestamp,
	const std::string& type,
	const std::string& userId,
	const std::string& appId,
	const std::string& clusterId);
}}}

#endif
