/*
 *	 Author: bwilliams
 *  Created: 10/19/2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CEnvironmentUtils.h"
using namespace Caf;


void CEnvironmentUtils::readEnvironmentVar(const char* varname, std::string& rValue) {
	CAF_CM_STATIC_FUNC_VALIDATE("CEnvironmentUtils", "readEnvironmentVar");
	CAF_CM_VALIDATE_PTR(varname);
	#ifdef WIN32
		char* dupEnvBuf = NULL;
		size_t dupEnvBufLen = 0;
		errno_t apiRc = ::_dupenv_s(&dupEnvBuf, &dupEnvBufLen, varname);
		rValue = std::string(dupEnvBuf && ::strlen(dupEnvBuf) ? dupEnvBuf : "");
		::free(dupEnvBuf);
		dupEnvBuf = NULL;
	#else
		const char *prValue = ::getenv(varname);
		if (NULL != prValue) {
			rValue = prValue;
		}
	#endif
}

void CEnvironmentUtils::writeEnvironmentVar(const char* varname, std::string& rValue) {
	CAF_CM_STATIC_FUNC_VALIDATE("CEnvironmentUtils", "writeEnvironmentVar");
	CAF_CM_VALIDATE_PTR(varname);
	#ifdef WIN32
		std::ostringstream formattedStream;
		formattedStream << std::string(varname) << std::string("=") << rValue;
		::_putenv(formattedStream.str().c_str());
	#else
		//	Not Implemented
	#endif
}
