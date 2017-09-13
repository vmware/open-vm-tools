/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CConfigEnvInboundChannelAdapterInstance.h"
#include "CConfigEnvReadingMessageSource.h"

using namespace Caf;

CConfigEnvInboundChannelAdapterInstance::CConfigEnvInboundChannelAdapterInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CConfigEnvInboundChannelAdapterInstance") {
}

CConfigEnvInboundChannelAdapterInstance::~CConfigEnvInboundChannelAdapterInstance() {
}

void CConfigEnvInboundChannelAdapterInstance::initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_configSection = configSection;
	_id = _configSection->findRequiredAttribute("id");

	_isInitialized = true;
}

std::string CConfigEnvInboundChannelAdapterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CConfigEnvInboundChannelAdapterInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	const std::string outputChannelStr =
		_configSection->findRequiredAttribute("channel");

	SmartPtrCConfigEnvReadingMessageSource configEnvReadingMessageSource;
	configEnvReadingMessageSource.CreateInstance();
	configEnvReadingMessageSource->initialize(_configSection);

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
		messageHandler, configEnvReadingMessageSource, errorHandler);

	SmartPtrCSimpleAsyncTaskExecutor simpleAsyncTaskExecutor;
	simpleAsyncTaskExecutor.CreateInstance();
	simpleAsyncTaskExecutor->initialize(sourcePollingChannelAdapter, errorHandler);
	_taskExecutor = simpleAsyncTaskExecutor;
}

void CConfigEnvInboundChannelAdapterInstance::start(
		const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("start");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("Starting the executor");
	_taskExecutor->execute(timeoutMs);
}

void CConfigEnvInboundChannelAdapterInstance::stop(
		const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("stop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("Stopping the executor");
	_taskExecutor->cancel(timeoutMs);
}

bool CConfigEnvInboundChannelAdapterInstance::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const bool rc = (_taskExecutor->getState() == ITaskExecutor::ETaskStateStarted);
	return rc;
}

bool CConfigEnvInboundChannelAdapterInstance::isMessageProducer() const {
	CAF_CM_FUNCNAME_VALIDATE("isMessageProducer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return true;
}
