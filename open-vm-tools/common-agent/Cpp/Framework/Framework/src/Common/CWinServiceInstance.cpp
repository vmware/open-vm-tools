/*
 *  Author: bwilliams
 *  Created: June 25, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CWinServiceInstance.h"

CWinServiceInstance::CWinServiceInstance() :
	CAF_CM_INIT_LOG("CWinServiceInstance"),
	_isInitialized(false) {
}

CWinServiceInstance::~CWinServiceInstance() {
	CAF_CM_FUNCNAME("~CWinServiceInstance");
}

void CWinServiceInstance::initialize(
		const SmartPtrCWinServiceState& winServiceState) {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(winServiceState);

	_winServiceState = winServiceState;
	_isInitialized = true;
}

////////////////////////////////////////////////////////////////////////
//
// CWinServiceInstance::runService()
//
// Fires off and monitors the main service thread
//
////////////////////////////////////////////////////////////////////////
void CWinServiceInstance::runService() {
	CAF_CM_FUNCNAME("runService");

	static const DWORD scdwCheckStateSecs = 10;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Loop until stop event...
		bool bIsContinue = true;
		while (bIsContinue) {
			const bool isSignaled = _winServiceState->waitForServiceStop(
				scdwCheckStateSecs * 1000);
			if (isSignaled) {
				CAF_CM_LOG_INFO_VA0("Received the stop event");

				// Let the worker thread know it's time to stop.
				const SmartPtrIWork work = _winServiceState->getWork();
				work->stopWork();

				bIsContinue = false;
			}

			// Check the worker thread
			if (bIsContinue) {
				const bool isWorkerSignaled =
						_winServiceState->waitForWorkerThreadFinished(10);
				if (isWorkerSignaled) {
					CAF_CM_LOG_INFO_VA0("Worker thread is not running");
					bIsContinue = false;
				}
			}
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;

	try {
		CAF_CM_LOG_INFO_VA0("Service is stopping - waiting for worker thread to finish");

		const bool isSignaled = _winServiceState->waitForWorkerThreadFinished(
			_winServiceState->getWorkerThreadStopMs());
		if (!isSignaled) {
			CAF_CM_LOG_WARN_VA1("Worker thread did not stop within timeout period - %d",
				_winServiceState->getWorkerThreadStopMs());
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;

	CAF_CM_THROWEXCEPTION;
}

void CWinServiceInstance::runWorkerThread() {
	CAF_CM_FUNCNAME("runWorkerThread");

	try {
		CafInitialize::init();
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		_winServiceState->putCurrentServiceState(SERVICE_RUNNING);
		if (_winServiceState->getIsService()) {
			_winServiceState->setStatus();
		}

		const SmartPtrIWork work = _winServiceState->getWork();
		work->doWork();

		_winServiceState->putCurrentServiceState(SERVICE_STOP_PENDING);
		if (_winServiceState->getIsService()) {
			_winServiceState->setStatus();
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;

	try {
		CAF_CM_LOG_INFO_VA0("workerThread is shutting down");

		// Let the main thread know what's going on.
		_winServiceState->signalServiceStop();
		_winServiceState->signalWorkerThreadFinished();
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;

	CafInitialize::term();

	CAF_CM_THROWEXCEPTION;
}

CWinServiceInstance::SCommandLineParams CWinServiceInstance::processCommandLine(
	int32 argc,
	char** argv) const {
	CAF_CM_FUNCNAME("processCommandLine");

	CAF_CM_LOG_DEBUG_VA0("processCommandLine");

	SCommandLineParams commandLineParams;
	if (argc == 1) {
		commandLineParams._eMode = EModeRunAsService;
	} else if (argc == 2) {
		if ((::_stricmp("/Service", argv[1]) == 0)
				|| (::_stricmp("-Service", argv[1]) == 0)
				|| (::_stricmp("/RegServer", argv[1]) == 0)
				|| (::_stricmp("-RegServer", argv[1]) == 0)) {
			commandLineParams._eMode = EModeRegister;
		} else if ((::_stricmp("/UnregService", argv[1]) == 0)
				|| (::_stricmp("-UnregService", argv[1]) == 0)
				|| (::_stricmp("/UnregServer", argv[1]) == 0)
				|| (::_stricmp("-UnregServer", argv[1]) == 0)) {
			commandLineParams._eMode = EModeUnregister;
		} else if ((::_stricmp("/n", argv[1]) == 0)
				|| (::_stricmp("-n", argv[1]) == 0)) {
			commandLineParams._eMode = EModeRunAsConsole;
		} else {
			commandLineParams._eMode = EModeUnknown;
			CWinServiceInstance::usage(_winServiceState->getServiceName());
		}
	} else {
		commandLineParams._eMode = EModeUnknown;
		CWinServiceInstance::usage(_winServiceState->getServiceName());
	}

	return commandLineParams;
}

void CWinServiceInstance::install(
		const std::string& fileName) const {
	CAF_CM_FUNCNAME("install");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA2("Installing the service - serviceName: %s, path: %s",
			_winServiceState->getServiceName().c_str(), fileName.c_str());

	// Create the service.
	CWinScm winScm(_winServiceState->getServiceName());
	winScm.createService(
			fileName, // ServiceFilename
			_winServiceState->getDisplayName(), // ServiceDisplayName,
			_winServiceState->getDescription(), // ServiceDescription,
			std::string(), // ServiceAccountName,
			std::string(), //ServiceAccountPasswd,
			SERVICE_AUTO_START);
}

void CWinServiceInstance::uninstall() const {
	CAF_CM_FUNCNAME("uninstall");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA1("Uninstalling the service - serviceName: %s",
		_winServiceState->getServiceName().c_str());

	// Remove the service.
	CWinScm winScm(_winServiceState->getServiceName());
	winScm.deleteService();
}

void CWinServiceInstance::usage(
		const std::string& serviceName) const {
	CAF_CM_FUNCNAME("usage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	::fprintf(stderr, "usage: %s [/Service] | [-Service] |\n"
		"\t[/RegServer] | [-RegServer]\n"
		"\tRegister service\n\n", serviceName.c_str());
	::fprintf(stderr, "usage: %s [/UnregService] | [-UnregService] |\n"
		"\t[/UnregServer] | [-UnregServer]\n"
		"\tUnregister service\n\n", serviceName.c_str());
	::fprintf(stderr, "usage: %s -n\n"
		"\t[-n]\tRun in console mode\n", serviceName.c_str());
}
