/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CManagementAgentHostWork.h"
#include "Common/CLoggingUtils.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#ifndef WIN32
#include <syslog.h>
#endif

bool _gDaemonized = true;
bool _gSysLogInfos = false;
SmartPtrCManagementAgentHostWork _gManagementAgentHostWork;

using namespace Caf;

#ifndef WIN32
extern "C" void TermHandler(int32 signum);
#endif

int32 main(int32 argc, char** argv) {
	HRESULT hr = CafInitialize::init();

	if (hr != S_OK) {
#ifndef WIN32
		::syslog(LOG_ERR, "ManagementAgentHost: CafInitialize::init() failed 0x%08X.", hr);
#endif
		::fprintf(stderr, "ManagementAgentHost: CafInitialize::init() failed 0x%08X\n", hr);
		return 1;
	}


	SmartPtrIAppConfig appConfig;
	try {
		CafInitialize::serviceConfig();

		std::string appConfigEnv;
		CEnvironmentUtils::readEnvironmentVar("CAF_APPCONFIG", appConfigEnv);
		if (appConfigEnv.empty()) {
			Cdeqstr deqstr;
			deqstr.push_back("cafenv-appconfig");
			deqstr.push_back("persistence-appconfig");
			deqstr.push_back("ma-appconfig");
			deqstr.push_back("custom-appconfig");
			appConfig = getAppConfig(deqstr);
		} else {
			appConfig = getAppConfig();
		}
	} catch(CCafException *ex) {
#ifndef WIN32
		::syslog(
				LOG_ERR,
				"ManagementAgentHost: getAppConfig() failed . %s",
				ex->getFullMsg().c_str());
#endif
		::fprintf(
				stderr,
				"ManagementAgentHost: getAppConfig() failed . %s\n",
				ex->getFullMsg().c_str());
		ex->Release();
	} catch (std::exception ex) {
#ifndef WIN32
		::syslog(
				LOG_ERR,
				"ManagementAgentHost: getAppConfig() failed . %s",
				ex.what());
#endif
		::fprintf(
				stderr,
				"ManagementAgentHost: getAppConfig() failed . %s\n",
				ex.what());
	} catch (...) {
#ifndef WIN32
		::syslog(
				LOG_ERR,
				"ManagementAgentHost: getAppConfig() failed . unknown exception");
#endif
		::fprintf(
				stderr,
				"ManagementAgentHost: getAppConfig() failed . unknown exception\n");
	}

	CAF_CM_STATIC_FUNC_LOG("ManagementAgentHostMain", "main");
	int32 iRc = 0;
	try {
		if (!appConfig) {
			CafInitialize::term();
			return 1;
		}

		const std::string cafBinDir = AppConfigUtils::getRequiredString("globals", "bin_dir");
		g_setenv("CAF_BIN_DIR", cafBinDir.c_str(), TRUE);

		const std::string cafLibDir = AppConfigUtils::getRequiredString("globals", "lib_dir");
		g_setenv("CAF_LIB_DIR", cafLibDir.c_str(), TRUE);

		_gManagementAgentHostWork.CreateInstance();
		_gManagementAgentHostWork->initialize();

		const uint32 maxStrLen = 4096;
		if ((argc < 1) || (NULL == argv) || (NULL == argv[0]) || (::strnlen(argv[0], maxStrLen) >= maxStrLen)) {
			CAF_CM_EXCEPTION_VA0(E_INVALIDARG, "argc/argv are invalid");
		}

#ifdef WIN32
		CWinService::initialize(_gManagementAgentHostWork);
		CWinService::execute(argc, argv);
#else
		if (S_OK != hr) {
			::syslog(LOG_ERR, "CafInitialize::init() failed (0x%08X.", hr);
			return 1;
		}

		const std::string procPath(argv[0]);
		CDaemonUtils::MakeDaemon(
			argc,
			argv,
			procPath,
			"ManagementAgentHost",
			TermHandler,
			_gDaemonized,
			_gSysLogInfos);

		CLoggingUtils::setStartupConfigFile(
			AppConfigUtils::getRequiredString(_sAppConfigGlobalParamLogConfigFile),
			AppConfigUtils::getRequiredString(_sAppConfigGlobalParamLogDir));

		_gManagementAgentHostWork->doWork();
#endif

	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT
	CAF_CM_LOG_CRIT_CAFEXCEPTION;

	if (CAF_CM_ISEXCEPTION) {
		const std::string msg = CAF_CM_EXCEPTION_GET_FULLMSG;
#ifndef WIN32
		::syslog(
				LOG_ERR,
				"ManagementAgentHost: %s",
				msg.c_str());
#endif

		::fprintf(
				stderr,
				"ManagementAgentHost: %s\n",
				msg.c_str());
		iRc = 1;
	}
	CAF_CM_CLEAREXCEPTION;

	CafInitialize::term();

	return iRc;
}

#ifndef WIN32
extern "C" void TermHandler(int32 signum) {
	CAF_CM_STATIC_FUNC_LOG_ONLY( "ManagementAgentHost", "TermHandler" );

	CAF_CM_ENTER {
		switch (signum) {
			case SIGTERM:
				CAF_CM_LOG_INFO_VA0( "Received SIGTERM" );
				if (! _gManagementAgentHostWork.IsNull()) {
					_gManagementAgentHostWork->stopWork();
				}
				break;
			case SIGINT:
				CAF_CM_LOG_INFO_VA0( "Received SIGINT" );
				if (! _gManagementAgentHostWork.IsNull()) {
					_gManagementAgentHostWork->stopWork();
				}
				break;
			default:
				CAF_CM_LOG_ERROR_VA1( "Ignoring Unexpected signal %d", signum);
				break;
		}
	}
	CAF_CM_EXIT;
}
#endif
