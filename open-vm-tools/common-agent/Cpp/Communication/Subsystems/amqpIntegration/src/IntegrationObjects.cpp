/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "Exception/CCafException.h"
#include "IntegrationObjects.h"
#include "RabbitTemplateInstance.h"
#include "RabbitAdminInstance.h"
#include "ExchangeInstance.h"
#include "QueueInstance.h"
#include "AmqpOutboundEndpointInstance.h"
#include "AmqpInboundChannelAdapterInstance.h"

using namespace Caf::AmqpIntegration;

IntegrationObjects::IntegrationObjects() :
	CAF_CM_INIT("IntegrationObjects") {
}

IntegrationObjects::~IntegrationObjects() {
}

void IntegrationObjects::initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
	CAF_CM_VALIDATE_STL_EMPTY(properties);
	_ctorArgs = ctorArgs;
	_properties = properties;
}

void IntegrationObjects::terminateBean() {
}

bool IntegrationObjects::isResponsible(
		const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");
	CAF_CM_VALIDATE_INTERFACE(configSection);

	const std::string name = configSection->getName();
	return (name.compare("rabbit-template") == 0)
			|| (name.compare("rabbit-admin") == 0)
			|| (name.compare("rabbit-direct-exchange") == 0)
			|| (name.compare("rabbit-topic-exchange") == 0)
			|| (name.compare("rabbit-headers-exchange") == 0)
			|| (name.compare("rabbit-fanout-exchange") == 0)
			|| (name.compare("rabbit-queue") == 0)
			|| (name.compare("rabbit-outbound-channel-adapter") == 0)
			|| (name.compare("rabbit-inbound-channel-adapter") == 0);
}

SmartPtrIIntegrationObject IntegrationObjects::createObject(
		const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME("createObject");
	CAF_CM_VALIDATE_INTERFACE(configSection);

	SmartPtrIIntegrationObject rc;
	const std::string name = configSection->getName();
	if (name.compare("rabbit-template") == 0) {
		SmartPtrRabbitTemplateInstance object;
		object.CreateInstance();
		rc = object;
	} else if (name.compare("rabbit-admin") == 0) {
		SmartPtrRabbitAdminInstance object;
		object.CreateInstance();
		rc = object;
	} else if ((name.compare("rabbit-direct-exchange") == 0)
			|| (name.compare("rabbit-topic-exchange") == 0)
			|| (name.compare("rabbit-headers-exchange") == 0)
			|| (name.compare("rabbit-fanout-exchange") == 0)) {
		SmartPtrExchangeInstance object;
		object.CreateInstance();
		rc = object;
	} else if (name.compare("rabbit-queue") == 0) {
		SmartPtrQueueInstance object;
		object.CreateInstance();
		rc = object;
	} else if (name.compare("rabbit-outbound-channel-adapter") == 0) {
		SmartPtrAmqpOutboundEndpointInstance object;
		object.CreateInstance();
		rc = object;
	} else if (name.compare("rabbit-inbound-channel-adapter") == 0) {
		SmartPtrAmqpInboundChannelAdapterInstance object;
		object.CreateInstance();
		rc = object;
	} else {
		CAF_CM_EXCEPTION_VA1(E_INVALIDARG, "Unknown name - %s", name.c_str());
	}

	rc->initialize(_ctorArgs, _properties, configSection);
	return rc;
}
