/*
 *	 Author: bwilliams
 *  Created: Aug 16, 2012
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CVgAuthContext.h"
#include "CVgAuthUserHandle.h"

CVgAuthUserHandle::CVgAuthUserHandle() :
	_isInitialized(false),
	_vgAuthUserHandle(NULL),
	CAF_CM_INIT_LOG("CVgAuthUserHandle") {
}

CVgAuthUserHandle::~CVgAuthUserHandle() {
	CAF_CM_FUNCNAME("~CVgAuthUserHandle");

	try {
		if (_isInitialized) {
			   VGAuth_UserHandleFree(_vgAuthUserHandle);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

void CVgAuthUserHandle::initialize(
	const SmartPtrCVgAuthContext& vgAuthContext,
	const std::string& signedSamlToken) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(vgAuthContext);
		CAF_CM_VALIDATE_STRING(signedSamlToken);

		const VGAuthError vgAuthError = VGAuth_ValidateSamlBearerToken(vgAuthContext->getPtr(),
			signedSamlToken.c_str(), NULL, 0, NULL, &_vgAuthUserHandle);
		CVgAuthError::checkErrorExc(vgAuthError, "VGAuth_ValidateSamlBearerToken Failed");

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

void CVgAuthUserHandle::initialize(
	const SmartPtrCVgAuthContext& vgAuthContext,
	const std::string& signedSamlToken,
	const std::string& userName) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(vgAuthContext);
		CAF_CM_VALIDATE_STRING(signedSamlToken);
		CAF_CM_VALIDATE_STRING(userName);

	   const VGAuthError vgAuthError = VGAuth_ValidateSamlBearerToken(vgAuthContext->getPtr(),
			signedSamlToken.c_str(), userName.c_str(), 0, NULL, &_vgAuthUserHandle);
		CVgAuthError::checkErrorExc(vgAuthError, "VGAuth_ValidateSamlBearerToken Failed", userName);

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CVgAuthUserHandle::getUserName(
	const SmartPtrCVgAuthContext& vgAuthContext) const {
	CAF_CM_FUNCNAME_VALIDATE("getUserName");

	std::string userNameRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(vgAuthContext);
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		char* userName = NULL;
		const VGAuthError vgAuthError = VGAuth_UserHandleUsername(vgAuthContext->getPtr(),
			_vgAuthUserHandle, &userName);
		CVgAuthError::checkErrorExc(vgAuthError, "VGAuth_UserHandleUsername Failed");
		CAF_CM_VALIDATE_PTR(userName);

		userNameRc = userName;

		VGAuth_FreeBuffer(userName);
	}
	CAF_CM_EXIT;

	return userNameRc;
}

VGAuthUserHandle* CVgAuthUserHandle::getPtr() const {
	CAF_CM_FUNCNAME_VALIDATE("getPtr");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _vgAuthUserHandle;
}
