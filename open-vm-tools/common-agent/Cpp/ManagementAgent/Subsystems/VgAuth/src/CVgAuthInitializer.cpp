/*
 *	 Author: bwilliams
 *  Created: Aug 16, 2012
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CVgAuthContext.h"
#include "CVgAuthInitializer.h"
#include "CVgAuthImpersonation.h"

CVgAuthInitializer::CVgAuthInitializer() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CVgAuthInitializer") {
}

CVgAuthInitializer::~CVgAuthInitializer() {
}

void CVgAuthInitializer::initialize(
	const std::string& applicationName) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(applicationName);

		VGAuth_SetLogHandler(logHandler, NULL, 0, NULL);

		_vgAuthContext.CreateInstance();
		_vgAuthContext->initialize(applicationName);

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

SmartPtrCVgAuthContext CVgAuthInitializer::getContext() const {
	CAF_CM_FUNCNAME_VALIDATE("initializeThirdParty");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _vgAuthContext;
}

void CVgAuthInitializer::installClient() const {
	CAF_CM_FUNCNAME_VALIDATE("installClient");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		const VGAuthError vgAuthError = VGAuth_InstallClient(_vgAuthContext->getPtr(), 0, NULL);
		CVgAuthError::checkErrorExc(vgAuthError, "VGAuth_InstallClient Failed", _vgAuthContext->getApplicationName());
	}
	CAF_CM_EXIT;
}

void CVgAuthInitializer::uninstallClient() const {
	CAF_CM_FUNCNAME_VALIDATE("uninstallClient");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		const VGAuthError vgAuthError = VGAuth_UninstallClient(_vgAuthContext->getPtr(), 0, NULL);
		CVgAuthError::checkErrorExc(vgAuthError, "VGAuth_UninstallClient Failed", _vgAuthContext->getApplicationName());
	}
	CAF_CM_EXIT;
}

void CVgAuthInitializer::endImpersonation() {
	CAF_CM_FUNCNAME_VALIDATE("endImpersonation");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		CVgAuthImpersonation::endImpersonation(_vgAuthContext);
	}
	CAF_CM_EXIT;
}

void CVgAuthInitializer::logHandler(
	const char *logDomain,
    int32 logLevel,
    const char *msg,
    void *userData) {
	CAF_CM_STATIC_FUNC_LOG_ONLY("CVgAuthInitializer", "logHandler");

	CAF_CM_ENTER {
		switch (logLevel & G_LOG_LEVEL_MASK) {
			case G_LOG_LEVEL_ERROR:
				CAF_CM_LOG_ERROR_VA2("[ERROR][%s] %s", logDomain ? logDomain : "", msg);
			break;

			case G_LOG_LEVEL_CRITICAL:
				CAF_CM_LOG_ERROR_VA2("[CRITICAL][%s] %s", logDomain ? logDomain : "", msg);
			break;

			case G_LOG_LEVEL_WARNING:
				CAF_CM_LOG_WARN_VA2("[WARNING][%s] %s", logDomain ? logDomain : "", msg);
			break;

			case G_LOG_LEVEL_INFO:
				CAF_CM_LOG_INFO_VA2("[INFO][%s] %s", logDomain ? logDomain : "", msg);
			break;

			case G_LOG_LEVEL_MESSAGE:
				CAF_CM_LOG_DEBUG_VA2("[MESSAGE][%s] %s", logDomain ? logDomain : "", msg);
			break;

			case G_LOG_LEVEL_DEBUG:
				CAF_CM_LOG_DEBUG_VA2("[DEBUG][%s] %s", logDomain ? logDomain : "", msg);
			break;

			default:
				CAF_CM_LOG_WARN_VA2("[default][%s] %s", logDomain ? logDomain : "", msg);
		}
	}
	CAF_CM_EXIT;
}
