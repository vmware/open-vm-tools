#ifndef CommonGlobals_h_
#define CommonGlobals_h_

#include "FrameworkLink.h"

namespace Caf {
	extern FRAMEWORK_LINKAGE const char* _sConfigTmpDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigInputDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigOutputDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigSchemaCacheDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigProviderRegDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigInstallDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigInvokersDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigProvidersDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigCommonPackagesDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigWorkingDir;
	extern FRAMEWORK_LINKAGE const char* _sConfigPrivateKeyFile;
	extern FRAMEWORK_LINKAGE const char* _sConfigCertFile;

	extern FRAMEWORK_LINKAGE const char* _sProviderHostArea;
	extern FRAMEWORK_LINKAGE const char* _sManagementAgentArea;

	extern FRAMEWORK_LINKAGE const char* _sSchemaSummaryFilename;
	extern FRAMEWORK_LINKAGE const char* _sProviderResponseFilename;
	extern FRAMEWORK_LINKAGE const char* _sStdoutFilename;
	extern FRAMEWORK_LINKAGE const char* _sStderrFilename;

	extern FRAMEWORK_LINKAGE const char* _sPayloadRequestFilename;
	extern FRAMEWORK_LINKAGE const char* _sResponseFilename;
	extern FRAMEWORK_LINKAGE const char* _sErrorResponseFilename;
	extern FRAMEWORK_LINKAGE const char* _sProviderRequestFilename;
	extern FRAMEWORK_LINKAGE const char* _sInfraErrFilename;

	// AppConfig common parameter names
	extern FRAMEWORK_LINKAGE const char* _sAppConfigGlobalParamRootDir;
	extern FRAMEWORK_LINKAGE const char* _sAppConfigGlobalParamDataDir;
	extern FRAMEWORK_LINKAGE const char* _sAppConfigGlobalParamLogDir;
	extern FRAMEWORK_LINKAGE const char* _sAppConfigGlobalParamLogConfigFile;
	extern FRAMEWORK_LINKAGE const char* _sAppConfigGlobalParamInputDir;
	extern FRAMEWORK_LINKAGE const char* _sAppConfigGlobalParamOutputDir;
	extern FRAMEWORK_LINKAGE const char* _sAppConfigGlobalParamDbDir;
	extern FRAMEWORK_LINKAGE const char* _sAppConfigGlobalThreadStackSizeKb;

	extern FRAMEWORK_LINKAGE const GUID CAFCOMMON_GUID_NULL;

	extern FRAMEWORK_LINKAGE const char* CAFCOMMON_PATH_DELIM;
	extern FRAMEWORK_LINKAGE const char CAFCOMMON_PATH_DELIM_CHAR;
}

#endif
