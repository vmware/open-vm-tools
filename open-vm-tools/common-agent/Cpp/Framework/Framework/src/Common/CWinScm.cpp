/*
 *  Author: bwilliams
 *  Created: June 25, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CWinScm.h"

// Set the default values for how many times to retry and how int32 to wait
// between retries when starting and stopping the service.
const DWORD CWinScm::s_stopRetryMax = 30;
const DWORD CWinScm::s_stopRetryIntervalMillisecs = 1000;

const DWORD CWinScm::s_startPollMax = 30;
const DWORD CWinScm::s_startPollIntervalMillisecs = 1000;
const DWORD CWinScm::s_startRetryMax = 1;
const DWORD CWinScm::s_startRetryIntervalMillisecs = 5000;

CWinScm::CWinScm() :
	CAF_CM_INIT_LOG("CWinScm"),
	_isInitialized(false),
	_hSCM(NULL),
	_hService(NULL) {
}

CWinScm::CWinScm(const std::string& serviceName) :
	CAF_CM_INIT_LOG("CWinScm"),
	_isInitialized(false),
	_hSCM(NULL),
	_hService(NULL) {
	CAF_CM_FUNCNAME("CWinScm");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(serviceName);

		_serviceName = serviceName;

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

CWinScm::~CWinScm() {
	CAF_CM_FUNCNAME("~CWinScm");

	CAF_CM_ENTER {
		closeHandle(_hService);
		closeHandle(_hSCM);
	}
	CAF_CM_EXIT;
}

void CWinScm::initialize(
	const std::string& serviceName,
	const std::string& machineName) {
	CAF_CM_FUNCNAME("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(serviceName);
		// machineName is optional

		_serviceName = serviceName;

		// If a machine name was provided, then store it in UNC format (e.g. \\MachineName).
		if (! machineName.empty()) {
			_machineName = std::string("\\") + machineName;
		}

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

void CWinScm::attachScmRequired(
	const DWORD desiredAccess/*  = SC_MANAGER_ALL_ACCESS*/) {
	CAF_CM_FUNCNAME("attachScmRequired");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		attachScm(desiredAccess, true);
	}
	CAF_CM_EXIT;
}

void CWinScm::attachScmOptional(
	const DWORD desiredAccess/*  = SC_MANAGER_ALL_ACCESS*/) {
	CAF_CM_FUNCNAME("attachScmOptional");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		attachScm(desiredAccess, false);
	}
	CAF_CM_EXIT;
}

void CWinScm::createService(
	const std::string& serviceFilename,
	const DWORD startType/* = SERVICE_DEMAND_START*/,
	const CvecDependencies cvecDependencies/* = CvecDependencies()*/) {
	CAF_CM_FUNCNAME("createService");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(serviceFilename);

		createService(
			serviceFilename,
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			startType,
			cvecDependencies);
	}
	CAF_CM_EXIT;
}	

void CWinScm::createService(
	const std::string& serviceFilename,
	const std::string& serviceDisplayName,
	const std::string& serviceDescription,
	const std::string& serviceAccountName,
	const std::string& serviceAccountPasswd,
	const DWORD startType,
	const CvecDependencies cvecDependencies) {
	CAF_CM_FUNCNAME("createService");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(serviceFilename);
		// serviceDisplayName is optional
		// serviceDescription is optional
		// serviceAccountName is optional
		// serviceAccountPasswd is optional
		// startType is optional
		// cvecDependencies is optional

		// Make sure the previous service (if any) is closed.
		closeHandle(_hService);

		// Open the Service Control Manager.
		attachScm();

		// Create and initialize the string that will hold the list of dependenciesWide.
		const DWORD dependenciesLen = 2048;
		wchar_t dependenciesWide[dependenciesLen + 1];

		// Iterate through the dependenciesWide and stick them in the string.
		size_t dependencyIndex = 0;
		CvecDependencies::const_iterator civecDependencies;
		for (civecDependencies = cvecDependencies.begin();
			civecDependencies != cvecDependencies.end();
			civecDependencies++) {
			const std::string dependency = *civecDependencies;
			const std::wstring dependencyWide = CStringUtils::convertNarrowToWide(dependency);

			// Hang onto the length of this dependency.
			const size_t dependencyLen = dependencyWide.length();

			// Make sure the length of the dependenciesWide array won't be exceeded.
			if ((dependencyIndex + dependencyLen + 1) >= dependenciesLen) {
				CAF_CM_EXCEPTIONEX_VA1(
					NoSuchElementException, ERROR_NO_MORE_ITEMS,
					"Exceeded dependency length - %d", dependencyIndex);
			}

			// Copy the current dependency into the string.
			::wcsncpy_s(&dependenciesWide[dependencyIndex], dependenciesLen, dependencyWide.c_str(), dependencyLen);
			dependencyIndex += dependencyLen;

			// Null-terminate it.
			dependenciesWide[dependencyIndex] = L'\0';
			dependencyIndex++;
		}

		// Make sure the length of the dependenciesWide array won't be exceeded.
		if ((dependencyIndex + 1) >= dependenciesLen) {
			CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException, ERROR_NO_MORE_ITEMS,
				"Exceeded dependency length - %d", dependencyIndex);
		}

		// The dependency string is terminated by two NULLs.
		dependenciesWide[dependencyIndex] = L'\0';

		// Other config info
		std::wstring displayNameWide;
		if (! serviceDisplayName.empty()) {
			displayNameWide = CStringUtils::convertNarrowToWide(serviceDisplayName);
		} else {
			displayNameWide = CStringUtils::convertNarrowToWide(_serviceName);
		}

		std::wstring accountWide;
		std::wstring passwdWide;
		if (!serviceAccountName.empty()) {
			CAF_CM_VALIDATE_STRING(serviceAccountPasswd);
			const std::string account = ".\\" + serviceAccountName;
			accountWide = CStringUtils::convertNarrowToWide(account);
			passwdWide = CStringUtils::convertNarrowToWide(serviceAccountPasswd);
		}

		const wchar_t* accountWidePtr = accountWide.empty() ? NULL : accountWide.c_str();
		const wchar_t* passwdWidePtr = passwdWide.empty() ? NULL : passwdWide.c_str();

		const std::wstring serviceFilenameWide = CStringUtils::convertNarrowToWide(serviceFilename);

		// Open the service so that its OK for the service to be missing.
		if (openService(false)) {
			// Change the configuration information.
			BOOL bRc = ::ChangeServiceConfig(
				_hService,					// handle to the service
				SERVICE_WIN32_OWN_PROCESS,	// Only one Service in this process
				startType,					// Start type: AUTO, BOOT, DEMAND, DISABLED, START
				SERVICE_ERROR_NORMAL,		// Error type (NORMAL)
				serviceFilenameWide.c_str(),// Service's binary location
				NULL,						// no load ordering grouping
				NULL,						// no tag identifier
				dependenciesWide,			// this list of dependenciesWide
				accountWidePtr,				// account
				passwdWidePtr,				// password
				displayNameWide.c_str());	// Display name

			// If the call failed, throw an exception.
			if (0 == bRc) {
				const DWORD lastError = ::GetLastError();
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
				CAF_CM_EXCEPTION_VA3(lastError, "::ChangeServiceConfig() Failed - serviceName: %s, serviceFilename: %s, msg: \"%s\"",
					_serviceName.c_str(), serviceFilename.c_str(), errorMsg.c_str());
			}
		} else {
			const std::wstring serviceNameWide = CStringUtils::convertNarrowToWide(_serviceName);

			// Create the Service
			_hService = ::CreateService(
				_hSCM,						// handle to the SCM
				serviceNameWide.c_str(),	// Binary name of the Service
				displayNameWide.c_str(),	// The name to be displayed
				SERVICE_ALL_ACCESS,			// The desired Access
				SERVICE_WIN32_OWN_PROCESS,	// Only one Service in this process
				startType,					// Start type (AUTO, or DISABLED)
				SERVICE_ERROR_NORMAL,		// Error type (NORMAL)
				serviceFilenameWide.c_str(),// Service's binary location
				NULL,						// no load ordering grouping
				NULL,						// no tag identifier
				dependenciesWide,			// this list of dependencies
				accountWidePtr,				// account
				passwdWidePtr);				// password

			// If the call failed, throw an exception.
			if (! _hService) {
				const DWORD lastError = ::GetLastError();
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
				CAF_CM_EXCEPTION_VA3(lastError, "::CreateService() Failed - serviceName: %s, serviceFilename: %s, msg: \"%s\"",
					_serviceName.c_str(), serviceFilename.c_str(), errorMsg.c_str());
			}
		}

		// Set the description
		std::wstring wsDescription(serviceDescription.empty() ? L"" : CStringUtils::convertNarrowToWide(serviceDescription));

		SERVICE_DESCRIPTION stDescription;
		stDescription.lpDescription = const_cast<LPWSTR>(wsDescription.c_str());

		const BOOL bRc = ::ChangeServiceConfig2(_hService, SERVICE_CONFIG_DESCRIPTION, &stDescription);
		if (bRc == 0) {
			const DWORD lastError = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
			CAF_CM_EXCEPTION_VA3(lastError, "::ChangeServiceConfig2() Failed - serviceName: %s, serviceFilename: %s, msg: \"%s\"",
				_serviceName.c_str(), serviceFilename.c_str(), errorMsg.c_str());
		}

		// Make sure the service can be opened.
		openService(true);
	}
	CAF_CM_EXIT;
}	

///////////////////////////////////////////////////////////////////////////////
//
//  CWinScm::changeService()
//
//  Modifies the Service with only the non-empty elements passed in
//  NOTE: If you want to change other service settings, use CreateService
//        instead; it will also modify an existing service
//
///////////////////////////////////////////////////////////////////////////////
void CWinScm::changeService(
	const std::string& serviceFilename,
	const std::string& serviceDisplayName,
	const std::string& serviceDescription,
	const std::string& serviceAccountName,
	const std::string& serviceAccountPasswd,
	const DWORD startupType) {
	CAF_CM_FUNCNAME("changeService");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Open the Sevice Control Manager.
		attachScm();

		// Open the service
		openService(true);

		// Other config info
		std::wstring binaryPathNameWide;
		if (!serviceFilename.empty()) {
			binaryPathNameWide = CStringUtils::convertNarrowToWide(serviceFilename);
		}

		std::wstring displayNameWide = NULL;
		if (!serviceDisplayName.empty()) {
			displayNameWide = CStringUtils::convertNarrowToWide(serviceDisplayName);
		}

		std::wstring accountWide;
		std::wstring passwdWide;
		if (!serviceAccountName.empty()) {
			CAF_CM_VALIDATE_STRING(serviceAccountPasswd);
			const std::string account = ".\\" + serviceAccountName;
			accountWide = CStringUtils::convertNarrowToWide(account);
			passwdWide = CStringUtils::convertNarrowToWide(serviceAccountPasswd);
		}

		const wchar_t* accountWidePtr = accountWide.empty() ? NULL : accountWide.c_str();
		const wchar_t* passwdWidePtr = passwdWide.empty() ? NULL : passwdWide.c_str();

		DWORD startType = SERVICE_NO_CHANGE;
		if (SERVICE_NO_CHANGE != startupType) {
			switch(startupType) {
				case SERVICE_BOOT_START:
				case SERVICE_SYSTEM_START:
				case SERVICE_AUTO_START:
				case SERVICE_DEMAND_START:
				case SERVICE_DISABLED:
					startType = startupType;
					break;
				default:
					CAF_CM_LOG_WARN_VA1("Unrecognized value for service startup type (using anyway) - startupType: %d", startupType);
					startType = startupType;
					break;
			}
		}

		// Change the configuration information.
		BOOL bRc = ::ChangeServiceConfig(
			_hService,					// handle to the service
			SERVICE_NO_CHANGE,			// Only one Service in this process
			startType,					// Start type: AUTO, BOOT, DEMAND, DISABLED, START
			SERVICE_NO_CHANGE,			// Error type (NORMAL)
			binaryPathNameWide.c_str(),	// Service's binary location
			NULL,						// no load ordering grouping
			NULL,						// no tag identifier
			NULL,           			// this list of dependencies
			accountWidePtr,				// account
			passwdWidePtr,				// password
			displayNameWide.c_str());	// Display name

		// If the call failed, throw an exception.
		if (0 == bRc) {
			const DWORD lastError = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
			CAF_CM_EXCEPTION_VA3(lastError, "::ChangeServiceConfig() Failed - serviceName: %s, binaryPathNameWide: %s, msg: \"%s\"",
				_serviceName.c_str(), binaryPathNameWide.c_str(), errorMsg.c_str());
		}

		if (!serviceDescription.empty()) {
			// Set the description
			SERVICE_DESCRIPTION stDescription;
			stDescription.lpDescription = const_cast<LPWSTR>(
				CStringUtils::convertNarrowToWide(serviceDescription).c_str());

			// Check to see if this function is supported on this platform, if so we can use it.
			BOOL bRc = ::ChangeServiceConfig2(_hService, SERVICE_CONFIG_DESCRIPTION, &stDescription);
			if (!bRc) {
				const DWORD lastError = ::GetLastError();
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
				CAF_CM_EXCEPTION_VA3(lastError, "::ChangeServiceConfig2() Failed - serviceName: %s, binaryPathNameWide: %s, msg: \"%s\"",
					_serviceName.c_str(), binaryPathNameWide.c_str(), errorMsg.c_str());
			}
		}

		// Make sure the service can be opened.
		openService(true);
	}
	CAF_CM_EXIT;
}

///////////////////////////////////////////////////////////////////////////////
//
//	CWinScm::deleteService()
//
//	Delete this Service
//
///////////////////////////////////////////////////////////////////////////////
void CWinScm::deleteService(
	const DWORD stopRetryMax/* = s_stopRetryMax*/,
	const DWORD stopRetryIntervalMillisecs/* = s_stopRetryIntervalMillisecs*/,
	const DWORD servicePid/* = 0*/) {
	CAF_CM_FUNCNAME("deleteService");
	
	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Open the service so that its OK for the service to be missing.
		if (openService(false)) {
			// Stop the service.
			stopService(stopRetryMax, stopRetryIntervalMillisecs, servicePid);

			// Delete the service.
			if (! ::DeleteService(_hService)) {
				const DWORD cdwError = ::GetLastError();

				// If the service has already been marked for deletion, then this is a no-op.
				if (ERROR_SERVICE_MARKED_FOR_DELETE == cdwError) {
					CAF_CM_LOG_WARN_VA1("Already marked for deletion - serviceName: %s", _serviceName.c_str());
				} else {
					const DWORD lastError = ::GetLastError();
					const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
					CAF_CM_EXCEPTION_VA2(lastError, "::DeleteService() Failed - serviceName: %s, msg: \"%s\"",
						_serviceName.c_str(), errorMsg.c_str());
				}
			} else {
				closeHandle(_hService);
			}
		}
	}
	CAF_CM_EXIT;
}

///////////////////////////////////////////////////////////////////////////////
//
//  CWinScm::changeServiceRecoveryProperties()
//
//  Modifies the Service Recovery Options
//  NOTE: If you want to change other service settings, use CreateService
//        instead; it will also modify an existing service
//
// serviceFilename - service name
// firstFailureAction,
//	secondFailureAction,
//	subsequentFailureAction
//		members of enum _SC_ACTION_TYPE
//	      SC_ACTION_NONE          = 0,
//        SC_ACTION_RESTART       = 1,
//        SC_ACTION_REBOOT        = 2,
//        SC_ACTION_RUN_COMMAND   = 3
// ** I am seeing "access denied" when SC_ACTION_REBOOT is included
// ** If all three are SC_ACTION_NONE, the reset period is deleted
// cresetFailureCountAfter_days - failure count is reset to 0 after this many days
//		-1 is INFINITE.  This shows as infinite using "sc qfailure" but as 49710 in SCM
// crestartServiceAfter_minutes - for action SC_ACTION_RESTART, service will be started after this period
// clpstrCommandLineToRun - for action SC_ACTION_RUN_COMMAND, this command line will be passed to CreateProcess
//		If this is null, the command will be unchanged
//		If "", the command will be cleared
// crebootComputerAfter_minutes - for action SC_ACTION_REBOOT, the computer will reboot after this period
// clpstrRestartMessage - for action SC_ACTION_REBOOT, this message will be sent to all sessions prior to reboot period
//		If this is null, the message will be unchanged
//		If "", the message will be cleared
//
///////////////////////////////////////////////////////////////////////////////
void CWinScm::changeServiceRecoveryProperties(
	const std::string& serviceFilename,
	const DWORD firstFailureAction,
	const DWORD secondFailureAction,
	const DWORD subsequentFailureAction,
	const DWORD cresetFailureCountAfter_days,
	const DWORD crestartServiceAfter_minutes,
	const LPCWSTR clpstrCommandLineToRun,
	const DWORD crebootComputerAfter_minutes,
	const LPCWSTR clpstrRestartMessage) {
	CAF_CM_FUNCNAME("changeServiceRecoveryProperties");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Open the Sevice Control Manager.
		attachScm();

		// Open the service
		openService(true);

		// Change the configuration information.
		SERVICE_FAILURE_ACTIONS stFailureActions;

		// reset failure count after...
		// The length of time, in seconds, after which to reset the failure count to zero if there are no failures. Specify INFINITE to indicate that this value should never be reset. 
		if (cresetFailureCountAfter_days == INFINITE) {
			stFailureActions.dwResetPeriod = INFINITE;
		} else {
			// convert to seconds = * 24 hours/day * 60 minutes/hour * 60 seconds/minute
			stFailureActions.dwResetPeriod = cresetFailureCountAfter_days * 24 * 60 * 60;
		}

		// Reboot message
		// Message to broadcast to server users before rebooting in response to the SC_ACTION_REBOOT service controller action. 
		// If this value is NULL, the reboot message is unchanged. If the value is an empty string (""), the reboot message is deleted and no message is broadcast. 
		//
		stFailureActions.lpRebootMsg = const_cast<wchar_t*>(clpstrRestartMessage);

		// Command to run
		// Command line of the process for the CreateProcess function to execute in response to the SC_ACTION_RUN_COMMAND service controller action. This process runs under the same account as the service. 
		// If this value is NULL, the command is unchanged. If the value is an empty string (""), the command is deleted and no program is run when the service fails. 
		stFailureActions.lpCommand = const_cast<wchar_t*>(clpstrCommandLineToRun);

		// actions
		// cActions
		// Number of elements in the lpsaActions array. 
		// If this value is 0, but lpsaActions is not NULL, the reset period and array of failure actions are deleted. 
		//
		// lpsaActions 
		// Pointer to an array of SC_ACTION structures. 
		// If this value is NULL, the cActions and resetPeriod members are ignored.
		SC_ACTION aAction[3];
		if ((firstFailureAction != SC_ACTION_NONE) ||
			(secondFailureAction != SC_ACTION_NONE) ||
			(subsequentFailureAction != SC_ACTION_NONE)) {
			stFailureActions.cActions = 3;
			stFailureActions.lpsaActions = aAction;
			aAction[0].Type = SC_ACTION_TYPE(firstFailureAction);
			aAction[1].Type = SC_ACTION_TYPE(secondFailureAction);
			aAction[2].Type = SC_ACTION_TYPE(subsequentFailureAction);

			for (int32 i=0; i< 3; i++) {
				switch(aAction[i].Type) {
				case SC_ACTION_NONE:
					aAction[i].Delay = 0;
					break;
				case SC_ACTION_REBOOT:
					// convert minutes to milliseconds
					aAction[i].Delay = crebootComputerAfter_minutes * 60 * 1000;
					break;
				case SC_ACTION_RESTART:
					aAction[i].Delay = crestartServiceAfter_minutes * 60 * 1000;
					break;
				case SC_ACTION_RUN_COMMAND:
					// reuse the restart service parameter
					aAction[i].Delay = crestartServiceAfter_minutes * 60 * 1000;
					break;
				default:
					aAction[i].Type = SC_ACTION_NONE;
					aAction[i].Delay = 0;
					break;
				}
			}
		} else {
			// All actions are none - clear the actions
			stFailureActions.cActions = 0;
			stFailureActions.lpsaActions = aAction;
		}

		// make the requested changes
		// Check to see if this function is supported on this platform, if so we can use it.
		BOOL bRc = ::ChangeServiceConfig2(_hService, SERVICE_CONFIG_FAILURE_ACTIONS, &stFailureActions);
		if (!bRc) {
			const DWORD lastError = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
			CAF_CM_EXCEPTION_VA2(lastError, "::ChangeServiceConfig2() Failed - serviceName: %s, msg: \"%s\"",
				_serviceName.c_str(), errorMsg.c_str());
		}

		// Make sure the service can be opened.
		openService(true);
	}
	CAF_CM_EXIT;
}

///////////////////////////////////////////////////////////////////////////////
//
//	CWinScm::startService()
//
//	Starts the Service
//
///////////////////////////////////////////////////////////////////////////////
SERVICE_STATUS CWinScm::startService(
	const DWORD startPollMax/* = s_startPollMax*/,
	const DWORD startPollIntervalMillisecs/* = s_startPollIntervalMillisecs*/,
	const DWORD startRetryMax/* = s_startRetryMax*/,
	const DWORD startRetryIntervalMillisecs/* = s_startRetryIntervalMillisecs*/) {
	CAF_CM_FUNCNAME("startService");

	SERVICE_STATUS stServiceStatus;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Initialize the structure.
		::memset(&stServiceStatus, 0, sizeof(stServiceStatus));

		// Enter a loop that tries to restart the service.
		bool isFirstTime = true;
		DWORD retryCnt = 0;
		for (retryCnt = 0; (0 == startRetryMax) || (retryCnt < startRetryMax); retryCnt++) {
			// Don't Sleep the first time through the loop.
			if (isFirstTime) {
				isFirstTime = false;
			} else {
				::Sleep(startRetryIntervalMillisecs);
			}

			// Attempt to start the service.
			CAF_CM_LOG_INFO_VA2("Attempting to start service - serviceName: %s, retryCnt: %d", _serviceName.c_str(), retryCnt);
			stServiceStatus = startServiceInternal(startPollMax, startPollIntervalMillisecs);

			// If the service is started, then exit the loop.
			if ((startPollMax == 0) || (stServiceStatus.dwCurrentState == SERVICE_RUNNING)) {
				break;
			}

			CAF_CM_LOG_ERROR_VA2("Failed to start service - serviceName: %s, retryCnt: %d", _serviceName.c_str(), retryCnt);
		}

		// Is the service running?
		if ((startPollMax > 0) && (stServiceStatus.dwCurrentState != SERVICE_RUNNING)) {
			CAF_CM_EXCEPTIONEX_VA2(
				IllegalStateException, E_FAIL,
				"Unable to start the service - serviceName: %s, retryCnt: %d", _serviceName.c_str(), retryCnt);
		}

		if (stServiceStatus.dwCurrentState == SERVICE_RUNNING) {
			CAF_CM_LOG_INFO_VA2("Successfully started the service - serviceName: %s, retryCnt: %d", _serviceName.c_str(), retryCnt);
		}
	}
	CAF_CM_EXIT;

	return stServiceStatus;
}

///////////////////////////////////////////////////////////////////////////////
//
//	CWinScm::stopService()
//
//	Stops the Service
//
///////////////////////////////////////////////////////////////////////////////
SERVICE_STATUS CWinScm::stopService(
	const DWORD stopRetryMax/* = s_stopRetryMax*/,
	const DWORD stopRetryIntervalMillisecs/* = s_stopRetryIntervalMillisecs*/,
	const DWORD servicePid/* = 0*/) {
	CAF_CM_FUNCNAME("stopService");

	SERVICE_STATUS stServiceStatus;
	
	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Make sure the previous service (if any) is closed.
		closeHandle(_hService);

		// Initialize the input param.
		::memset(&stServiceStatus, 0, sizeof(stServiceStatus));

		// Is the service stopped?
		if (getServiceStatus().dwCurrentState == SERVICE_STOPPED) {
			CAF_CM_LOG_WARN_VA1("The service is already stopped - serviceName: %s", _serviceName.c_str());
		} else {
			HANDLE serviceProcessHandle = NULL;

			// If the service PID is valid and this is the "local" machine, then open the process just
			// in case it needs to be terminated later.
			if ((servicePid > 0) && _machineName.empty()) {
				// Try to set the debug privelege to ensure that we can kill the process.
				grantPrivilege(SE_DEBUG_NAME);

				serviceProcessHandle = ::OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, servicePid);
				if (! serviceProcessHandle) {
					const DWORD lastError = ::GetLastError();
					CAF_CM_LOG_ERROR_VA2("::OpenProcess() Failed - serviceName: %s, servicePid: %d", _serviceName.c_str(), servicePid);
				}
			}

			// if there are any services that depend on us, stop them first
//			CdeqDependentOnMe deqDeps;
//			getDependentServices(deqDeps);
			ERc eRcDependentsStopped = ERcSucceeded;
//			if (!deqDeps.empty())
//			{
//				eRcDependentsStopped = stopDependentServices(deqDeps, s_stopRetryMax, s_stopRetryIntervalMillisecs);
//			}

			// stop our service only if we are able to stop all services dependent upon us
			if (eRcDependentsStopped == ERcSucceeded) {
				// Tell the service to stop.
				if (getServiceStatus().dwCurrentState != SERVICE_STOP_PENDING) {
					controlService(SERVICE_CONTROL_STOP);
				}

				// Wait for the service to stop.
				for (DWORD stopCnt = 0; stopCnt < stopRetryMax; stopCnt++) {
					CAF_CM_LOG_INFO_VA2("Waiting for service to stop - serviceName: %s, stopCnt: %d", _serviceName.c_str(), stopCnt);

					// Is the service stopped?
					if (getServiceStatus().dwCurrentState == SERVICE_STOPPED)
						break;

					// Give the Service time to stop
					::Sleep(stopRetryIntervalMillisecs);
				}

				// If the handle is valid then I'm supposed to kill the process if it's still alive.
				if (serviceProcessHandle) {
					// Get the exit code of the process to determine whether or not it's still alive.
					DWORD exitCode = 0;
					if (0 == ::GetExitCodeProcess(serviceProcessHandle, &exitCode)) {
						const DWORD lastError = ::GetLastError();
						CAF_CM_LOG_ERROR_VA2("::GetExitCodeProcess() Failed - serviceName: %s, servicePid: %d",
							_serviceName.c_str(), servicePid);
					} else {
						// If the process is still alive, then terminate it.
						if (STILL_ACTIVE == exitCode) {
							// Terminate the process.
							if (0 == ::TerminateProcess(serviceProcessHandle, 1)) {
								// Hang onto the error code.
								const DWORD cdwLastErrorTerminateProcess = ::GetLastError();

								// Get the exit code of the process to determine whether or not it's still alive.
								DWORD exitCode = 0;
								if (0 == ::GetExitCodeProcess(serviceProcessHandle, &exitCode)) {
									const DWORD lastError = ::GetLastError();
									CAF_CM_LOG_ERROR_VA2("::GetExitCodeProcess() Failed - serviceName: %s, servicePid: %d", _serviceName.c_str(), servicePid);
								} else {
									// If the process is still alive, then TerminateProcess clearly failed.
									if (STILL_ACTIVE == exitCode)
										CAF_CM_LOG_ERROR_VA1("::TerminateProcess() Failed - serviceName: %s", _serviceName.c_str());
								}
							}
						}
					}
				}
			}
		}

		stServiceStatus = getServiceStatus();
		if (stServiceStatus.dwCurrentState != SERVICE_STOPPED) {
			CAF_CM_LOG_WARN_VA1("Failed to stop service - serviceName: %s", _serviceName.c_str());
		}

	}
	CAF_CM_EXIT;

	return stServiceStatus;
}

///////////////////////////////////////////////////////////////////////////////
//
//	CWinScm::getServiceStatus()
//
//	Gets the status of this service.
//
///////////////////////////////////////////////////////////////////////////////
SERVICE_STATUS CWinScm::getServiceStatus(
	const bool isExceptionOnMissingService/* = true*/) {
	CAF_CM_FUNCNAME("getServiceStatus");

	SERVICE_STATUS stServiceStatus;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Initialize the input param.
		::memset(&stServiceStatus, 0, sizeof(stServiceStatus));

		// Try to open the service.  If it's possible to open it, then get the service status.  Otherwise,
		// the service isn't there and the state is set to stopped.
		if (openService(isExceptionOnMissingService, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) {
			// Get the state of the service.
			if (0 == ::QueryServiceStatus(_hService, &stServiceStatus)) {
				const DWORD lastError = ::GetLastError();
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
				CAF_CM_EXCEPTION_VA2(lastError, "::QueryServiceStatus() Failed - serviceName: %s, msg: \"%s\"",
					_serviceName.c_str(), errorMsg.c_str());
			}
		} else {
			stServiceStatus.dwCurrentState = SERVICE_STOPPED;
		}
	}
	CAF_CM_EXIT;

	return stServiceStatus;
}

///////////////////////////////////////////////////////////////////////////////
//
//	CWinScm::controlService()
//
//	Controls the Service
//
///////////////////////////////////////////////////////////////////////////////
SERVICE_STATUS CWinScm::controlService(
	const DWORD cdwCommand) {
	CAF_CM_FUNCNAME("controlService");

	SERVICE_STATUS stServiceStatus;
	
	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Initialize the input param.
		::memset(&stServiceStatus, 0, sizeof(stServiceStatus));

		// Open the service.
		openService(true);

		if (0 == ::ControlService(_hService, cdwCommand, &stServiceStatus)) {
			const DWORD cdwError = ::GetLastError();
			CAF_CM_LOG_WARN_VA1("::ControlService() Failed - serviceName: %s", _serviceName.c_str());

			stServiceStatus = getServiceStatus();
		}
	}
	CAF_CM_EXIT;

	return stServiceStatus;
}

///////////////////////////////////////////////////////////////////////////////
//
//  CEcmColService::setStatus()
//
//  Wrapper function to implement API call SetServiceStatus
//
///////////////////////////////////////////////////////////////////////////////
void CWinScm::setStatus(
	const SERVICE_STATUS_HANDLE chSrv,
	const DWORD state,
	const DWORD exitCode,
	const DWORD progress/* = 0*/,
	const DWORD waitHintMilliseconds/* = 3000 */) {
	CAF_CM_FUNCNAME("setStatus");

	CAF_CM_ENTER {
		// Declare a SERVICE_STATUS structure, and fill it in	
		SERVICE_STATUS srvStatus;

		// We're the only Service running in our current process
		srvStatus.dwServiceType  = SERVICE_WIN32_OWN_PROCESS;

		// Set the state of the Service from the argument, and save it away
		srvStatus.dwCurrentState = state;

		// Which commands sould we accept from the SCM? All the common ones...
		srvStatus.dwControlsAccepted =
			SERVICE_ACCEPT_STOP |
			SERVICE_ACCEPT_SHUTDOWN |
			SERVICE_ACCEPT_POWEREVENT;

		// Set the Win32 exit code for the Service
		srvStatus.dwWin32ExitCode = exitCode;

		// Set the Service-specific exit code
		srvStatus.dwServiceSpecificExitCode = 0;

		// Set the checkpoint value
		srvStatus.dwCheckPoint = exitCode;

		// Set the timeout for waits
		// Note: Here we are telling the SMC how int32 it should wait for the next 
		// status message. As int32 as the SCM gets the next status message before 
		// this time has elapsed, we are OK, if the SCM has not received a message
		// and the check point value (exitCode) has not been incremented, the
		// SCM will assume that something is wrong and end this Service. So be 
		// generous. 3 seconds seems to be standard but is overridable.
		srvStatus.dwWaitHint = waitHintMilliseconds;

		// Pass the structure to the SCM.
		if (! ::SetServiceStatus(chSrv, &srvStatus)) {
			const DWORD lastError = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
			CAF_CM_EXCEPTION_VA2(lastError, "::SetServiceStatus() Failed - serviceName: %s, msg: \"%s\"",
				_serviceName.c_str(), errorMsg.c_str());
		}
	}
	CAF_CM_EXIT;
}

///////////////////////////////////////////////////////////////////////////////
//
//  CWinScm::openService()
//
//  Opens the service.
//
///////////////////////////////////////////////////////////////////////////////
bool CWinScm::openService(
	const bool isExceptionOnMissingService,
	const DWORD scmDesiredAccess/* = SC_MANAGER_ALL_ACCESS*/,
	const DWORD desiredAccess/* = SERVICE_ALL_ACCESS*/) {
	CAF_CM_FUNCNAME("openService");

	bool isServiceFound = true;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		closeHandle(_hService);

		// Open the Service Control Manager.
		attachScm(scmDesiredAccess, isExceptionOnMissingService);

		// Get a handle to the Service that we want to delete
		const std::wstring serviceNameWide = CStringUtils::convertNarrowToWide(_serviceName);
		_hService = ::OpenService(
			_hSCM,					// handle to the SCM
			serviceNameWide.c_str(),// name of the Service to delete
			desiredAccess);	// desired access

		if (! _hService) {
			const DWORD lastError = ::GetLastError();

			// If the service doesn't exist, then this is a no-op.
			if (ERROR_SERVICE_DOES_NOT_EXIST == lastError) {
				if (isExceptionOnMissingService) {
					const DWORD lastError = ::GetLastError();
					const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
					CAF_CM_EXCEPTION_VA2(lastError, "::OpenService() says that the service does not exist - serviceName: %s, msg: \"%s\"",
						_serviceName.c_str(), errorMsg.c_str());
				}

				isServiceFound = false;
			} else {
				const DWORD lastError = ::GetLastError();
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
				CAF_CM_EXCEPTION_VA2(lastError, "::OpenService() Failed - serviceName: %s, msg: \"%s\"",
					_serviceName.c_str(), errorMsg.c_str());
			}
		}
	}
	CAF_CM_EXIT;

	return isServiceFound;
}

///////////////////////////////////////////////////////////////////////////////
//
//  CWinScm::getServiceConfig()
//
///////////////////////////////////////////////////////////////////////////////
CWinScm::SmartPtrSServiceConfig CWinScm::getServiceConfig(
	const bool isExceptionOnMissingService) {
	CAF_CM_FUNCNAME("getServiceConfig");

	SmartPtrSServiceConfig spcConfig;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		if (openService(isExceptionOnMissingService, SC_MANAGER_CONNECT, SERVICE_QUERY_CONFIG)) {
			// Docs say that 8K is the max so just go for it
			BYTE configBuffer[8192];
			LPQUERY_SERVICE_CONFIG pstServiceConfig = reinterpret_cast<LPQUERY_SERVICE_CONFIG>(configBuffer);

			DWORD dwBytesNeeded = 0;
			if (0 == ::QueryServiceConfig(
				_hService,
				pstServiceConfig,
				8191,
				&dwBytesNeeded)) {
				const DWORD lastError = ::GetLastError();
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
				CAF_CM_EXCEPTION_VA2(lastError, "::QueryServiceConfig() Failed - serviceName: %s, msg: \"%s\"",
					_serviceName.c_str(), errorMsg.c_str());
			}

			// Copy the data into a smart class
			spcConfig.CreateInstance();
			spcConfig->_serviceType = pstServiceConfig->dwServiceType;
			spcConfig->_startType = pstServiceConfig->dwStartType;
			spcConfig->_errorControl = pstServiceConfig->dwErrorControl;
			spcConfig->_binaryPathName = CStringUtils::convertWideToNarrow(pstServiceConfig->lpBinaryPathName);
			spcConfig->_loadOrderGroup = CStringUtils::convertWideToNarrow(pstServiceConfig->lpLoadOrderGroup);
			spcConfig->_tagId = pstServiceConfig->dwTagId;
			spcConfig->_dependencies = CStringUtils::convertWideToNarrow(pstServiceConfig->lpDependencies);
			spcConfig->_serviceStartName = CStringUtils::convertWideToNarrow(pstServiceConfig->lpServiceStartName);
			spcConfig->_displayName = CStringUtils::convertWideToNarrow(pstServiceConfig->lpDisplayName);
		}
	}
	CAF_CM_EXIT;

	return spcConfig;
}

///////////////////////////////////////////////////////////////////////////////
//
//  CWinScm::closeHandle()
//
//  Closes a handle.
//
///////////////////////////////////////////////////////////////////////////////
void CWinScm::closeHandle(
	SC_HANDLE& rhService) {
	CAF_CM_FUNCNAME("closeHandle");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		if (rhService) {
			if (0 == ::CloseServiceHandle(rhService)) {
				const DWORD cdwError = ::GetLastError();
				CAF_CM_LOG_ERROR_VA1("::CloseServiceHandle() Failed - serviceName: %s", _serviceName);
			}

			rhService = NULL;
		}
	}
	CAF_CM_EXIT;
}	

///////////////////////////////////////////////////////////////////////////////
//
//	CWinScm::startServiceInternal()
//
//	Starts the Service
//
///////////////////////////////////////////////////////////////////////////////
SERVICE_STATUS CWinScm::startServiceInternal(
	const DWORD startPollMax,
	const DWORD startPollIntervalMillisecs) {
	CAF_CM_FUNCNAME("startServiceInternal");

	SERVICE_STATUS stServiceStatus;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Make sure the previous service (if any) is closed.
		closeHandle(_hService);

		// Initialize the input param.
		::memset(&stServiceStatus, 0, sizeof(stServiceStatus));

		// Is the service stopped?
		if (getServiceStatus().dwCurrentState == SERVICE_RUNNING) {
			CAF_CM_LOG_WARN_VA1("The service is already running - serviceName: %s", _serviceName.c_str());
		} else {
			// Start the service.
			if (0 == ::StartService(_hService, 0, NULL)) {
				const DWORD lastError = ::GetLastError();
				if (ERROR_SERVICE_ALREADY_RUNNING != lastError) {
					const DWORD lastError = ::GetLastError();
					const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
					CAF_CM_EXCEPTION_VA2(lastError, "::StartService() Failed - serviceName: %s, msg: \"%s\"",
						_serviceName.c_str(), errorMsg.c_str());
				}

				CAF_CM_LOG_WARN_VA1("The service is already running - serviceName: %s", _serviceName.c_str());
			}

			// Wait for the service to start.
			for (DWORD dwPollCnt = 0; dwPollCnt < startPollMax; dwPollCnt++) {
				// Is the service running?
				if (getServiceStatus().dwCurrentState == SERVICE_RUNNING) {
					break;
				}

				// Give the Service time to start
				::Sleep(startPollIntervalMillisecs);
			}
		}

		stServiceStatus = getServiceStatus();
	}
	CAF_CM_EXIT;

	return stServiceStatus;
}

///////////////////////////////////////////////////////////////////////////////
//
//	CWinScm::getDependentServices()
//
//	Gets all the services dependent on this one
//
///////////////////////////////////////////////////////////////////////////////
void CWinScm::getDependentServices(
	CdeqDependentOnMe& rdeqDependentOnMe) {
	CAF_CM_FUNCNAME("getDependentServices");

	static const DWORD scdwNumElems = 50;
	static const DWORD scdwAddtlInfoPerElem = 100;

	ENUM_SERVICE_STATUS* serviceStatus = NULL;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Make sure the services is open.
		openService(true, SC_MANAGER_CONNECT, SERVICE_ENUMERATE_DEPENDENTS);

		// Initialize the input param.
		rdeqDependentOnMe.clear();

		// Create a buffer with a default number of elements and additional
		// info (used to contain the strings that the structure points to).
		DWORD serviceStatusBufSz = scdwNumElems * scdwAddtlInfoPerElem;
		serviceStatus = new ENUM_SERVICE_STATUS[serviceStatusBufSz + 1];

		// Loop until all of the dependent services are retrieved.
		BOOL bRet = FALSE;
		BOOL bExitLoop = FALSE;
		do {
			// Get the dependent services.
			DWORD dwBytesNeeded = 0;
			DWORD dwNumServices = 0;
			DWORD dwLastError = ERROR_SUCCESS;
			bRet = ::EnumDependentServices(
				_hService,
				SERVICE_ACTIVE,
				serviceStatus,
				serviceStatusBufSz,
				&dwBytesNeeded,
				&dwNumServices);

			// If the call returned FALSE get the last error. FALSE isn't necessarily
			// a bad condition. The last error must be checked.
			if (! bRet)
				dwLastError = ::GetLastError();

			// If the call returned FALSE AND the last error isn't "more data"
			// then this is a bad condition.
			if (! bRet && (dwLastError != ERROR_MORE_DATA)) {
				const DWORD lastError = ::GetLastError();
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
				CAF_CM_EXCEPTION_VA2(lastError, "::EnumDependentServices() Failed - serviceName: %s, msg: \"%s\"",
					_serviceName.c_str(), errorMsg.c_str());
			}

			// One of three things is possible:
			//
			// 1. There are services available and the last error
			//    was "more data"
			//
			// 2. There are no services available and the call
			//    returned TRUE: i.e. no dependent services
			//
			// 3. There are no services and the call returned
			//    FALSE: i.e. the buffer isn't big enough
			
			if (0 == dwNumServices) {
				// If the call returned TRUE then this simply means
				// that there are no services.
				if (bRet) {
					bExitLoop = true;
				} else {
					if (dwBytesNeeded < sizeof(ENUM_SERVICE_STATUS)) {
						CAF_CM_EXCEPTIONEX_VA1(
							InvalidArgumentException, E_INVALIDARG,
							"dwBytesNeeded < sizeof(ENUM_SERVICE_STATUS) - serviceName: %s", _serviceName.c_str());
					}
					
					// Resize the buffer to contain all of the bytes.
					delete[] serviceStatus;
					serviceStatusBufSz = dwBytesNeeded - sizeof(ENUM_SERVICE_STATUS);
					serviceStatus = new ENUM_SERVICE_STATUS[serviceStatusBufSz + 1];
				}
			} else {
				// Loop through the services returned and add them to the list
				for (DWORD dwServiceIndex = 0; dwServiceIndex < dwNumServices; dwServiceIndex++) {
					const std::string serviceName = CStringUtils::convertWideToNarrow(serviceStatus[ dwServiceIndex ].lpServiceName);
					rdeqDependentOnMe.push_back(serviceName);
				}

				bExitLoop = true;
			}
		} while(!bExitLoop);
	}
	CAF_CM_CATCH_ALL;

	if (serviceStatus != NULL) {
		delete[] serviceStatus;
	}

	CAF_CM_THROWEXCEPTION;
}

CWinScm::ERc CWinScm::stopDependentServices(
	CdeqDependentOnMe& rdeqDependentOnMe,
	const DWORD stopRetryMax/* = s_stopRetryMax*/,
	const DWORD stopRetryIntervalMillisecs/* = s_stopRetryIntervalMillisecs*/) {
	CAF_CM_FUNCNAME("stopDependentServices");

	ERc eRcDependentsStopped = ERcSucceeded;

	CAF_CM_ENTER {
		// stop the services dependent upon us from outside to inside
		CdeqDependentOnMe::iterator deqIter = rdeqDependentOnMe.begin();
		CdeqDependentOnMe::iterator deqIterEnd = rdeqDependentOnMe.end();
		SmartPtrCWinScm dependentScm;
		for (; deqIter != deqIterEnd; deqIter++) {
			const std::string depServiceName = *deqIter;

			SERVICE_STATUS stServiceStatus;
			::memset(&stServiceStatus, 0, sizeof(stServiceStatus));

			// Create SCM object for the dependent service
			dependentScm.CreateInstance();
			dependentScm->initialize(depServiceName, _machineName);

			// Tell the service to stop.
			if (dependentScm->getServiceStatus().dwCurrentState != SERVICE_STOP_PENDING) {
				dependentScm->controlService(SERVICE_CONTROL_STOP);
			}

			// Wait for the service to stop.
			for (DWORD stopCnt = 0; stopCnt < stopRetryMax; stopCnt++) {
				CAF_CM_LOG_INFO_VA2("Waiting for dependent service to stop - serviceName: %s", depServiceName.c_str(), stopCnt);

				// Is the service stopped?
				stServiceStatus = dependentScm->getServiceStatus();
				if (stServiceStatus.dwCurrentState  == SERVICE_STOPPED) {
					break;
				}

				// Give the Service time to stop
				::Sleep(stopRetryIntervalMillisecs);
			}

			stServiceStatus = dependentScm->getServiceStatus();
			if (stServiceStatus.dwCurrentState != SERVICE_STOPPED) {
				eRcDependentsStopped = ERcFailed;
				CAF_CM_LOG_ERROR_VA1("Failed to stop dependent service - serviceName: %s", depServiceName.c_str());
				break;
			}
		}
	}
	CAF_CM_EXIT;
	
	return eRcDependentsStopped;
}

///////////////////////////////////////////////////////////////////////////////
//
//  CWinScm::attachScm()
//
//  Attaches to the SCM.
//
///////////////////////////////////////////////////////////////////////////////
CWinScm::ERc CWinScm::attachScm(
	const DWORD desiredAccess/* = SC_MANAGER_ALL_ACCESS*/,
	const bool isExceptionOnFailure/* = true*/) {
	CAF_CM_FUNCNAME("attachScm");

	ERc eRc = ERcSucceeded;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		if (! _hSCM) {
			// Open the Sevice Control Manager.
			const std::wstring machineNameWide = CStringUtils::convertNarrowToWide(_machineName);
			_hSCM = ::OpenSCManager(machineNameWide.c_str(), NULL, desiredAccess);
			if (! _hSCM) {
				// Get the error that describes what happened.
				const DWORD lastError = ::GetLastError();

				// If the user wants an exception, give it to 'em.
				if (isExceptionOnFailure) {
					const DWORD lastError = ::GetLastError();
					const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
					CAF_CM_EXCEPTION_VA2(lastError, "::OpenSCManager() Failed - serviceName: %s, msg: \"%s\"",
						_serviceName.c_str(), errorMsg.c_str());
				}

				// If the user doesn't want an exception, then log it as a warning.
				CAF_CM_LOG_WARN_VA1("::OpenSCManager() Failed - serviceName: %s", _serviceName.c_str());

				// Map the return code.
				switch(lastError) {
					case ERROR_ACCESS_DENIED:
						eRc = ERcAccessDenied;
					break;

					default:
						eRc = ERcFailed;
					break;
				}
			}
		}
	}
	CAF_CM_EXIT;

	return eRc;
}

void CWinScm::grantPrivilege(
	const std::wstring& privilegeWide) {
	CAF_CM_FUNCNAME("grantPrivilege");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(privilegeWide);

		DWORD lastError = 0;

		HANDLE hToken = NULL;
		const BOOL bCallSucceeded = ::OpenProcessToken(
			::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
		if (! bCallSucceeded) {
			lastError = ::GetLastError();
			CAF_CM_LOG_WARN_VA0("::OpenProcessToken() Failed");
		} else {
			const std::string privilege = CStringUtils::convertWideToNarrow(privilegeWide);

			LUID stLuidPrivilege;
			if (::LookupPrivilegeValue(NULL, privilegeWide.c_str(), &stLuidPrivilege)) {
				TOKEN_PRIVILEGES tkp;
				tkp.PrivilegeCount = 1;
				tkp.Privileges[0].Luid = stLuidPrivilege;
				tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
				if (! ::AdjustTokenPrivileges(hToken, FALSE,
					&tkp, sizeof( tkp ), NULL, NULL)) {
					lastError = ::GetLastError();
					CAF_CM_LOG_WARN_VA1("::AdjustTokenPrivileges Failed - privilege: %s", privilege.c_str());
				}
			}
			else
			{
				lastError = ::GetLastError();
				CAF_CM_LOG_WARN_VA1("::LookupPrivilegeValue Failed - privilege: %s", privilege.c_str());
			}
		}
	}
	CAF_CM_EXIT;
}
