/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "CCmsMessageTransformer.h"

using namespace Caf;

CCmsMessageTransformer::CCmsMessageTransformer() :
	_isInitialized(false),
	CAF_CM_INIT("CCmsMessageTransformer") {
}

CCmsMessageTransformer::~CCmsMessageTransformer() {
}

void CCmsMessageTransformer::initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);

	_ctorArgs = ctorArgs;
	_properties = properties;
	initialize();
}

void CCmsMessageTransformer::terminateBean() {
}

void CCmsMessageTransformer::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_isInitialized = true;
}

bool CCmsMessageTransformer::isResponsible(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	return false;
}

SmartPtrIIntegrationObject CCmsMessageTransformer::createObject(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("createObject");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	SmartPtrIIntegrationObject rc;
	rc.CreateInstance(_sObjIdCommIntegrationCmsMessageTransformerInstance);
	rc->initialize(_ctorArgs, _properties, configSection);

	return rc;
}
