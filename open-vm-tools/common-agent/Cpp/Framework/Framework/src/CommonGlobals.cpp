/*
 *	 Author: bwilliams
 *  Created: 10/19/2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include <CommonGlobals.h>

namespace Caf {
	const char* _sConfigTmpDir = "tmp_dir";
	const char* _sConfigInputDir = "input_dir";
	const char* _sConfigOutputDir = "output_dir";
	const char* _sConfigSchemaCacheDir = "schema_cache_dir";
	const char* _sConfigProviderRegDir = "provider_reg_dir";
	const char* _sConfigInstallDir = "install_dir";
	const char* _sConfigInvokersDir = "invokers_dir";
	const char* _sConfigProvidersDir = "providers_dir";
	const char* _sConfigCommonPackagesDir = "common_packages_dir";
	const char* _sConfigWorkingDir = "working_dir";
	const char* _sConfigPrivateKeyFile = "private_key_file";
	const char* _sConfigCertFile = "cert_file";

	const char* _sProviderHostArea = "providerHost";
	const char* _sManagementAgentArea = "managementAgent";

	const char* _sSchemaSummaryFilename = "schemaSummary.xml";
	const char* _sProviderResponseFilename = "providerResponse.xml";
	const char* _sStdoutFilename = "_provider_stdout_";
	const char* _sStderrFilename = "_provider_stderr_";

	const char* _sPayloadRequestFilename = "payloadRequest.xml";
	const char* _sResponseFilename = "response.xml";
	const char* _sErrorResponseFilename = "errorResponse.xml";
	const char* _sProviderRequestFilename = "providerRequest.xml";
	const char* _sInfraErrFilename = "_infraError_";

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
