/*
 *  Created on: Aug 15, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IVariant.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#include "CRouterInstance.h"

using namespace Caf;

CRouterInstance::CRouterInstance() :
	_isInitialized(false),
	_timeout(-1),
	_resolutionRequired(false),
	CAF_CM_INIT_LOG("CRouterInstance") {
}

CRouterInstance::~CRouterInstance() {
}

void CRouterInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");

	std::string val = configSection->findOptionalAttribute("timeout");
	if (val.length()) {
		_timeout = CStringConv::fromString<int32>(val);
	}

	_defaultOutputChannelId =
			configSection->findOptionalAttribute("default-output-channel");

	_expressionStr = configSection->findRequiredAttribute("expression");

	const std::string resolutionRequiredStr = configSection->findOptionalAttribute("resolution-required");
	_resolutionRequired =
		(resolutionRequiredStr.empty() || resolutionRequiredStr.compare("true") == 0) ? true : false;

	const IDocument::SmartPtrCChildCollection childCollection = configSection->getAllChildren();
	for(TConstIterator<IDocument::CChildCollection> childIter(*childCollection); childIter; childIter++) {
		const std::string sectionName = childIter->first;
		if (sectionName.compare("mapping") == 0) {
			const SmartPtrIDocument document = childIter->second;
			const std::string value = document->findRequiredAttribute("value");
			const std::string channel = document->findRequiredAttribute("channel");
			_valueToChannelMapping.insert(std::make_pair(value, channel));
		}
	}

	if (_valueToChannelMapping.empty()) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"No mapping sections found - %s",
				_id.c_str());
	}

	_isInitialized = true;
}

std::string CRouterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _id;
}

void CRouterInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	_channelResolver = channelResolver;

	SmartPtrIMessageChannel defaultOutputChannel;
	if (_defaultOutputChannelId.length()) {
		try {
			defaultOutputChannel =
					_channelResolver->resolveChannelName(_defaultOutputChannelId);
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_WARN_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
		if (defaultOutputChannel) {
			CAF_CM_LOG_INFO_VA2(
				"Successfully resolved default channel - id: %s, defaultOutputChannelId: %s",
				_id.c_str(),
				_defaultOutputChannelId.c_str());
		} else {
			CAF_CM_EXCEPTIONEX_VA2(
					NoSuchElementException,
					0,
					"Failed to resolve default channel - id: %s, defaultChannelId: %s",
					_id.c_str(),
					_defaultOutputChannelId.c_str());
		}
	}

	_expressionHandler.CreateInstance();
	_expressionHandler->init(
			getAppConfig(),
			appContext,
			_expressionStr);

	CAbstractMessageRouter::init(defaultOutputChannel, false, -1);
}

CRouterInstance::ChannelCollection CRouterInstance::getTargetChannels(
		const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("getTargetChannels");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	ChannelCollection messageChannels;

	std::string exprValueStr;
	try {
		SmartPtrIVariant exprValue = _expressionHandler->evaluate(message);
		exprValueStr = exprValue->toString();
		CAF_CM_VALIDATE_STRING(exprValueStr);
		CAF_CM_LOG_DEBUG_VA2(
				"router '%s' expression returned '%s'",
				_id.c_str(),
				exprValueStr.c_str());
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_ERROR_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	SmartPtrIMessageChannel outputChannel;
	std::string outputChannelId;
	if (exprValueStr.length()) {
		Cmapstrstr::const_iterator valueIter = _valueToChannelMapping.find(exprValueStr);
		if (valueIter != _valueToChannelMapping.end()) {
			outputChannelId = valueIter->second;
			try {
				outputChannel = _channelResolver->resolveChannelName(outputChannelId);
				CAF_CM_LOG_DEBUG_VA3(
					"Successfully resolved channel "
					"- id: %s, expression value: %s, outputChannel: %s",
					_id.c_str(),
					exprValueStr.c_str(),
					outputChannelId.c_str());
			}
			CAF_CM_CATCH_ALL;
			CAF_CM_LOG_WARN_CAFEXCEPTION;
			CAF_CM_CLEAREXCEPTION;
		} else {
			CAF_CM_LOG_WARN_VA2(
					"Expression value not found in mappings - "
					"id: '%s', value: '%s'",
					_id.c_str(),
					exprValueStr.c_str());
		}
	}

	if (outputChannel) {
		messageChannels.push_back(outputChannel);
	} else {
		if (_resolutionRequired) {
			CAF_CM_EXCEPTIONEX_VA3(
					NoSuchElementException,
					0,
					"Failed to resolve channel when resolution is required "
					"- id: %s, expression value: '%s', outputChannel: '%s'",
					_id.c_str(),
					exprValueStr.c_str(),
					outputChannelId.c_str());
		} else if (_defaultOutputChannelId.empty()) {
			CAF_CM_EXCEPTIONEX_VA2(
					NoSuchElementException,
					0,
					"Did not resolve output channel and default channel not provided "
					"- id: %s, expression value: %s",
					_id.c_str(),
					exprValueStr.c_str());
		}
	}
	return messageChannels;
}
