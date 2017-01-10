/*
 *	 Author: bwilliams
 *  Created: Aug 16, 2012
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CVgAuthContext.h"
#include "CVgAuthUserHandle.h"
#include "CVgAuthImpersonation.h"

CVgAuthImpersonation::CVgAuthImpersonation() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CVgAuthImpersonation") {
}

CVgAuthImpersonation::~CVgAuthImpersonation() {
	CAF_CM_FUNCNAME("~CVgAuthImpersonation");

	try {
		if (_isInitialized) {
			endImpersonation(_vgAuthContext);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

void CVgAuthImpersonation::impersonateAndManage(
	const SmartPtrCVgAuthContext& vgAuthContext,
	const SmartPtrCVgAuthUserHandle& vgAuthUserHandle) {
	CAF_CM_FUNCNAME_VALIDATE("impersonateAndManage");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(vgAuthContext);
		CAF_CM_VALIDATE_SMARTPTR(vgAuthUserHandle);

		beginImpersonation(vgAuthContext, vgAuthUserHandle);

		_vgAuthContext = vgAuthContext;
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

void CVgAuthImpersonation::beginImpersonation(
	const SmartPtrCVgAuthContext& vgAuthContext,
	const SmartPtrCVgAuthUserHandle& vgAuthUserHandle) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CVgAuthImpersonation", "beginImpersonation");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(vgAuthContext);
		CAF_CM_VALIDATE_SMARTPTR(vgAuthUserHandle);

		const std::string userName = vgAuthUserHandle->getUserName(vgAuthContext);
		CAF_CM_LOG_DEBUG_VA1("Beginning impersonation - %s", userName.c_str());

		const VGAuthError vgAuthError = VGAuth_Impersonate(
			vgAuthContext->getPtr(), vgAuthUserHandle->getPtr(), 0, NULL);
		CVgAuthError::checkErrorExc(vgAuthError, "VGAuth_Impersonate Failed");
	}
	CAF_CM_EXIT;
}

void CVgAuthImpersonation::endImpersonation(
	const SmartPtrCVgAuthContext& vgAuthContext) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CVgAuthImpersonation", "endImpersonation");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(vgAuthContext);

		CAF_CM_LOG_DEBUG_VA0("Ending impersonation");

		const VGAuthError vgAuthError = VGAuth_EndImpersonation(vgAuthContext->getPtr());
		CVgAuthError::checkErrorExc(vgAuthError, "VGAuth_EndImpersonation Failed");
	}
	CAF_CM_EXIT;
}
