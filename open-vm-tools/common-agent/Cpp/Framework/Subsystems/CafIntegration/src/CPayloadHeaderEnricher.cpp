/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "CPayloadHeaderEnricher.h"

using namespace Caf;

CPayloadHeaderEnricher::CPayloadHeaderEnricher() :
	_isInitialized(false),
	CAF_CM_INIT("CPayloadHeaderEnricher") {
}

CPayloadHeaderEnricher::~CPayloadHeaderEnricher() {
}

void CPayloadHeaderEnricher::initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties) {

	CAF_CM_FUNCNAME_VALIDATE("initializeBean");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);

		_ctorArgs = ctorArgs;
		_properties = properties;

		initialize();
	}
	CAF_CM_EXIT;
}

void CPayloadHeaderEnricher::terminateBean() {
}

void CPayloadHeaderEnricher::initialize() {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

bool CPayloadHeaderEnricher::isResponsible(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);
	}
	CAF_CM_EXIT;

	return false;
}

SmartPtrIIntegrationObject CPayloadHeaderEnricher::createObject(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("createObject");

	SmartPtrIIntegrationObject rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		rc.CreateInstance(_sObjIdPayloadHeaderEnricherInstance);
		rc->initialize(_ctorArgs, _properties, configSection);
	}
	CAF_CM_EXIT;

	return rc;
}
