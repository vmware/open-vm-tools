/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CFileReadingMessageSource.h"
#include "Common/IAppContext.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/Core/CMessageHandler.h"
#include "Integration/Core/CSimpleAsyncTaskExecutor.h"
#include "Integration/Core/CSourcePollingChannelAdapter.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IMessageChannel.h"
#include "CFileInboundChannelAdapterInstance.h"

using namespace Caf;

CFileInboundChannelAdapterInstance::CFileInboundChannelAdapterInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CFileInboundChannelAdapterInstance") {
}

CFileInboundChannelAdapterInstance::~CFileInboundChannelAdapterInstance() {
}

void CFileInboundChannelAdapterInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

		_configSection = configSection;
		_id = _configSection->findRequiredAttribute("id");

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CFileInboundChannelAdapterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CFileInboundChannelAdapterInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(appContext);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);

		const std::string outputChannelStr =
			_configSection->findRequiredAttribute("channel");

		SmartPtrCFileReadingMessageSource fileReadingMessageSource;
		fileReadingMessageSource.CreateInstance();
		fileReadingMessageSource->initialize(_configSection);

		const SmartPtrIMessageChannel outputMessageChannel =
			channelResolver->resolveChannelName(outputChannelStr);
		const SmartPtrIMessageChannel errorMessageChannel =
			channelResolver->resolveChannelName("errorChannel");

		SmartPtrCMessageHandler messageHandler;
		messageHandler.CreateInstance();
		messageHandler->initialize(
			_id,
			outputMessageChannel,
			SmartPtrICafObject());

		SmartPtrCErrorHandler errorHandler;
		errorHandler.CreateInstance();
		errorHandler->initialize(channelResolver, errorMessageChannel);

		SmartPtrCSourcePollingChannelAdapter sourcePollingChannelAdapter;
		sourcePollingChannelAdapter.CreateInstance();
		sourcePollingChannelAdapter->initialize(
			messageHandler, fileReadingMessageSource, errorHandler);

		SmartPtrCSimpleAsyncTaskExecutor simpleAsyncTaskExecutor;
		simpleAsyncTaskExecutor.CreateInstance();
		simpleAsyncTaskExecutor->initialize(sourcePollingChannelAdapter, errorHandler);
		_taskExecutor = simpleAsyncTaskExecutor;
	}
	CAF_CM_EXIT;
}

void CFileInboundChannelAdapterInstance::start(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("start");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		CAF_CM_LOG_DEBUG_VA0("Starting the executor");
		_taskExecutor->execute(timeoutMs);
	}
	CAF_CM_EXIT;
}

void CFileInboundChannelAdapterInstance::stop(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("stop");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		CAF_CM_LOG_DEBUG_VA0("Stopping the executor");
		_taskExecutor->cancel(timeoutMs);
	}
	CAF_CM_EXIT;
}

bool CFileInboundChannelAdapterInstance::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");

	bool rc = false;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		rc = (_taskExecutor->getState() == ITaskExecutor::ETaskStateStarted);
	}
	CAF_CM_EXIT;

	return rc;
}

bool CFileInboundChannelAdapterInstance::isMessageProducer() const {
	CAF_CM_FUNCNAME_VALIDATE("isMessageProducer");

	bool rc = false;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		rc = true;
	}
	CAF_CM_EXIT;

	return rc;
}
