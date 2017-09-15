/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CMonitorReadingMessageSource.h"
#include "Common/IAppContext.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/Core/CMessageHandler.h"
#include "Integration/Core/CSimpleAsyncTaskExecutor.h"
#include "Integration/Core/CSourcePollingChannelAdapter.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IMessageChannel.h"
#include "CMonitorInboundChannelAdapterInstance.h"

using namespace Caf;

CMonitorInboundChannelAdapterInstance::CMonitorInboundChannelAdapterInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CMonitorInboundChannelAdapterInstance") {
}

CMonitorInboundChannelAdapterInstance::~CMonitorInboundChannelAdapterInstance() {
}

void CMonitorInboundChannelAdapterInstance::initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_configSection = configSection;
	_id = _configSection->findRequiredAttribute("id");

	_isInitialized = true;
}

std::string CMonitorInboundChannelAdapterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CMonitorInboundChannelAdapterInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	const std::string outputChannelStr =
		_configSection->findRequiredAttribute("channel");

	SmartPtrCMonitorReadingMessageSource monitorReadingMessageSource;
	monitorReadingMessageSource.CreateInstance();
	monitorReadingMessageSource->initialize(_configSection);

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
		messageHandler, monitorReadingMessageSource, errorHandler);

	SmartPtrCSimpleAsyncTaskExecutor simpleAsyncTaskExecutor;
	simpleAsyncTaskExecutor.CreateInstance();
	simpleAsyncTaskExecutor->initialize(sourcePollingChannelAdapter, errorHandler);
	_taskExecutor = simpleAsyncTaskExecutor;
}

void CMonitorInboundChannelAdapterInstance::start(
		const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("start");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("Starting the executor");
	_taskExecutor->execute(timeoutMs);
}

void CMonitorInboundChannelAdapterInstance::stop(
		const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("stop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("Stopping the executor");
	_taskExecutor->cancel(timeoutMs);
}

bool CMonitorInboundChannelAdapterInstance::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const bool rc = (_taskExecutor->getState() == ITaskExecutor::ETaskStateStarted);
	return rc;
}

bool CMonitorInboundChannelAdapterInstance::isMessageProducer() const {
	CAF_CM_FUNCNAME_VALIDATE("isMessageProducer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return true;
}
