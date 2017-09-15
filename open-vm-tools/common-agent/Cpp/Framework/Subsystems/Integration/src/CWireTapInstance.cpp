/*
 *  Created on: Jan 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "CWireTapInstance.h"

CWireTapInstance::CWireTapInstance() :
	_order(0),
	_timeout(0),
	_isRunning(false),
	_pattern(NULL),
	_isInitialized(false),
	CAF_CM_INIT_LOG("CWireTapInstance") {
}

CWireTapInstance::~CWireTapInstance() {
	if (_pattern) {
		g_regex_unref(_pattern);
	}
}

void CWireTapInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_configSection = configSection;
		_id = _configSection->findRequiredAttribute("id");

		const std::string orderAttr = _configSection->findOptionalAttribute("order");
		if (!orderAttr.empty()) {
			_order = CStringConv::fromString<uint32>(orderAttr);
		}

		const std::string timeoutAttr = _configSection->findOptionalAttribute("timeout");
		if (!timeoutAttr.empty()) {
			_timeout = CStringConv::fromString<int32>(timeoutAttr);
		}

		const std::string pattern = _configSection->findRequiredAttribute("pattern");
		GError *error = NULL;
		_pattern = g_regex_new(
			pattern.c_str(),
			(GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_UNGREEDY | G_REGEX_RAW),
			(GRegexMatchFlags)(G_REGEX_MATCH_ANCHORED | G_REGEX_MATCH_NOTEMPTY),
			&error);
		if (error) {
			throw error;
		}

		_channelId = _configSection->findRequiredAttribute("channel");
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CWireTapInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	std::string rc;
	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;
	return rc;
}

void CWireTapInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(appContext);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);
		_channel = channelResolver->resolveChannelName(_channelId);
	}
	CAF_CM_EXIT;
}

void CWireTapInstance::start(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("start");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		_isRunning = true;
	}
	CAF_CM_EXIT;
}

void CWireTapInstance::stop(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("stop");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		_isRunning = false;
	}
	CAF_CM_EXIT;
}

bool CWireTapInstance::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");
	bool rc = false;
	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _isRunning;
	}
	CAF_CM_EXIT;
	return rc;
}

SmartPtrIIntMessage& CWireTapInstance::preSend(
		SmartPtrIIntMessage& message,
		SmartPtrIMessageChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("preSend");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_INTERFACE(message);
		CAF_CM_VALIDATE_INTERFACE(channel);

		// If the channel passed in is the
		// output channel for this wire-tap then
		// log a message and no-op this.
		if (_channel == channel) {
			CAF_CM_LOG_DEBUG_VA2(
					"WireTap (%s) will not intercept its own "
					"channel (%s),",
					_id.c_str(),
					_channelId.c_str());
		} else if (_isRunning) {
			if (_timeout) {
				_channel->send(message);
			} else {
				_channel->send(message, _timeout);
			}
		}
	}
	CAF_CM_EXIT;
	return message;
}

uint32 CWireTapInstance::getOrder() const {
	CAF_CM_FUNCNAME_VALIDATE("getOrder");
	uint32 rc;
	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _order;
	}
	CAF_CM_EXIT;
	return rc;
}

bool CWireTapInstance::isChannelIdMatched(const std::string& channelId) const {
	CAF_CM_FUNCNAME_VALIDATE("isChannelIdMatched");
	bool rc = false;
	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(channelId);
		rc = g_regex_match(
				_pattern,
				channelId.c_str(),
				(GRegexMatchFlags)0,
				NULL);
	}
	CAF_CM_EXIT;
	return rc;
}
