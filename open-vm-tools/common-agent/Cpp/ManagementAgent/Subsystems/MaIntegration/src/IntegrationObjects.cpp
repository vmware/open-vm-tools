/*
 *  Created on: Nov 13, 2015
 *      Author: bwilliams
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CConfigEnvInboundChannelAdapterInstance.h"
#include "CConfigEnvOutboundChannelAdapterInstance.h"
#include "CMonitorInboundChannelAdapterInstance.h"
#include "CPersistenceOutboundChannelAdapterInstance.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "Exception/CCafException.h"
#include "IntegrationObjects.h"
#include "CPersistenceInboundChannelAdapterInstance.h"

using namespace Caf::MaIntegration;

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
	return (name.compare("persistence-inbound-channel-adapter") == 0)
			|| (name.compare("persistence-outbound-channel-adapter") == 0)
			|| (name.compare("configenv-inbound-channel-adapter") == 0)
			|| (name.compare("configenv-outbound-channel-adapter") == 0)
			|| (name.compare("monitor-inbound-channel-adapter") == 0);
}

SmartPtrIIntegrationObject IntegrationObjects::createObject(
		const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME("createObject");
	CAF_CM_VALIDATE_INTERFACE(configSection);

	SmartPtrIIntegrationObject rc;
	const std::string name = configSection->getName();
	if (name.compare("persistence-inbound-channel-adapter") == 0) {
		SmartPtrCPersistenceInboundChannelAdapterInstance object;
		object.CreateInstance();
		rc = object;
	} else if (name.compare("persistence-outbound-channel-adapter") == 0) {
		SmartPtrCPersistenceOutboundChannelAdapterInstance object;
		object.CreateInstance();
		rc = object;
	} else if (name.compare("configenv-inbound-channel-adapter") == 0) {
		SmartPtrCConfigEnvInboundChannelAdapterInstance object;
		object.CreateInstance();
		rc = object;
	} else if (name.compare("configenv-outbound-channel-adapter") == 0) {
		SmartPtrCConfigEnvOutboundChannelAdapterInstance object;
		object.CreateInstance();
		rc = object;
	} else if (name.compare("monitor-inbound-channel-adapter") == 0) {
		SmartPtrCMonitorInboundChannelAdapterInstance object;
		object.CreateInstance();
		rc = object;
	} else {
		CAF_CM_EXCEPTION_VA1(E_INVALIDARG, "Unknown name - %s", name.c_str());
	}

	rc->initialize(_ctorArgs, _properties, configSection);
	return rc;
}
