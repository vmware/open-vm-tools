/*
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "CVgAuthError.h"

using namespace Caf;

void CVgAuthError::checkErrorExc(
	const VGAuthError& vgAuthError,
    const std::string& msg) {
	CAF_CM_STATIC_FUNC("CVgAuthError", "checkErrorExc");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(msg);

		if (VGAUTH_FAILED(vgAuthError)) {
			CAF_CM_EXCEPTIONEX_VA2(UnsupportedOperationException, getErrorCode(vgAuthError),
				"%s (%s)", msg.c_str(), getErrorMsg(vgAuthError).c_str());
		}
	}
	CAF_CM_EXIT;
}

void CVgAuthError::checkErrorExc(
	const VGAuthError& vgAuthError,
    const std::string& msg,
    const std::string& addtlInfo) {
	CAF_CM_STATIC_FUNC("CVgAuthError", "checkErrorExc");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(msg);
		CAF_CM_VALIDATE_STRING(addtlInfo);

		if (VGAUTH_FAILED(vgAuthError)) {
			CAF_CM_EXCEPTIONEX_VA3(UnsupportedOperationException, getErrorCode(vgAuthError),
				"%s (%s) - %s", msg.c_str(), getErrorMsg(vgAuthError).c_str(), addtlInfo.c_str());
		}
	}
	CAF_CM_EXIT;
}

void CVgAuthError::checkErrorErr(
	const VGAuthError& vgAuthError,
    const std::string& msg) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CVgAuthError", "checkErrorErr");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(msg);

		if (VGAUTH_FAILED(vgAuthError)) {
			CAF_CM_LOG_ERROR_VA2("%s (%s)", msg.c_str(), getErrorMsg(vgAuthError).c_str());
		}
	}
	CAF_CM_EXIT;
}

void CVgAuthError::checkErrorErr(
	const VGAuthError& vgAuthError,
    const std::string& msg,
    const std::string& addtlInfo) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CVgAuthError", "checkErrorErr");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(msg);
		CAF_CM_VALIDATE_STRING(addtlInfo);

		if (VGAUTH_FAILED(vgAuthError)) {
			CAF_CM_LOG_ERROR_VA3("%s (%s) - %s", msg.c_str(), getErrorMsg(vgAuthError).c_str(), addtlInfo.c_str());
		}
	}
	CAF_CM_EXIT;
}

std::string CVgAuthError::getErrorMsg(
	const VGAuthError& vgAuthError) {

	std::string vgAuthErrorMsg;

	CAF_CM_ENTER {
		switch(vgAuthError) {
			case VGAUTH_E_OK:
				vgAuthErrorMsg = "VGAUTH_E_OK";
			break;
			case VGAUTH_E_FAIL:
				vgAuthErrorMsg = "VGAUTH_E_FAIL";
			break;
			case VGAUTH_E_INVALID_ARGUMENT:
				vgAuthErrorMsg = "VGAUTH_E_INVALID_ARGUMENT";
			break;
			case VGAUTH_E_INVALID_CERTIFICATE:
				vgAuthErrorMsg = "VGAUTH_E_INVALID_CERTIFICATE";
			break;
			case VGAUTH_E_PERMISSION_DENIED:
				vgAuthErrorMsg = "VGAUTH_E_PERMISSION_DENIED";
			break;
			case VGAUTH_E_OUT_OF_MEMORY:
				vgAuthErrorMsg = "VGAUTH_E_OUT_OF_MEMORY";
			break;
			case VGAUTH_E_COMM:
				vgAuthErrorMsg = "VGAUTH_E_COMM";
			break;
			case VGAUTH_E_NOTIMPLEMENTED:
				vgAuthErrorMsg = "VGAUTH_E_NOTIMPLEMENTED";
			break;
			case VGAUTH_E_NOT_CONNECTED:
				vgAuthErrorMsg = "VGAUTH_E_NOT_CONNECTED";
			break;
			case VGAUTH_E_VERSION_MISMATCH:
				vgAuthErrorMsg = "VGAUTH_E_VERSION_MISMATCH";
			break;
			case VGAUTH_E_SECURITY_VIOLATION:
				vgAuthErrorMsg = "VGAUTH_E_SECURITY_VIOLATION";
			break;
			case VGAUTH_E_CERT_ALREADY_EXISTS:
				vgAuthErrorMsg = "VGAUTH_E_CERT_ALREADY_EXISTS";
			break;
			case VGAUTH_E_AUTHENTICATION_DENIED:
				vgAuthErrorMsg = "VGAUTH_E_AUTHENTICATION_DENIED";
			break;
			case VGAUTH_E_INVALID_TICKET:
				vgAuthErrorMsg = "VGAUTH_E_INVALID_TICKET";
			break;
			case VGAUTH_E_MULTIPLE_MAPPINGS:
				vgAuthErrorMsg = "VGAUTH_E_MULTIPLE_MAPPINGS";
			break;
			case VGAUTH_E_ALREADY_IMPERSONATING:
				vgAuthErrorMsg = "VGAUTH_E_ALREADY_IMPERSONATING";
			break;
			case VGAUTH_E_NO_SUCH_USER:
				vgAuthErrorMsg = "VGAUTH_E_NO_SUCH_USER";
			break;
			case VGAUTH_E_SERVICE_NOT_RUNNING:
				vgAuthErrorMsg = "VGAUTH_E_SERVICE_NOT_RUNNING";
			break;
			case VGAUTH_E_SYSTEM_ERRNO:
			{
				const uint32 vgAuthErrorCode = VGAUTH_ERROR_EXTRA_ERROR(vgAuthError);
				const std::string errorMsg = CStringConv::toString<uint32>(vgAuthErrorCode);
				vgAuthErrorMsg = std::string("VGAUTH_E_SYSTEM_ERRNO, msg: " + errorMsg);
			}
			break;
			case VGAUTH_E_SYSTEM_WINDOWS:
			{
				const uint32 vgAuthErrorCode = VGAUTH_ERROR_EXTRA_ERROR(vgAuthError);
#ifdef WIN32
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(vgAuthErrorCode);
#else
				const std::string errorMsg = CStringConv::toString<uint32>(vgAuthErrorCode);
#endif
				vgAuthErrorMsg = std::string("VGAUTH_E_SYSTEM_WINDOWS, msg: " + errorMsg);
			}
			break;
			case VGAUTH_E_TOO_MANY_CONNECTIONS:
				vgAuthErrorMsg = "VGAUTH_E_TOO_MANY_CONNECTIONS";
			break;
			case VGAUTH_E_UNSUPPORTED:
				vgAuthErrorMsg = "VGAUTH_E_UNSUPPORTED";
			break;
			default:
				vgAuthErrorMsg = "Unknown";
		}
	}
	CAF_CM_EXIT;

	return vgAuthErrorMsg;
}

uint32 CVgAuthError::getErrorCode(
	const VGAuthError& vgAuthError) {

	uint32 vgAuthErrorCode;

	CAF_CM_ENTER {
		switch(vgAuthError) {
			case VGAUTH_E_OK:
			case VGAUTH_E_FAIL:
			case VGAUTH_E_INVALID_ARGUMENT:
			case VGAUTH_E_INVALID_CERTIFICATE:
			case VGAUTH_E_PERMISSION_DENIED:
			case VGAUTH_E_OUT_OF_MEMORY:
			case VGAUTH_E_COMM:
			case VGAUTH_E_NOTIMPLEMENTED:
			case VGAUTH_E_NOT_CONNECTED:
			case VGAUTH_E_VERSION_MISMATCH:
			case VGAUTH_E_SECURITY_VIOLATION:
			case VGAUTH_E_CERT_ALREADY_EXISTS:
			case VGAUTH_E_AUTHENTICATION_DENIED:
			case VGAUTH_E_INVALID_TICKET:
			case VGAUTH_E_MULTIPLE_MAPPINGS:
			case VGAUTH_E_ALREADY_IMPERSONATING:
			case VGAUTH_E_NO_SUCH_USER:
			case VGAUTH_E_SERVICE_NOT_RUNNING:
			case VGAUTH_E_TOO_MANY_CONNECTIONS:
			case VGAUTH_E_UNSUPPORTED:
				vgAuthErrorCode = VGAUTH_ERROR_CODE(vgAuthError);
			break;
			case VGAUTH_E_SYSTEM_ERRNO:
			case VGAUTH_E_SYSTEM_WINDOWS:
				vgAuthErrorCode = VGAUTH_ERROR_EXTRA_ERROR(vgAuthError);
			break;
			default:
				vgAuthErrorCode = E_UNEXPECTED;
		}
	}
	CAF_CM_EXIT;

	return vgAuthErrorCode;
}
