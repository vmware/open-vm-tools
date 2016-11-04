/*
 *  Created on: Jul 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpCore/AmqpOutboundEndpoint.h"
#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationAppContext.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageChannel.h"
#include "amqpCore/AmqpTemplate.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#include "AmqpOutboundEndpointInstance.h"

using namespace Caf::AmqpIntegration;

AmqpOutboundEndpointInstance::AmqpOutboundEndpointInstance() :
	_isInitialized(false),
	_isRunning(false),
	CAF_CM_INIT_LOG("AmqpOutboundEndpointInstance") {
}

AmqpOutboundEndpointInstance::~AmqpOutboundEndpointInstance() {
}

void AmqpOutboundEndpointInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	_id = configSection->findOptionalAttribute("id");
	if (!_id.length()) {
		_id = CStringUtils::createRandomUuid();
	}
	_configSection = configSection;
	_isInitialized = true;
}

std::string AmqpOutboundEndpointInstance::getId() const {
	return _id;
}

void AmqpOutboundEndpointInstance::setIntegrationAppContext(
			SmartPtrIIntegrationAppContext context) {
	_context = context;
}


void AmqpOutboundEndpointInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrIMessageChannel errorMessageChannel =
		channelResolver->resolveChannelName("errorChannel");

	std::string elementVal = _configSection->findOptionalAttribute("amqp-template");
	if (!elementVal.length()) {
		elementVal = "amqpTemplate";
		CAF_CM_LOG_DEBUG_VA0("Using default amqp-template reference value 'amqpTemplate'");
	}
	SmartPtrIIntegrationObject amqpTemplateObj = _context->getIntegrationObject(elementVal);
	SmartPtrAmqpTemplate amqpTemplate;
	amqpTemplate.QueryInterface(amqpTemplateObj, false);
	if (!amqpTemplate) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Bean '%s' is not of type AmqpTemplate",
				elementVal.c_str());
	}

	SmartPtrIAppConfig appConfig = getAppConfig();
	SmartPtrAmqpOutboundEndpoint outboundEndpoint;
	outboundEndpoint.CreateInstance();
	elementVal = _configSection->findOptionalAttribute("exchange-name");
	outboundEndpoint->setExchangeName(appConfig->resolveValue(elementVal));
	elementVal = _configSection->findOptionalAttribute("exchange-name-expression");
	outboundEndpoint->setExchangeNameExpression(elementVal);
	elementVal = _configSection->findOptionalAttribute("routing-key");
	outboundEndpoint->setRoutingKey(appConfig->resolveValue(elementVal));
	elementVal = _configSection->findOptionalAttribute("routing-key-expression");
	outboundEndpoint->setRoutingKeyExpression(elementVal);
	elementVal = _configSection->findOptionalAttribute("mapped-request-headers");
	outboundEndpoint->setMappedRequestHeadersExpression(appConfig->resolveValue(elementVal));
	outboundEndpoint->init(
			amqpTemplate,
			appConfig,
			appContext);

	elementVal = _configSection->findRequiredAttribute("channel");
	SmartPtrIMessageChannel inputChannel = channelResolver->resolveChannelName(elementVal);
	SmartPtrIIntegrationObject inputChannelObj;
	inputChannelObj.QueryInterface(inputChannel);


	_messagingTemplate.CreateInstance();
	_messagingTemplate->initialize(
			channelResolver,
			inputChannelObj,
			errorMessageChannel,
			SmartPtrIMessageChannel(),
			outboundEndpoint);
}

void AmqpOutboundEndpointInstance::start(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("start");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(!_isRunning);
	_isRunning = true;
	_messagingTemplate->start(0);
}

void AmqpOutboundEndpointInstance::stop(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("stop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(_isRunning);
	_isRunning = false;
	_messagingTemplate->stop(0);
}

bool AmqpOutboundEndpointInstance::isRunning() const {
	return _isRunning;
}
