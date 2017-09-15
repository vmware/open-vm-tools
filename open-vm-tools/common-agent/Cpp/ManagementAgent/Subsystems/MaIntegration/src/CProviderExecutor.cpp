/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CProviderExecutorRequest.h"
#include "Common/IAppContext.h"
#include "IBean.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationComponent.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"
#include "Exception/CCafException.h"
#include "CProviderExecutor.h"

using namespace Caf;

CProviderExecutor::CProviderExecutor() :
		_isInitialized(false),
		CAF_CM_INIT_LOG("CProviderExecutor") {
}

CProviderExecutor::~CProviderExecutor() {
}

void CProviderExecutor::initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties) {

	CAF_CM_FUNCNAME_VALIDATE("initializeBean");

	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);

	IBean::Cprops::const_iterator itr = properties.find("beginImpersonationBeanRef");
	if (itr != properties.end()) {
		_beginImpersonationBeanId = itr->second;
	}

	itr = properties.find("endImpersonationBeanRef");
	if (itr != properties.end()) {
		_endImpersonationBeanId = itr->second;
	}

	_isInitialized = true;
}

void CProviderExecutor::terminateBean() {
}

void CProviderExecutor::wire(const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {

	CAF_CM_FUNCNAME_VALIDATE("wire");

	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	if (AppConfigUtils::getOptionalBoolean(_sManagementAgentArea, "use_impersonation")) {
		_beginImpersonationTransformer = loadTransformer(_beginImpersonationBeanId, appContext, channelResolver);
		_endImpersonationTransformer = loadTransformer(_endImpersonationBeanId, appContext, channelResolver);
	}

	SmartPtrCErrorHandler errorHandler;
	errorHandler.CreateInstance();
	errorHandler->initialize(channelResolver, channelResolver->resolveChannelName("errorChannel"));
	_errorHandler = errorHandler;
}

SmartPtrITransformer CProviderExecutor::loadTransformer(
		const std::string& id,
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {

	CAF_CM_FUNCNAME("loadTransformer");

	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_STRING(id);

	SmartPtrITransformer transformer;
	if (!id.empty()) {
		const SmartPtrIBean bean = appContext->getBean(id);
		SmartPtrIIntegrationComponent integrationComponent;
		integrationComponent.QueryInterface(bean, false);
		if (!integrationComponent) {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, 0,
					"Bean is not an integration component - %s", id.c_str());
		}

		SmartPtrIDocument configSection;
		SmartPtrIIntegrationObject integrationObject;
		integrationObject = integrationComponent->createObject(configSection);

		SmartPtrIIntegrationComponentInstance integrationComponentInstance;
		integrationComponentInstance.QueryInterface(integrationObject, false);
		if (!integrationComponentInstance.IsNull()) {
			integrationComponentInstance->wire(appContext, channelResolver);
		}

		transformer.QueryInterface(integrationObject, false);
		CAF_CM_VALIDATE_INTERFACE(transformer);
	}
	return transformer;
}

void CProviderExecutor::handleMessage(const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("handleMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(message);

	CAF_CM_LOG_DEBUG_VA0("Called");

	SmartPtrCProviderExecutorRequest executorRequest;
	executorRequest.CreateInstance();
	executorRequest->initialize(message);

	const std::string& providerUri = executorRequest->getProviderUri();
	SmartPtrCProviderExecutorRequestHandler handler = _handlers[providerUri];
	if (handler == NULL) {
		SmartPtrCProviderExecutorRequestHandler requestHandler;
		requestHandler.CreateInstance();
		requestHandler->initialize(providerUri, _beginImpersonationTransformer,
				_endImpersonationTransformer, _errorHandler);
		_handlers[providerUri] = requestHandler;
		handler = requestHandler;
	}
	handler->handleRequest(executorRequest);
}

SmartPtrIIntMessage CProviderExecutor::getSavedMessage() const {
	return NULL;
}

void CProviderExecutor::clearSavedMessage() {
}
