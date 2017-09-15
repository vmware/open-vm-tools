/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CPersistenceReadingMessageSource.h"
#include "Common/IAppContext.h"
#include "IBean.h"
#include "IPersistence.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/Core/CMessageHandler.h"
#include "Integration/Core/CSimpleAsyncTaskExecutor.h"
#include "Integration/Core/CSourcePollingChannelAdapter.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IMessageChannel.h"
#include "CPersistenceInboundChannelAdapterInstance.h"

using namespace Caf;

CPersistenceInboundChannelAdapterInstance::CPersistenceInboundChannelAdapterInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CPersistenceInboundChannelAdapterInstance") {
}

CPersistenceInboundChannelAdapterInstance::~CPersistenceInboundChannelAdapterInstance() {
}

void CPersistenceInboundChannelAdapterInstance::initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_configSection = configSection;
	_id = _configSection->findRequiredAttribute("id");

	_isInitialized = true;
}

std::string CPersistenceInboundChannelAdapterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CPersistenceInboundChannelAdapterInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	const SmartPtrIPersistence persistence = createPersistence(appContext);
	if (! persistence.IsNull()) {
		const std::string outputChannelStr =
			_configSection->findRequiredAttribute("channel");

		SmartPtrCPersistenceReadingMessageSource persistenceReadingMessageSource;
		persistenceReadingMessageSource.CreateInstance();
		persistenceReadingMessageSource->initialize(_configSection, persistence);

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
			messageHandler, persistenceReadingMessageSource, errorHandler);

		SmartPtrCSimpleAsyncTaskExecutor simpleAsyncTaskExecutor;
		simpleAsyncTaskExecutor.CreateInstance();
		simpleAsyncTaskExecutor->initialize(sourcePollingChannelAdapter, errorHandler);
		_taskExecutor = simpleAsyncTaskExecutor;
	}
}

void CPersistenceInboundChannelAdapterInstance::start(
		const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("start");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (! _taskExecutor.IsNull()) {
		CAF_CM_LOG_DEBUG_VA0("Starting the executor");
		_taskExecutor->execute(timeoutMs);
	}
}

void CPersistenceInboundChannelAdapterInstance::stop(
		const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("stop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (! _taskExecutor.IsNull()) {
		CAF_CM_LOG_DEBUG_VA0("Stopping the executor");
		_taskExecutor->cancel(timeoutMs);
	}
}

bool CPersistenceInboundChannelAdapterInstance::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const bool rc = ! _taskExecutor.IsNull()
			&& (_taskExecutor->getState() == ITaskExecutor::ETaskStateStarted);
	return rc;
}

bool CPersistenceInboundChannelAdapterInstance::isMessageProducer() const {
	CAF_CM_FUNCNAME_VALIDATE("isMessageProducer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return true;
}

SmartPtrIPersistence CPersistenceInboundChannelAdapterInstance::createPersistence(
	const SmartPtrIAppContext& appContext) const {
	CAF_CM_FUNCNAME("createPersistence");
	CAF_CM_VALIDATE_INTERFACE(appContext);

	SmartPtrIPersistence rc;
	const std::string removeRefStr = _configSection->findRequiredAttribute("ref");
	CAF_CM_LOG_DEBUG_VA1("Creating the persistence impl - %s", removeRefStr.c_str());
	const SmartPtrIBean bean = appContext->getBean(removeRefStr);
	rc.QueryInterface(bean, false);
	CAF_CM_VALIDATE_INTERFACE(rc);

	try {
		rc->initialize();
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT;

	if (CAF_CM_ISEXCEPTION) {
		CAF_CM_LOG_WARN_VA2("initialize failed - ref: %s, msg: %s",
				removeRefStr.c_str(), (CAF_CM_EXCEPTION_GET_FULLMSG).c_str());
		rc = SmartPtrIPersistence();
		CAF_CM_CLEAREXCEPTION;
	}

	return rc;
}
