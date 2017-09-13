/*
 *	Copyright (c) 2011 VMware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _Common_CommonDefines_H
#define _Common_CommonDefines_H

namespace Caf {
	// AppConfig common parameter names
	extern COMMONGLOBALS_LINKAGE const char* _sAppConfigGlobalParamRootDir;
	extern COMMONGLOBALS_LINKAGE const char* _sAppConfigGlobalParamDataDir;
	extern COMMONGLOBALS_LINKAGE const char* _sAppConfigGlobalParamLogDir;
	extern COMMONGLOBALS_LINKAGE const char* _sAppConfigGlobalParamLogConfigFile;
	extern COMMONGLOBALS_LINKAGE const char* _sAppConfigGlobalParamInputDir;
	extern COMMONGLOBALS_LINKAGE const char* _sAppConfigGlobalParamOutputDir;
	extern COMMONGLOBALS_LINKAGE const char* _sAppConfigGlobalParamDbDir;
	extern COMMONGLOBALS_LINKAGE const char* _sAppConfigGlobalThreadStackSizeKb;

	extern COMMONGLOBALS_LINKAGE const GUID CAFCOMMON_GUID_NULL;

	extern COMMONGLOBALS_LINKAGE const char* CAFCOMMON_PATH_DELIM;
	extern COMMONGLOBALS_LINKAGE const char CAFCOMMON_PATH_DELIM_CHAR;
}

#endif
