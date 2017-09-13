/*
 *	 Author: bwilliams
 *  Created: June 25, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CWinService.h"
#include <process.h>

bool CWinService::s_isInitialized = false;
SmartPtrCWinServiceState CWinService::s_winServiceState;
SmartPtrCWinServiceInstance CWinService::s_winServiceInstance;

////////////////////////////////////////////////////////////////////////
//
// controlHandler()
//
// Handler for the break event when running in console mode
//
////////////////////////////////////////////////////////////////////////
BOOL WINAPI controlHandler(
	DWORD dwCtrlType) {
	CAF_CM_STATIC_FUNC_LOG("CWinService", "controlHandler");

	try {
		switch(dwCtrlType) {
			case CTRL_BREAK_EVENT:  // use Ctrl+C or Ctrl+Break to simulate
			case CTRL_C_EVENT:      // SERVICE_CONTROL_STOP in debug mode
				CAF_CM_LOG_DEBUG_VA0("Received Ctrl+C or Ctrl+Break... Stopping the service");
				CWinService::s_winServiceState->signalServiceStop();
			return TRUE;
			break;
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	const BOOL rc = (CAF_CM_ISEXCEPTION) ? FALSE : TRUE;
	CAF_CM_CLEAREXCEPTION;

	return rc;
}

uint32 __stdcall Caf::serviceWorkerThreadFunc(
	void* pThreadContext) {
	CAF_CM_STATIC_FUNC_LOG("CWinService", "serviceWorkerThreadFunc");

	try {
		// Bring the thread into the correct scope
		CWinService::s_winServiceInstance->runWorkerThread();
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	const uint32 rc = (CAF_CM_ISEXCEPTION) ? -1 : 0;
	CAF_CM_CLEAREXCEPTION;

	return rc;
}

void CWinService::initialize(
	const SmartPtrIWork& work) {
	CAF_CM_STATIC_FUNC_LOG("CWinService", "initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(s_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(work);

		s_winServiceState.CreateInstance();
		s_winServiceState->initialize(
				"VMwareCAFCommAmqpListener",
				"VMware CAF AMQP Communication Service",
				"VMware Common Agent AMQP Communication Service",
				work);

		s_winServiceInstance.CreateInstance();
		s_winServiceInstance->initialize(s_winServiceState);
		s_isInitialized = true;
	}
	CAF_CM_EXIT;
}

void CWinService::execute(
	int32 argc,
	char** argv) {
	CAF_CM_STATIC_FUNC_LOG("CWinService", "execute");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(s_isInitialized);

		// Figure out what to do based on the optional command line argument
		const CWinServiceInstance::SCommandLineParams commandLineParams =
			s_winServiceInstance->processCommandLine(argc, argv);
		switch (commandLineParams._eMode) {
			case CWinServiceInstance::EModeRunAsService:
				CAF_CM_LOG_DEBUG_VA0("Running the service");
				run();
			break;

			case CWinServiceInstance::EModeRunAsConsole:
				CAF_CM_LOG_DEBUG_VA0("Running as console");
				runAsConsole();
			break;

			case CWinServiceInstance::EModeRegister: {
				CAF_CM_LOG_DEBUG_VA0("Installing the service");
				const std::string currentFile = FileSystemUtils::getCurrentFile();
				s_winServiceInstance->install(currentFile);
			}
			break;

			case CWinServiceInstance::EModeUnregister:
				CAF_CM_LOG_DEBUG_VA0("Uninstalling the service");
				s_winServiceInstance->uninstall();
			break;

			default:
				std::string cmdLine;
				for (int32 index = 0; index < argc; index++) {
					cmdLine += argv[index];
					cmdLine += " ";
				}
				CAF_CM_EXCEPTIONEX_VA2(
					InvalidHandleException, E_FAIL,
					"Invalid mode returned from processCommandLine - Mode: %d, cmdLine: %s",
					static_cast<DWORD>(commandLineParams._eMode), cmdLine.c_str());
			break;
		}
	}
	CAF_CM_EXIT;
}

void CWinService::run() {
	CAF_CM_STATIC_FUNC_LOG("CWinService", "run");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(s_isInitialized);

		const std::string serviceName = s_winServiceState->getServiceName();

		// build the table of Services in this exe, name and the address of the
		// Service's main function terminated by NULL's
		SERVICE_TABLE_ENTRYA stbl[] =
		{
			{
				const_cast<char*>(serviceName.c_str()),
				reinterpret_cast<LPSERVICE_MAIN_FUNCTIONA>(CWinService::serviceMain)
			},
			{
				NULL,
				NULL
			}
		};

		// call the SCM.  We have 30 seconds to execute this call.  If it fails to
		// execute within that time, the SCM will assume that something is wrong
		// and terminate this process...
		if (::StartServiceCtrlDispatcherA(stbl) == FALSE) {
			const DWORD lastError = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
			CAF_CM_EXCEPTION_VA2(lastError, "::StartServiceCtrlDispatcher() Failed - serviceName: \"%s\", msg: \"%s\"",
				serviceName.c_str(), errorMsg.c_str());
		}
	}
	CAF_CM_EXIT;
}

void CWinService::runAsConsole() {
	CAF_CM_STATIC_FUNC_LOG("CWinService", "runAsConsole");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(s_isInitialized);

		::SetConsoleCtrlHandler(controlHandler, TRUE);

		CWinService::s_winServiceState->putIsService(false);
		CWinService::serviceMain(0, NULL);
	}
	CAF_CM_EXIT;
}

void CALLBACK CWinService::serviceMain(
	const DWORD cdwArgc,
	LPCWSTR* plpwszArgv) {
	CAF_CM_STATIC_FUNC_LOG("CWinService", "serviceMain");

	try {
		// This is a new thread from the SCM
		CafInitialize::init();

		CAF_CM_PRECOND_ISINITIALIZED(s_isInitialized);

		const std::string serviceName = s_winServiceState->getServiceName();

		// Register our handler routine with the SCM.
		// The function that we are registering will be called when certain
		// events occur like : starting, stopping, pausing this Service
		if (s_winServiceState->getIsService()) {
			const SERVICE_STATUS_HANDLE serviceHandle = ::RegisterServiceCtrlHandlerExA(
				serviceName.c_str(), CWinService::scmHandlerEx, NULL);
			if (! serviceHandle) 	{
				const DWORD lastError = ::GetLastError();
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
				CAF_CM_EXCEPTION_VA2(lastError, "::RegisterServiceCtrlHandlerEx() Failed - serviceName: \"%s\", msg: \"%s\"",
					serviceName.c_str(), errorMsg.c_str());
			}

			s_winServiceState->putServiceHandle(serviceHandle);
		}

		// Let the SCM know that the service is starting.
		s_winServiceState->putCurrentServiceState(SERVICE_START_PENDING);
		if (s_winServiceState->getIsService()) {
			s_winServiceState->setStatus();
		}

		// Creates the worker thread.
		createWorkerThread();

		// Runs the Service
		s_winServiceInstance->runService();

		// Let the SCM know that we've stopped.
		s_winServiceState->putCurrentServiceState(SERVICE_STOPPED);
		if (s_winServiceState->getIsService()) {
			s_winServiceState->setStatus();
		}

		// Close all of the handles.
		s_winServiceState->close();
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	CafInitialize::term();
}

DWORD CWinService::scmHandlerEx(
	DWORD dwCommand,
	DWORD dwEventType,
	LPVOID lpEventData,
	LPVOID lpContext) {
	CAF_CM_STATIC_FUNC_LOG("CWinService", "scmHandlerEx");

	try {
		CAF_CM_PRECOND_ISINITIALIZED(s_isInitialized);
		
		switch(dwCommand) {
			case SERVICE_CONTROL_STOP:
			case SERVICE_CONTROL_SHUTDOWN:
			case SERVICE_CONTROL_POWEREVENT: {
				CAF_CM_LOG_INFO_VA0("Caught stop, shutdown or power event");

				// Let the SCM know that we're in the process of stopping.
				s_winServiceState->putCurrentServiceState(SERVICE_STOP_PENDING);
				s_winServiceState->setStatus();

				// Signal the event that tells us to stop.
				s_winServiceState->signalServiceStop();
			}
			break;

			case SERVICE_CONTROL_INTERROGATE: {
				CAF_CM_LOG_INFO_VA0("Caught interrogate event");

				// Let the SCM know our current state.
				s_winServiceState->setStatus();
			}
			break;
			default:
				CAF_CM_LOG_WARN_VA1("Unhandled command - %d", dwCommand);
			break;
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	return NO_ERROR;
}

void CWinService::createWorkerThread() {
	CAF_CM_STATIC_FUNC_LOG("CWinService", "createWorkerThread");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(s_isInitialized);

		// Create the thread.
		uint32 uiThreadAddress = 0;
		UINT_PTR uipRc = ::_beginthreadex(NULL, 0, ::serviceWorkerThreadFunc, NULL, 0, &uiThreadAddress);
		if (0 == uipRc) {
			const DWORD lastError = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
			CAF_CM_EXCEPTION_VA1(lastError, "::_beginthreadex() Failed - msg: \"%s\"",
				errorMsg.c_str());
		}
	}
	CAF_CM_EXIT;
}
