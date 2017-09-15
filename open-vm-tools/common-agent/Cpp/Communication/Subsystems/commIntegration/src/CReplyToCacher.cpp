/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "CReplyToCacher.h"

using namespace Caf;

CReplyToCacher::CReplyToCacher() :
	_isInitialized(false),
	CAF_CM_INIT("CReplyToCacher") {
}

CReplyToCacher::~CReplyToCacher() {
}

void CReplyToCacher::initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
	CAF_CM_VALIDATE_STL_EMPTY(properties);

	_ctorArgs = ctorArgs;
	_properties = properties;
	initialize();
}

void CReplyToCacher::terminateBean() {
}

void CReplyToCacher::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_isInitialized = true;
}

bool CReplyToCacher::isResponsible(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	return false;
}

SmartPtrIIntegrationObject CReplyToCacher::createObject(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("createObject");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	SmartPtrIIntegrationObject rc;
	rc.CreateInstance(_sObjIdCommIntegrationReplyToCacherInstance);
	rc->initialize(_ctorArgs, _properties, configSection);

	return rc;
}
