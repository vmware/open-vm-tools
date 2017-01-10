/*
 *  Created on: Aug 8, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "Exception/CCafException.h"
#include "CIntegrationObjectFactory.h"
#include "CObjectFactoryTables.h"
#include "CDirectChannelInstance.h"
#include "CQueueChannelInstance.h"

CIntegrationObjectFactory::CIntegrationObjectFactory() :
	CAF_CM_INIT("CIntegrationObjectFactory") {
}

CIntegrationObjectFactory::~CIntegrationObjectFactory() {
}

void CIntegrationObjectFactory::initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
	CAF_CM_VALIDATE_STL_EMPTY(properties);
	_ctorArgs = ctorArgs;
	_properties = properties;
}

void CIntegrationObjectFactory::terminateBean() {
}

bool CIntegrationObjectFactory::isResponsible(
		const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");
	CAF_CM_VALIDATE_INTERFACE(configSection);
	return CObjectFactoryTables::objectCreatorMap.find(
			configSection->getName()) != CObjectFactoryTables::objectCreatorMap.end();
}

SmartPtrIIntegrationObject CIntegrationObjectFactory::createObject(
		const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME("createObject");
	ObjectCreatorMap::const_iterator entry =
			CObjectFactoryTables::objectCreatorMap.find(configSection->getName());
	CAF_CM_ASSERT(entry != CObjectFactoryTables::objectCreatorMap.end());

	SmartPtrIIntegrationObject object;
	if (entry->second) {
		object = (entry->second)();
	} else if (configSection->getName() == "channel") {
		if (configSection->findOptionalChild("queue")) {
			SmartPtrCQueueChannelInstance channel;
			channel.CreateInstance();
			object = channel;
		} else {
			SmartPtrCDirectChannelInstance channel;
			channel.CreateInstance();
			object = channel;
		}
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				IllegalStateException,
				0,
				"config section '%s' is not handled by this factory. report this as a bug.",
				configSection->getName().c_str());
	}

	object->initialize(_ctorArgs, _properties, configSection);
	return object;
}
