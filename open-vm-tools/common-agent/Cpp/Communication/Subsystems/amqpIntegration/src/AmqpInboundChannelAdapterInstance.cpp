/*
 *  Created on: Aug 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IBean.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/Core/CMessageHandler.h"
#include "Integration/Core/CSimpleAsyncTaskExecutor.h"
#include "Integration/Core/CSourcePollingChannelAdapter.h"
#include "Integration/Dependencies/CPollerMetadata.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationAppContext.h"
#include "Integration/IIntegrationObject.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpCore/AmqpMessageListenerSource.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpCore/DefaultAmqpHeaderMapper.h"
#include "amqpCore/Queue.h"
#include "Common/CCafRegex.h"
#include "Exception/CCafException.h"
#include "AmqpInboundChannelAdapterInstance.h"


using namespace Caf::AmqpIntegration;

AmqpInboundChannelAdapterInstance::AmqpInboundChannelAdapterInstance() :
	_isInitialized(false),
	_isRunning(false),
	_ackModeProp(ACKNOWLEDGEMODE_AUTO),
	_connectionFactoryProp("connectionFactory"),
	_autoStartupProp(true),
	_phaseProp(G_MAXINT32),
	_prefetchCountProp(1),
	_receiveTimeoutProp(1000),
	_recoveryIntervalProp(5000),
	_txSizeProp(1),
	CAF_CM_INIT_LOG("AmqpInboundChannelAdapterInstance") {
}

AmqpInboundChannelAdapterInstance::~AmqpInboundChannelAdapterInstance() {
}

void AmqpInboundChannelAdapterInstance::initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	std::string prop = configSection->findOptionalAttribute("id");
	if (prop.length()) {
		_idProp = prop;
	} else {
		_idProp = "AmqpInboundChannelAdapter-";
		_idProp += CStringUtils::createRandomUuid();
	}
	_channelProp = configSection->findRequiredAttribute("channel");
	_queueProp = configSection->findRequiredAttribute("queue-name");
	prop = configSection->findOptionalAttribute("acknowledge-mode");
	if (prop.length()) {
		if (prop == "NONE") {
			_ackModeProp = ACKNOWLEDGEMODE_NONE;
		} else if (prop == "AUTO") {
			_ackModeProp = ACKNOWLEDGEMODE_AUTO;
		} else if (prop == "MANUAL") {
			CAF_CM_EXCEPTIONEX_VA0(
					InvalidArgumentException,
					0,
					"acknowledge-mode MANUAL is not supported");
			//_ackModeProp = ACKNOWLEDGEMODE_MANUAL;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					InvalidArgumentException,
					0,
					"invalid acknowledge-mode '%s'",
					prop.c_str());
		}
	}
	prop = configSection->findOptionalAttribute("connection-factory");
	if (prop.length()) {
		_connectionFactoryProp = prop;
	}
	_errorChannelProp = configSection->findRequiredAttribute("error-channel");
	_mappedRequestHeadersProp = configSection->findOptionalAttribute("mapped-request-headers");
	prop = configSection->findOptionalAttribute("auto-startup");
	if (prop.length()) {
		if (prop == "true") {
			_autoStartupProp = true;
		} else if (prop == "false") {
			_autoStartupProp = false;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					InvalidArgumentException,
					0,
					"invalid auto-startup '%s'",
					prop.c_str());
		}
	}
	prop = configSection->findOptionalAttribute("phase");
	if (prop.length()) {
		_phaseProp = CStringConv::fromString<int32>(prop);
	}
	prop = configSection->findOptionalAttribute("prefetch-count");
	if (prop.length()) {
		_prefetchCountProp = CStringConv::fromString<uint32>(prop);
	}
	prop = configSection->findOptionalAttribute("receive-timeout");
	if (prop.length()) {
		_receiveTimeoutProp = CStringConv::fromString<uint32>(prop);
	}
	prop = configSection->findOptionalAttribute("recovery-interval");
	if (prop.length()) {
		_recoveryIntervalProp = CStringConv::fromString<uint32>(prop);
	}
	prop = configSection->findOptionalAttribute("tx-size");
	if (prop.length()) {
		_txSizeProp = CStringConv::fromString<uint32>(prop);
	}

	_isInitialized = true;
}

std::string AmqpInboundChannelAdapterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _idProp;
}

void AmqpInboundChannelAdapterInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	CCafRegex qRegEx;
	qRegEx.initialize("^\\#\\{(?P<name>.+)\\}$");
	Cdeqstr matchName = qRegEx.matchName(_queueProp, "name");
	if (matchName.size()) {
		std::string queueRef = matchName.front();
		CAF_CM_LOG_DEBUG_VA1("Resolving queue object reference '%s'", queueRef.c_str());
		SmartPtrIIntegrationObject obj = _intAppContext->getIntegrationObject(queueRef);
		SmartPtrQueue queueObj;
		queueObj.QueryInterface(obj, false);
		if (queueObj) {
			_queueProp = queueObj->getName();
			CAF_CM_LOG_DEBUG_VA2(
					"Queue object reference '%s' resolved to AMQP queue name '%s'",
					queueRef.c_str(),
					_queueProp.c_str());
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					NoSuchInterfaceException,
					0,
					"Integration object '%s' is not of type Queue.",
					queueRef.c_str());
		}
	}

	SmartPtrAmqpHeaderMapper headerMapper;
	if (_mappedRequestHeadersProp.length()) {
		SmartPtrDefaultAmqpHeaderMapper defaultHeaderMapper;
		defaultHeaderMapper.CreateInstance();
		defaultHeaderMapper->init(_mappedRequestHeadersProp);
		headerMapper = defaultHeaderMapper;
	}

	SmartPtrCPollerMetadata pollerMetadata;
	pollerMetadata.CreateInstance();
	pollerMetadata->putMaxMessagesPerPoll(_txSizeProp);
	pollerMetadata->putFixedRate(50);

	SmartPtrAmqpMessageListenerSource listenerSource;
	listenerSource.CreateInstance();
	listenerSource->init(headerMapper, pollerMetadata);

	_listenerContainer.CreateInstance();
	_listenerContainer->setAcknowledgeMode(_ackModeProp);
	SmartPtrIBean bean = appContext->getBean(_connectionFactoryProp);
	SmartPtrConnectionFactory connectionFactory;
	connectionFactory.QueryInterface(bean, false);
	if (!connectionFactory) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"Bean '%s' is not a ConnectionFactory",
				_connectionFactoryProp.c_str());
	}
	_listenerContainer->setConnectionFactory(connectionFactory);
	_listenerContainer->setPrefetchCount(_prefetchCountProp);
	_listenerContainer->setQueue(_queueProp);
	_listenerContainer->setReceiveTimeout(_receiveTimeoutProp);
	_listenerContainer->setRecoveryInterval(_recoveryIntervalProp);
	_listenerContainer->setTxSize(_txSizeProp);
	_listenerContainer->setMessagerListener(listenerSource);
	_listenerContainer->init();

	SmartPtrCErrorHandler errorHandler;
	errorHandler.CreateInstance();
	errorHandler->initialize(
			channelResolver,
			channelResolver->resolveChannelName(_errorChannelProp));

	SmartPtrCMessageHandler messageHandler;
	messageHandler.CreateInstance();
	messageHandler->initialize(
		_idProp,
		channelResolver->resolveChannelName(_channelProp),
		SmartPtrICafObject());

	SmartPtrCSourcePollingChannelAdapter sourcePollingChannelAdapter;
	sourcePollingChannelAdapter.CreateInstance();
	sourcePollingChannelAdapter->initialize(
		messageHandler,
		listenerSource,
		errorHandler);

	SmartPtrCSimpleAsyncTaskExecutor simpleAsyncTaskExecutor;
	simpleAsyncTaskExecutor.CreateInstance();
	simpleAsyncTaskExecutor->initialize(
			sourcePollingChannelAdapter,
			errorHandler);
	_taskExecutor = simpleAsyncTaskExecutor;

	_intAppContext = NULL;
}

void AmqpInboundChannelAdapterInstance::setIntegrationAppContext(
				SmartPtrIIntegrationAppContext context) {
	CAF_CM_FUNCNAME_VALIDATE("setIntegrationAppContext");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(context);
	_intAppContext = context;
}

bool AmqpInboundChannelAdapterInstance::isAutoStartup() const {
	CAF_CM_FUNCNAME_VALIDATE("isAutoStartup");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _autoStartupProp;
}

void AmqpInboundChannelAdapterInstance::start(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("start");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_listenerContainer->start(timeoutMs);
	_taskExecutor->execute(timeoutMs);
	_isRunning = true;
}

void AmqpInboundChannelAdapterInstance::stop(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("stop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_listenerContainer->stop(timeoutMs);
	_taskExecutor->cancel(timeoutMs);
}

bool AmqpInboundChannelAdapterInstance::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _isRunning;
}

int32 AmqpInboundChannelAdapterInstance::getPhase() const {
	CAF_CM_FUNCNAME_VALIDATE("getPhase");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _phaseProp;
}
