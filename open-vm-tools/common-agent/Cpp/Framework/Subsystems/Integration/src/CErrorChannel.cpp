/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "CErrorChannel.h"
#include "CErrorChannelInstance.h"

using namespace Caf;

CErrorChannel::CErrorChannel() :
	_isInitialized(false),
	CAF_CM_INIT("CErrorChannel"){
}

CErrorChannel::~CErrorChannel() {
}

void CErrorChannel::initializeBean(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties) {

	CAF_CM_FUNCNAME_VALIDATE("initializeBean");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
		CAF_CM_VALIDATE_STL_EMPTY(properties);

		_ctorArgs = ctorArgs;
		_properties = properties;

		initialize();
	}
	CAF_CM_EXIT;
}

void CErrorChannel::terminateBean() {
}

void CErrorChannel::initialize() {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

bool CErrorChannel::isResponsible(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);
	}
	CAF_CM_EXIT;

	return false;
}

SmartPtrIIntegrationObject CErrorChannel::createObject(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("createObject");

	SmartPtrIIntegrationObject rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		// configSection is optional

		SmartPtrCErrorChannelInstance instance;
		instance.CreateInstance();
		instance->initialize(_ctorArgs, _properties, configSection);
		rc.QueryInterface(instance, false);
		CAF_CM_VALIDATE_INTERFACE(rc);
	}
	CAF_CM_EXIT;

	return rc;
}
