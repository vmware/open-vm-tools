/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "CEnvelopeToPayloadTransformer.h"

using namespace Caf;

CEnvelopeToPayloadTransformer::CEnvelopeToPayloadTransformer() :
	_isInitialized(false),
	CAF_CM_INIT("CEnvelopeToPayloadTransformer") {
}

CEnvelopeToPayloadTransformer::~CEnvelopeToPayloadTransformer() {
}

void CEnvelopeToPayloadTransformer::initializeBean(
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

void CEnvelopeToPayloadTransformer::terminateBean() {
}

void CEnvelopeToPayloadTransformer::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_isInitialized = true;
}

bool CEnvelopeToPayloadTransformer::isResponsible(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	return false;
}

SmartPtrIIntegrationObject CEnvelopeToPayloadTransformer::createObject(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("createObject");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	SmartPtrIIntegrationObject rc;
	rc.CreateInstance(_sObjIdEnvelopeToPayloadTransformerInstance);
	rc->initialize(_ctorArgs, _properties, configSection);

	return rc;
}
