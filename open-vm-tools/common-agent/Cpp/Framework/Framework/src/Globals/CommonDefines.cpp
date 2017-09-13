/*
 *	Copyright (c) 2011 VMware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CommonDefines.h"

namespace Caf {
	const char* _sAppConfigGlobalParamRootDir = "root_dir";
	const char* _sAppConfigGlobalParamDataDir = "data_dir";
	const char* _sAppConfigGlobalParamLogDir = "log_dir";
	const char* _sAppConfigGlobalParamLogConfigFile = "log_config_file";
	const char* _sAppConfigGlobalParamInputDir = "input_dir";
	const char* _sAppConfigGlobalParamOutputDir = "output_dir";
	const char* _sAppConfigGlobalParamDbDir = "db_dir";
	const char* _sAppConfigGlobalThreadStackSizeKb = "thread_stack_size_kb";

	const GUID CAFCOMMON_GUID_NULL = {0};

#ifdef WIN32
	const char* CAFCOMMON_PATH_DELIM = "\\";
	const char CAFCOMMON_PATH_DELIM_CHAR = '\\';
#else
	const char* CAFCOMMON_PATH_DELIM = "/";
	const char CAFCOMMON_PATH_DELIM_CHAR = '/';
#endif
}
