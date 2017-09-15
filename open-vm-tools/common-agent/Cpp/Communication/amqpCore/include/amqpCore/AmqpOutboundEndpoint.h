/*
 *  Created on: Jul 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_AMQPOUTBOUNDENDPOINT_H_
#define AMQPINTEGRATIONCORE_AMQPOUTBOUNDENDPOINT_H_



#include "Integration/IMessageHandler.h"

#include "Common/IAppConfig.h"
#include "Common/IAppContext.h"
#include "Integration/Core/CExpressionHandler.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpCore/AmqpTemplate.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Adapter that converts and sends Messages to an AMQP exchange
 */
class AMQPINTEGRATIONCORE_LINKAGE AmqpOutboundEndpoint : public IMessageHandler {
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IMessageHandler)
	CAF_END_QI()

public:
	AmqpOutboundEndpoint();
	virtual ~AmqpOutboundEndpoint();

	/**
	 * @brief Initialize this object
	 * @param amqpTemplate the #Caf::AmqpIntegration::AmqpTemplate used to
	 * send and receive messages
	 * @param appConfig the application configuration object
	 * @param appContext the application context object
	 */
	void init(
			SmartPtrAmqpTemplate amqpTemplate,
			SmartPtrIAppConfig appConfig,
			SmartPtrIAppContext appContext);

	/**
	 * @brief Set the name of the AMQP exchange
	 * @param exchangeName the name of the exchange
	 */
	void setExchangeName(const std::string& exchangeName);

	/**
	 * @brief Set the name of the AMQP exchange as an expression of message information
	 * @param exchangeNameExpression the exchange name expression
	 */
	void setExchangeNameExpression(const std::string& exchangeNameExpression);

	/**
	 * @brief Set the routing key
	 * @param routingKey the routing key
	 */
	void setRoutingKey(const std::string& routingKey);

	/**
	 * @brief Set the routing key as an expression of message information
	 * @param routingKeyExpression the routing key expression
	 */
	void setRoutingKeyExpression(const std::string& routingKeyExpression);

	/**
	 * @brief Set the regular expression mapping of headers to send along with the message
	 * <p>
	 * AMQP headers (amqp_XXXX) are always automatically mapped to their AMQP
	 * BasicProperties counterparts.  This expression controls which additional headers
	 * are sent along with the message.
	 * @param expression header mapping expression
	 */
	void setMappedRequestHeadersExpression(const std::string& expression);

	/**
	 * @brief Set the expects reply flag.
	 * @param expectReply <b>true</b> if a reply is expected else <b>false</b>
	 */
	void setExpectReply(const bool expectReply);

	/**
	 * @brief Set the requires reply flag.
	 * @param requiresReply <b>true</b> if a reply is required else <b>false</b>
	 */
	void setRequiresReply(const bool requiresReply);

	/**
	 * @brief Set the friendly name of this instance.
	 * @param name the instance name
	 */
	void setComponentName(const std::string& name);

	/**
	 * @brief Set the channel resolver object
	 * @param channelResolver the channel resolver
	 */
	void setChannelResolver(SmartPtrIChannelResolver channelResolver);

public: // IMessageHandler
	UUID getHandlerId() const;

	void handleMessage(const SmartPtrIIntMessage& message);

	SmartPtrIIntMessage getSavedMessage() const;

	void clearSavedMessage();

private:
	SmartPtrIIntMessage handleRequestMessage(SmartPtrIIntMessage requestMessage);

	void handleResult(
			SmartPtrIIntMessage resultMessage,
			IIntMessage::SmartPtrCHeaders requestHeaders);

	void send(
			const std::string& exchangeName,
			const std::string& routingKey,
			SmartPtrIIntMessage requestMessage);

	SmartPtrIIntMessage sendAndReceive(
			const std::string& exchangeName,
			const std::string& routingKey,
			SmartPtrIIntMessage requestMessage);

	SmartPtrIIntMessage createReplyMessage(
			SmartPtrIIntMessage reply,
			IIntMessage::SmartPtrCHeaders requestHeaders);

	void sendReplyMessage(
			SmartPtrIIntMessage reply,
			const std::string& replyChannelHeaderValue);

private:
	bool _isInitialized;
	UUID _id;
	std::string _componentName;
	SmartPtrAmqpTemplate _amqpTemplate;
	bool _expectReply;
	bool _requiresReply;
	std::string _exchangeName;
	std::string _exchangeNameExpression;
	std::string _routingKey;
	std::string _routingKeyExpression;
	std::string _requestHeaderMapperExpression;
	SmartPtrIChannelResolver _channelResolver;
	SmartPtrIMessageChannel _outputChannel;
	SmartPtrCExpressionHandler _exchangeNameHandler;
	SmartPtrCExpressionHandler _routingKeyHandler;
	SmartPtrAmqpHeaderMapper _requestHeaderMapper;
	SmartPtrAmqpHeaderMapper _responseHeaderMapper;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(AmqpOutboundEndpoint);
};
CAF_DECLARE_SMART_QI_POINTER(AmqpOutboundEndpoint);

}}

#endif
