/*
 *  Created on: Aug 20, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "AmqpListenerWorker.h"
#include "Common/CLoggingUtils.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#ifndef WIN32
#include <syslog.h>
#endif

bool _gDaemonized = true;
bool _gSysLogInfos = false;
SmartPtrAmqpListenerWorker _gAmqpListenerWorker;

using namespace Caf;

#ifndef WIN32
extern "C" void TermHandler(int32 signum);
#endif

int32 main(int32 argc, char** argv) {
	HRESULT hr = CafInitialize::init();
	if (hr != S_OK) {
#ifndef WIN32
		::syslog(LOG_ERR, "CommAmqpListener: CafInitialize::init() failed 0x%08X.", hr);
#endif
		::fprintf(stderr, "CommAmqpListener: CafInitialize::init() failed 0x%08X\n", hr);
		return 1;
	}

	CafInitialize::serviceConfig();

	CAF_CM_STATIC_FUNC_LOG( "CommAmqpListener", "main" );
	SmartPtrIAppConfig appConfig;
	try {
		std::string appConfigEnv;
		CEnvironmentUtils::readEnvironmentVar("CAF_APPCONFIG", appConfigEnv);
		if (appConfigEnv.empty()) {
			Cdeqstr deqstr;
			deqstr.push_back("cafenv-appconfig");
			deqstr.push_back("persistence-appconfig");
			deqstr.push_back("CommAmqpListener-appconfig");
			deqstr.push_back("custom-appconfig");
			appConfig = getAppConfig(deqstr);
		} else {
			appConfig = getAppConfig();
		}
	} catch(CCafException *ex) {
#ifndef WIN32
		::syslog(
				LOG_ERR,
				"CommAmqpListener: getAppConfig() failed . %s",
				ex->getFullMsg().c_str());
#endif
		::fprintf(
				stderr,
				"CommAmqpListener: getAppConfig() failed . %s\n",
				ex->getFullMsg().c_str());
		ex->Release();
	} catch (std::exception ex) {
#ifndef WIN32
		::syslog(
				LOG_ERR,
				"CommAmqpListener: getAppConfig() failed . %s",
				ex.what());
#endif
		::fprintf(
				stderr,
				"CommAmqpListener: getAppConfig() failed . %s\n",
				ex.what());
	} catch (...) {
#ifndef WIN32
		::syslog(
				LOG_ERR,
				"CommAmqpListener: getAppConfig() failed . unknown exception");
#endif
		::fprintf(
				stderr,
				"CommAmqpListener: getAppConfig() failed . unknown exception\n");
	}

	if (!appConfig) {
		CafInitialize::term();
		return 1;
	}

	int32 iRc = 0;

	try {
		_gAmqpListenerWorker.CreateInstance();

		const uint32 maxStrLen = 4096;
		if ((argc < 1) || (NULL == argv) || (NULL == argv[0]) || (::strnlen(argv[0], maxStrLen) >= maxStrLen)) {
			CAF_CM_EXCEPTION_VA0(E_INVALIDARG, "argc/argv are invalid");
		}

#ifdef WIN32
		CWinService::initialize(_gAmqpListenerWorker);
		CWinService::execute(argc, argv);
#else
		const std::string procPath(reinterpret_cast<const char*>(argv[0]));
		Cdeqstr parts = CStringUtils::split(procPath, G_DIR_SEPARATOR);
		std::string procName = "CommAmqpListener";
		if (parts.size()) {
			procName = parts.back();
		}

		CDaemonUtils::MakeDaemon(
			argc,
			argv,
			procPath,
			procName,
			TermHandler,
			_gDaemonized,
			_gSysLogInfos);

		CLoggingUtils::setStartupConfigFile(
			AppConfigUtils::getRequiredString(_sAppConfigGlobalParamLogConfigFile),
			AppConfigUtils::getRequiredString(_sAppConfigGlobalParamLogDir));

		_gAmqpListenerWorker->doWork();
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
				"CommAmqpListener: %s",
				msg.c_str());
#endif

		::fprintf(
				stderr,
				"CommAmqpListener: %s\n",
				msg.c_str());
		iRc = 1;
	}
	CAF_CM_CLEAREXCEPTION;
	_gAmqpListenerWorker = NULL;
	CafInitialize::term();
	return iRc;
}

extern "C" void TermHandler(int32 signum) {
	CAF_CM_STATIC_FUNC_LOG_ONLY( "CommAmqpListener", "TermHandler" );

	CAF_CM_ENTER {
		switch (signum) {
			case SIGTERM:
				CAF_CM_LOG_INFO_VA0( "Received SIGTERM" );
				if (_gAmqpListenerWorker) {
					_gAmqpListenerWorker->stopWork();
				}
				break;
			case SIGINT:
				CAF_CM_LOG_INFO_VA0( "Received SIGINT" );
				if (_gAmqpListenerWorker) {
					_gAmqpListenerWorker->stopWork();
				}
				break;
			default:
				CAF_CM_LOG_ERROR_VA1( "Ignoring Unexpected signal %d", signum);
				break;
		}
	}
	CAF_CM_EXIT;
}
