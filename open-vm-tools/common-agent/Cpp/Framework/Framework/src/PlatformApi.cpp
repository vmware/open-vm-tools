/*
 *  Created: Nov 14, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */
#include "stdafx.h"
#include "PlatformApi.h"

std::string BasePlatform::PlatformApi::GetApiErrorMessage(const uint32 code) {
	char *buffer = NULL;
	uint32 msgLen = ::FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		0,
		code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&buffer),
		0,
		NULL);
	if (msgLen == 0) {
		// try again by stripping away the HRESULT_FROM_WIN32 bits
		// in case this error code was translated first.
		uint32 subcode = HRESULT_CODE(code);
		msgLen = ::FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			0,
			code,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&buffer),
			0,
			NULL);
	}
	
	// strip ending CRLFs
	while (msgLen && (('\n' == buffer[msgLen-1]) || ('\r' == buffer[msgLen-1]))) {
		buffer[msgLen-1] = 0;
		--msgLen;
	}
	
	std::string message = msgLen ? std::string(buffer) : std::string();
	::LocalFree(buffer);
	return message;
}
