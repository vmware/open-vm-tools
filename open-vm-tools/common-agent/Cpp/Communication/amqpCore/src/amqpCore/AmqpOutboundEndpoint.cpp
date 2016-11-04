/*
 *  Created on: Jul 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppConfig.h"
#include "Common/IAppContext.h"
#include "IVariant.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "amqpCore/AmqpTemplate.h"
#include "amqpCore/DefaultAmqpHeaderMapper.h"
#include "Exception/CCafException.h"
#include "amqpCore/AmqpOutboundEndpoint.h"
#include "Integration/Core/MessageHeaders.h"

using namespace Caf::AmqpIntegration;

AmqpOutboundEndpoint::AmqpOutboundEndpoint() :
	_isInitialized(false),
	_id(CAFCOMMON_GUID_NULL),
	_expectReply(false),
	_requiresReply(false),
	CAF_CM_INIT("AmqpOutboundEndpoint") {
}

AmqpOutboundEndpoint::~AmqpOutboundEndpoint() {
}

void AmqpOutboundEndpoint::init(
		SmartPtrAmqpTemplate amqpTemplate,
		SmartPtrIAppConfig appConfig,
		SmartPtrIAppContext appContext) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(amqpTemplate);
	CAF_CM_VALIDATE_SMARTPTR(appConfig);
	CAF_CM_VALIDATE_SMARTPTR(appContext);

	_amqpTemplate = amqpTemplate;
	if (!_exchangeName.length() && !_exchangeNameExpression.length()) {
		CAF_CM_EXCEPTIONEX_VA0(
				IllegalStateException,
				0,
				"exchange-name or exchange-name-expression must be set before calling init()");
	}
	if (!_routingKey.length() && !_routingKeyExpression.length()) {
		CAF_CM_EXCEPTIONEX_VA0(
				IllegalStateException,
				0,
				"routing-key or routing-key-expression must be set before calling init()");
	}
	::UuidCreate(&_id);

	if (_exchangeNameExpression.length()) {
		_exchangeNameHandler.CreateInstance();
		_exchangeNameHandler->init(appConfig, appContext, _exchangeNameExpression);
	}

	if (_routingKeyExpression.length()) {
		_routingKeyHandler.CreateInstance();
		_routingKeyHandler->init(appConfig, appContext, _routingKeyExpression);
	}

	if (_requestHeaderMapperExpression.length()) {
		SmartPtrDefaultAmqpHeaderMapper mapper;
		mapper.CreateInstance();
		mapper->init(_requestHeaderMapperExpression);
		_requestHeaderMapper = mapper;
	}

	_isInitialized = true;
}

void AmqpOutboundEndpoint::setExchangeName(const std::string& exchangeName) {
	_exchangeName = exchangeName;
}

void AmqpOutboundEndpoint::setExchangeNameExpression(const std::string& exchangeNameExpression) {
	_exchangeNameExpression = exchangeNameExpression;
}

void AmqpOutboundEndpoint::setRoutingKey(const std::string& routingKey) {
	_routingKey = routingKey;
}

void AmqpOutboundEndpoint::setRoutingKeyExpression(const std::string& routingKeyExpression) {
	_routingKeyExpression = routingKeyExpression;
}
void AmqpOutboundEndpoint::setMappedRequestHeadersExpression(const std::string& expression) {
	_requestHeaderMapperExpression = expression;
}

void AmqpOutboundEndpoint::setExpectReply(const bool expectReply) {
	_expectReply = expectReply;
}

void AmqpOutboundEndpoint::setRequiresReply(const bool requiresReply) {
	_requiresReply = requiresReply;
}

void AmqpOutboundEndpoint::setComponentName(const std::string& name) {
	_componentName = name;
}

void AmqpOutboundEndpoint::setChannelResolver(SmartPtrIChannelResolver channelResolver) {
	_channelResolver = channelResolver;
}

UUID AmqpOutboundEndpoint::getHandlerId() const {
	CAF_CM_FUNCNAME_VALIDATE("getHandlerId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _id;
}

void AmqpOutboundEndpoint::handleMessage(const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("handleMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(message);

	SmartPtrIIntMessage result = handleRequestMessage(message);
	if (result) {
		IIntMessage::SmartPtrCHeaders requestHeaders = message->getHeaders();
		handleResult(result, requestHeaders);
	} else if (_requiresReply) {
		std::stringstream msg;
		msg << "No reply produced by handler '";
		if (_componentName.length()) {
			msg << _componentName;
		} else {
			msg << BasePlatform::UuidToString(_id);
		}
		msg << "', and its 'requiresReply' property is set to true.";
		CAF_CM_EXCEPTIONEX_VA0(
				AmqpIntExceptions::ReplyRequiredException,
				0,
				msg.str().c_str());
	}
}

SmartPtrIIntMessage AmqpOutboundEndpoint::getSavedMessage() const {
	CAF_CM_FUNCNAME_VALIDATE("getSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return NULL;
}

void AmqpOutboundEndpoint::clearSavedMessage() {
	CAF_CM_FUNCNAME_VALIDATE("clearSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
}

SmartPtrIIntMessage AmqpOutboundEndpoint::handleRequestMessage(
		SmartPtrIIntMessage requestMessage) {
	SmartPtrIIntMessage reply;
	std::string exchangeName = _exchangeName;
	std::string routingKey = _routingKey;
	if (_expectReply) {
		reply = sendAndReceive(exchangeName, routingKey, requestMessage);
	} else {
		send(exchangeName, routingKey, requestMessage);
	}
	return reply;
}

void AmqpOutboundEndpoint::handleResult(
		SmartPtrIIntMessage resultMessage,
		IIntMessage::SmartPtrCHeaders requestHeaders) {
	SmartPtrIIntMessage reply = createReplyMessage(resultMessage, requestHeaders);
	sendReplyMessage(
			reply,
			resultMessage->findRequiredHeader(MessageHeaders::_sREPLY_CHANNEL)->toString());
}

void AmqpOutboundEndpoint::send(
		const std::string& exchangeName,
		const std::string& routingKey,
		SmartPtrIIntMessage requestMessage) {
	CAF_CM_FUNCNAME("send");

	std::string resolvedExchange = exchangeName;
	std::string resolvedRoutingKey = routingKey;
	SmartPtrIVariant evalResult;
	if (_exchangeNameHandler) {
		evalResult = _exchangeNameHandler->evaluate(requestMessage);
		if (evalResult) {
			resolvedExchange = evalResult->toString();
		} else {
			CAF_CM_EXCEPTIONEX_VA0(
					AmqpIntExceptions::ExpressionResultNull,
					0,
					"The exchange name was not resolved");
		}
	}

	if (_routingKeyHandler) {
		evalResult = _routingKeyHandler->evaluate(requestMessage);
		if (evalResult) {
			resolvedRoutingKey = evalResult->toString();
		} else {
			CAF_CM_EXCEPTIONEX_VA0(
					AmqpIntExceptions::ExpressionResultNull,
					0,
					"The routing key was not resolved");
		}
	}

	_amqpTemplate->send(
			resolvedExchange,
			resolvedRoutingKey,
			requestMessage,
			_requestHeaderMapper);
}

SmartPtrIIntMessage AmqpOutboundEndpoint::sendAndReceive(
		const std::string& exchangeName,
		const std::string& routingKey,
		SmartPtrIIntMessage requestMessage) {
	CAF_CM_FUNCNAME("sendAndReceive");

	std::string resolvedExchange = exchangeName;
	std::string resolvedRoutingKey = routingKey;
	SmartPtrIVariant evalResult;
	if (_exchangeNameHandler) {
		evalResult = _exchangeNameHandler->evaluate(requestMessage);
		if (evalResult) {
			resolvedExchange = evalResult->toString();
		} else {
			CAF_CM_EXCEPTIONEX_VA0(
					AmqpIntExceptions::ExpressionResultNull,
					0,
					"The exchange name was not resolved");
		}
	}

	if (_routingKeyHandler) {
		evalResult = _routingKeyHandler->evaluate(requestMessage);
		if (evalResult) {
			resolvedRoutingKey = evalResult->toString();
		} else {
			CAF_CM_EXCEPTIONEX_VA0(
					AmqpIntExceptions::ExpressionResultNull,
					0,
					"The routing key was not resolved");
		}
	}

	return _amqpTemplate->sendAndReceive(
			resolvedExchange,
			resolvedRoutingKey,
			requestMessage,
			_requestHeaderMapper,
			_responseHeaderMapper);
}

SmartPtrIIntMessage AmqpOutboundEndpoint::createReplyMessage(
		SmartPtrIIntMessage reply,
		IIntMessage::SmartPtrCHeaders requestHeaders) {
	SmartPtrCIntMessage replyMessage;
	replyMessage.CreateInstance();
	replyMessage->initialize(
			reply->getPayload(),
			reply->getHeaders(),
			requestHeaders);
	return replyMessage;
}

void AmqpOutboundEndpoint::sendReplyMessage(
		SmartPtrIIntMessage reply,
		const std::string& replyChannelHeaderValue) {
	CAF_CM_FUNCNAME("sendReplyMessage");

	if (_outputChannel) {
		_outputChannel->send(reply);
	} else if (replyChannelHeaderValue.length()) {
		if (_channelResolver) {
			SmartPtrIMessageChannel channel =
					_channelResolver->resolveChannelName(replyChannelHeaderValue);
			channel->send(reply);
		} else {
			CAF_CM_EXCEPTIONEX_VA0(
					AmqpIntExceptions::ChannelResolutionException,
					0,
					"No ChannelResolver is available");
		}
	} else {
		CAF_CM_EXCEPTIONEX_VA0(
				AmqpIntExceptions::ChannelResolutionException,
				0,
				"No output-channel or replyChannel header available");
	}
}
