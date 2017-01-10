/*
 *	 Author: bwilliams
 *  Created: Aug 16, 2012
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CVgAuthContext.h"

CVgAuthContext::CVgAuthContext() :
	_isInitialized(false),
	_vgAuthContext(NULL),
	CAF_CM_INIT_LOG("CVgAuthContext") {
}

CVgAuthContext::~CVgAuthContext() {
	CAF_CM_FUNCNAME("~CVgAuthContext");

	try {
		if (_isInitialized) {
			const VGAuthError vgAuthError = VGAuth_Shutdown(_vgAuthContext);
			CVgAuthError::checkErrorExc(vgAuthError, "VGAuth_Shutdown Failed");
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

void CVgAuthContext::initialize(
	const std::string& applicationName) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(applicationName);

		const VGAuthError vgAuthError = VGAuth_Init(applicationName.c_str(), 0, NULL, &_vgAuthContext);
		CVgAuthError::checkErrorExc(vgAuthError, "VGAuth_Init Failed", applicationName);
		CAF_CM_VALIDATE_PTR(_vgAuthContext);

		_applicationName = applicationName;
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

VGAuthContext* CVgAuthContext::getPtr() const {
	CAF_CM_FUNCNAME_VALIDATE("getPtr");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _vgAuthContext;
}

std::string CVgAuthContext::getApplicationName() const {
	CAF_CM_FUNCNAME_VALIDATE("getApplicationName");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _applicationName;
}
