/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Exception/CCafException.h"
#include "CHeaderValueRouterInstance.h"

using namespace Caf;

CHeaderValueRouterInstance::CHeaderValueRouterInstance() :
	_isInitialized(false),
	_resolutionRequired(false),
	CAF_CM_INIT_LOG("CHeaderValueRouterInstance") {
}

CHeaderValueRouterInstance::~CHeaderValueRouterInstance() {
}

void CHeaderValueRouterInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_id = configSection->findRequiredAttribute("id");
		_headerName = configSection->findRequiredAttribute("header-name");
		_defaultOutputChannelId =
				configSection->findOptionalAttribute("default-output-channel");

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
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"No mapping sections found - %s", _id.c_str());
		}

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CHeaderValueRouterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CHeaderValueRouterInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME("wire");

	CAF_CM_ENTER {
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

		CAbstractMessageRouter::init(defaultOutputChannel, false, -1);
	}
	CAF_CM_EXIT;
}

CHeaderValueRouterInstance::ChannelCollection CHeaderValueRouterInstance::getTargetChannels(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("getTargetChannels");

	ChannelCollection messageChannels;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		const std::string outputChannel = calcOutputChannel(message);
		if (outputChannel.empty() && _defaultOutputChannelId.empty()) {
			CAF_CM_EXCEPTIONEX_VA2(NoSuchElementException, ERROR_NOT_FOUND,
				"Did not find output channel and default channel not "
				"provided - id: %s, headerName: %s",
				_id.c_str(), _headerName.c_str());
		}

		SmartPtrIMessageChannel messageChannel;
		if (! outputChannel.empty()) {
			try {
				messageChannel = _channelResolver->resolveChannelName(outputChannel);
			}
			CAF_CM_CATCH_ALL;
			CAF_CM_LOG_WARN_CAFEXCEPTION;
			CAF_CM_CLEAREXCEPTION;

			if (messageChannel.IsNull() && _resolutionRequired) {
				CAF_CM_EXCEPTIONEX_VA3(NoSuchElementException, ERROR_NOT_FOUND,
					"Failed to resolve channel when resolution is required - id: %s, "
					"headerName: %s, outputChannel: %s",
					_id.c_str(), _headerName.c_str(), outputChannel.c_str());
			}

			if (! messageChannel.IsNull()) {
				CAF_CM_LOG_INFO_VA3(
					"Successfully resolved channel - id: %s, headerName: %s, outputChannel: %s",
					_id.c_str(), _headerName.c_str(), outputChannel.c_str());
			}
		}

		if (messageChannel.IsNull() &&  _defaultOutputChannelId.empty()) {
			CAF_CM_EXCEPTIONEX_VA3(NoSuchElementException, ERROR_NOT_FOUND,
				"Failed to resolve channel when resolution is not required and default "
				"channel is not available - id: %s, headerName: %s, outputChannel: %s",
				_id.c_str(), _headerName.c_str(), outputChannel.c_str());
		}

		if (!messageChannel.IsNull()) {
			messageChannels.push_back(messageChannel);
		}
	}
	CAF_CM_EXIT;

	return messageChannels;
}

std::string CHeaderValueRouterInstance::calcOutputChannel(
	const SmartPtrIIntMessage& message) const {
	CAF_CM_FUNCNAME_VALIDATE("calcOutputChannel");

	std::string outputChannel;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(message);

		const std::string headerValue = message->findOptionalHeaderAsString(_headerName);
		if (headerValue.empty()) {
			CAF_CM_LOG_WARN_VA2("Header name not found in headers - id: %s, headerName: %s",
				_id.c_str(), _headerName.c_str());
		} else {
			const Cmapstrstr::const_iterator valueToChannelIter =
				_valueToChannelMapping.find(headerValue);
			if (valueToChannelIter == _valueToChannelMapping.end()) {
				CAF_CM_LOG_WARN_VA3("Header name found in headers, but not config - id: %s, name: %s, val: %s",
					_id.c_str(), _headerName.c_str(), headerValue.c_str());
			} else {
				outputChannel = valueToChannelIter->second;
			}
		}
	}
	CAF_CM_EXIT;

	return outputChannel;
}
