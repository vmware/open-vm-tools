/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IBean.h"
#include "ICafObject.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationComponent.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageChannel.h"
#include "Exception/CCafException.h"
#include "CServiceActivatorInstance.h"
#include "CObjectFactoryTables.h"

using namespace Caf;

CServiceActivatorInstance::CServiceActivatorInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CServiceActivatorInstance") {
}

CServiceActivatorInstance::~CServiceActivatorInstance() {
}

void CServiceActivatorInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

		_ctorArgs = ctorArgs;
		_properties = properties;
		_configSection = configSection;
		_id = _configSection->findRequiredAttribute("id");

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CServiceActivatorInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CServiceActivatorInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME("wire");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(appContext);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);

		MessageHandlerObjectCreatorMap::const_iterator objectCreatorEntry =
				CObjectFactoryTables::messageHandlerObjectCreatorMap.find(
						_configSection->getName());
		if (objectCreatorEntry ==
				CObjectFactoryTables::messageHandlerObjectCreatorMap.end()) {
			CAF_CM_EXCEPTIONEX_VA1(
					NoSuchElementException,
					0,
					"Configuration section '%s' is not handled by this object",
					_configSection->getName().c_str());
		}

		const SmartPtrIMessageChannel errorMessageChannel =
			channelResolver->resolveChannelName("errorChannel");

		std::string inputChannelStr;
		SmartPtrIMessageChannel outputMessageChannel;
		if (objectCreatorEntry->second.second) {
			inputChannelStr = _configSection->findRequiredAttribute("input-channel");

			const std::string outputChannelStr = _configSection->findRequiredAttribute("output-channel");
			outputMessageChannel = channelResolver->resolveChannelName(outputChannelStr);
		} else {
			inputChannelStr = _configSection->findOptionalAttribute("channel");
			if (inputChannelStr.empty()) {
				inputChannelStr = _configSection->findRequiredAttribute("input-channel");
			}
		}
		const SmartPtrIIntegrationObject inputIntegrationObject =
			channelResolver->resolveChannelNameToObject(inputChannelStr);

		SmartPtrICafObject messageHandlerObj;
		if (_configSection->getName().compare("service-activator") == 0) {
			const std::string refStr = _configSection->findRequiredAttribute("ref");
			CAF_CM_LOG_DEBUG_VA1("Creating the message processor - %s", refStr.c_str());
			const SmartPtrIBean bean = appContext->getBean(refStr);
			messageHandlerObj.QueryInterface(bean, false);
			CAF_CM_VALIDATE_INTERFACE(messageHandlerObj);

			SmartPtrIIntegrationComponentInstance integrationComponentInstance;
			integrationComponentInstance.QueryInterface(bean, false);
			if (!integrationComponentInstance.IsNull()) {
				// Bean is also an integration component instance...wire
				integrationComponentInstance->wire(appContext, channelResolver);
			}
		} else {
			SmartPtrIIntegrationObject integrationObject;
			if (objectCreatorEntry->second.first) {
				integrationObject = (objectCreatorEntry->second.first)();
				integrationObject->initialize(_ctorArgs, _properties, _configSection);
			} else {
				const std::string& beanId = _configSection->findRequiredAttribute("ref");
				const SmartPtrIBean bean = appContext->getBean(beanId);
				SmartPtrIIntegrationComponent integrationComponent;
				integrationComponent.QueryInterface(bean, false);
				if (!integrationComponent) {
					CAF_CM_EXCEPTIONEX_VA1(
							InvalidArgumentException,
							0,
							"Bean is not an integration component - %s",
							beanId.c_str());
				}
				integrationObject = integrationComponent->createObject(_configSection);
			}
			SmartPtrIIntegrationComponentInstance integrationComponentInstance;
			integrationComponentInstance.QueryInterface(integrationObject, false);
			CAF_CM_VALIDATE_INTERFACE(integrationComponentInstance);
			integrationComponentInstance->wire(appContext, channelResolver);
			messageHandlerObj.QueryInterface(integrationObject, false);
			CAF_CM_VALIDATE_INTERFACE(messageHandlerObj);
		}

		_messagingTemplate.CreateInstance();
		_messagingTemplate->initialize(
				channelResolver,
				inputIntegrationObject,
				errorMessageChannel,
				outputMessageChannel,
				messageHandlerObj);
	}
	CAF_CM_EXIT;
}

void CServiceActivatorInstance::start(
	const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("start");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		CAF_CM_LOG_DEBUG_VA0("Starting");
		_messagingTemplate->start(timeoutMs);
	}
	CAF_CM_EXIT;
}

void CServiceActivatorInstance::stop(
	const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("stop");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		CAF_CM_LOG_DEBUG_VA0("Stopping");
		_messagingTemplate->stop(timeoutMs);
	}
	CAF_CM_EXIT;
}

bool CServiceActivatorInstance::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");

	bool rc = false;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		rc = _messagingTemplate->isRunning();
	}
	CAF_CM_EXIT;

	return rc;
}
