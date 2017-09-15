/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "CAttachmentRequestTransformer.h"

using namespace Caf;

CAttachmentRequestTransformer::CAttachmentRequestTransformer() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CAttachmentRequestTransformer") {
}

CAttachmentRequestTransformer::~CAttachmentRequestTransformer() {
}

void CAttachmentRequestTransformer::initializeBean(
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

void CAttachmentRequestTransformer::terminateBean() {
}

void CAttachmentRequestTransformer::initialize() {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

bool CAttachmentRequestTransformer::isResponsible(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);
	}
	CAF_CM_EXIT;

	return false;
}

SmartPtrIIntegrationObject CAttachmentRequestTransformer::createObject(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("createObject");

	SmartPtrIIntegrationObject rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		rc.CreateInstance(_sObjIdAttachmentRequestTransformerInstance);
		rc->initialize(_ctorArgs, _properties, configSection);
	}
	CAF_CM_EXIT;

	return rc;
}
