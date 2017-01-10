/*
 *  Created on: Jan 31, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Exception/CCafException.h"
#include "CLoggingChannelAdapterInstance.h"

using namespace Caf;

CLoggingChannelAdapterInstance::CLoggingChannelAdapterInstance() :
	_isInitialized(false),
	_level(log4cpp::Priority::INFO),
	_logFullMessage(false),
	_category(NULL),
	CAF_CM_INIT("CLoggingChannelAdapterInstance") {
}

CLoggingChannelAdapterInstance::~CLoggingChannelAdapterInstance() {
}

void CLoggingChannelAdapterInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");
	_category = &log4cpp::Category::getInstance(_id.c_str());
	std::string arg = configSection->findOptionalAttribute("level");
	if (!arg.empty()) {
		if (g_ascii_strncasecmp(arg.c_str(), "crit", arg.length()) == 0) {
			_level = log4cpp::Priority::CRIT;
		} else if (g_ascii_strncasecmp(arg.c_str(), "error", arg.length()) == 0) {
			_level = log4cpp::Priority::ERROR;
		} else if (g_ascii_strncasecmp(arg.c_str(), "warn", arg.length()) == 0) {
			_level = log4cpp::Priority::WARN;
		} else if (g_ascii_strncasecmp(arg.c_str(), "info", arg.length()) == 0) {
			_level = log4cpp::Priority::INFO;
		} else if (g_ascii_strncasecmp(arg.c_str(), "debug", arg.length()) == 0) {
			_level = log4cpp::Priority::DEBUG;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					InvalidArgumentException,
					0,
					"'%s' is not a valid logging level. "
					"Choices are 'debug', 'info', 'warn', 'error' and 'crit'",
					arg.c_str());
		}
	}
	arg = configSection->findOptionalAttribute("log-full-message");
	if (!arg.empty()) {
		if (g_ascii_strncasecmp(arg.c_str(), "true", arg.length()) == 0) {
			_logFullMessage = true;
		} else if (g_ascii_strncasecmp(arg.c_str(), "false", arg.length()) == 0) {
			_logFullMessage = false;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					InvalidArgumentException,
					0,
					"'%s' is not a valid log-full-message value. "
					"Choices are 'true' and 'false'",
					arg.c_str());
		}
	}

	_isInitialized = true;
}

std::string CLoggingChannelAdapterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CLoggingChannelAdapterInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
}

void CLoggingChannelAdapterInstance::handleMessage(const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("handleMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	_savedMessage = message;
	_category->log(_level, message->getPayloadStr().c_str());
	if (_logFullMessage) {
		IIntMessage::SmartPtrCHeaders headers = message->getHeaders();
		for (IIntMessage::CHeaders::const_iterator headerIter = headers->begin();
			headerIter != headers->end();
			headerIter++) {
			std::stringstream logMessage;
			logMessage << '['
				<< headerIter->first
				<< '='
				<< headerIter->second.first->toString()
				<< ']';
			_category->log(_level, logMessage.str());
		}
	}
}

SmartPtrIIntMessage CLoggingChannelAdapterInstance::getSavedMessage() const {
	CAF_CM_FUNCNAME_VALIDATE("getSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _savedMessage;
}

void CLoggingChannelAdapterInstance::clearSavedMessage() {
	CAF_CM_FUNCNAME_VALIDATE("clearSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_savedMessage = NULL;
}
