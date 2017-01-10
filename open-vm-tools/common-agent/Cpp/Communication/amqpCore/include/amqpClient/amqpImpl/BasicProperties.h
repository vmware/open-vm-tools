/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICPROPERTIES_H_
#define BASICPROPERTIES_H_

#include "amqpClient/api/amqpClient.h"
#include "amqpClient/amqpImpl/IContentHeader.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/api/AmqpContentHeaders.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic properties
 */
class BasicProperties :
	public AmqpContentHeaders::BasicProperties,
	public IContentHeader {
	CAF_DECL_UUID("db3dbcd6-f241-47ff-b17c-d5cf6addabf8")

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(AmqpContentHeaders::BasicProperties)
		CAF_QI_ENTRY(IContentHeader)
		CAF_QI_ENTRY(BasicProperties)
	CAF_END_QI()
public:
	BasicProperties();
	virtual ~BasicProperties();

	/**
	 * @brief Initialize the properties
	 * <p>
	 * This version creates an object with no properties. Call the setXXX methods
	 * to add properties.
	 */
	void init();

	/**
	 * @brief Initialize the properties
	 * <p>
	 * Set the <code><i>flags</i></code> property to the binary 'or' (<code><b>|</b></code>) of
	 * BASIC_PROPERTY_XXXX_FLAG constants representing the fields present.
	 * <p>
	 * Use <code><b>0</b></code>, <code><b>std::string()</b></code> or
	 * <code><b>SmartPtrTable()</b></code> to skip initialization of fields not included
	 * in the object.
	 */
	void init(
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

	/**
	 * @brief Converts this class instance into a c-api amqp_basic_properties_t struct.
	 * <p>
	 * The pointers in the c-api struct point at member of this class.  This class instance
	 * must be realized as int32 as the return <code><i>properties</i></code> is in use.
	 * @param properties the c-api properties structure to be filled out
	 */
	void getAsApiProperties(amqp_basic_properties_t& properties);

public: // IContentHeader
	void init(const SmartPtrCAmqpFrame& frame);

	uint64 getBodySize();

public: // AmqpProperties::BasicProperties,
	bool areHeadersAvailable();
	uint32 getFlags();
	std::string getContentType();
	void setContentType(const std::string& contentType);
	std::string getContentEncoding();
	void setContentEncoding(const std::string& contentEncoding);
	SmartPtrTable getHeaders();
	void setHeaders(const SmartPtrTable& headers);
	uint8 getDeliveryMode();
	void setDeliveryMode(const uint8 deliveryMode);
	uint8 getPriority();
	void setPriority(const uint8 priority);
	std::string getCorrelationId();
	void setCorrelationId(const std::string& correlationId);
	std::string getReplyTo();
	void setReplyTo(const std::string& replyTo);
	std::string getExpiration();
	void setExpiration(const std::string& expiration);
	std::string getMessageId();
	void setMessageId(const std::string& messageId);
	uint64 getTimestamp();
	void setTimestamp(const uint64 timestamp);
	std::string getType();
	void setType(const std::string& type);
	std::string getUserId();
	void setUserId(const std::string& userId);
	std::string getAppId();
	void setAppId(const std::string& appId);
	std::string getClusterId();
	void setClusterId(const std::string& clusterId);

public: // IAmqpContentHeader
	uint16 getClassId();

	std::string getClassName();

private:
	void ValidatePropertyIsSet(
		const uint32 flag,
		const char* propertyName);

private:
	bool _isInitialized;
	uint32 _flags;
	uint64 _bodySize;
	std::string _contentType;
	std::string _contentEncoding;
	SmartPtrTable _headers;
	uint8 _deliveryMode;
	uint8 _priority;
	std::string _correlationId;
	std::string _replyTo;
	std::string _expiration;
	std::string _messageId;
	uint64 _timestamp;
	std::string _type;
	std::string _userId;
	std::string _appId;
	std::string _clusterId;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicProperties);
};
CAF_DECLARE_SMART_QI_POINTER(BasicProperties);

}}

#endif /* BASICPROPERTIES_H_ */
