/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "CGuestAuthenticator.h"

using namespace Caf;

CGuestAuthenticator::CGuestAuthenticator() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CGuestAuthenticator") {
}

CGuestAuthenticator::~CGuestAuthenticator() {
}

void CGuestAuthenticator::initializeBean(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties) {

	CAF_CM_FUNCNAME_VALIDATE("initializeBean");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
		CAF_CM_VALIDATE_STL(properties);

		_ctorArgs = ctorArgs;
		_properties = properties;

		initialize();

		CAF_CM_LOG_DEBUG_VA1("num properties - %d", properties.size());
	}
	CAF_CM_EXIT;
}

void CGuestAuthenticator::terminateBean() {
}

void CGuestAuthenticator::initialize() {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

bool CGuestAuthenticator::isResponsible(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);
	}
	CAF_CM_EXIT;

	return false;
}

SmartPtrIIntegrationObject CGuestAuthenticator::createObject(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("createObject");

	SmartPtrIIntegrationObject rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		rc.CreateInstance(_sObjIdGuestAuthenticatorInstance);
		rc->initialize(_ctorArgs, _properties, configSection);
	}
	CAF_CM_EXIT;

	return rc;
}
