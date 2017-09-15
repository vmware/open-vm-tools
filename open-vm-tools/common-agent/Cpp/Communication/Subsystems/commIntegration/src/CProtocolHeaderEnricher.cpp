/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "CProtocolHeaderEnricher.h"

using namespace Caf;

CProtocolHeaderEnricher::CProtocolHeaderEnricher() :
	_isInitialized(false),
	CAF_CM_INIT("CProtocolHeaderEnricher") {
}

CProtocolHeaderEnricher::~CProtocolHeaderEnricher() {
}

void CProtocolHeaderEnricher::initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);

	_ctorArgs = ctorArgs;
	_properties = properties;
	initialize();
}

void CProtocolHeaderEnricher::terminateBean() {
}

void CProtocolHeaderEnricher::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_isInitialized = true;
}

bool CProtocolHeaderEnricher::isResponsible(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	return false;
}

SmartPtrIIntegrationObject CProtocolHeaderEnricher::createObject(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("createObject");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	SmartPtrIIntegrationObject rc;
	rc.CreateInstance(_sObjIdCommIntegrationProtocolHeaderEnricherInstance);
	rc->initialize(_ctorArgs, _properties, configSection);

	return rc;
}
