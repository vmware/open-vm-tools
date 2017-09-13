/*
 *  Author: bwilliams
 *  Created: June 25, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CWinScm_h_
#define CWinScm_h_

namespace Caf {

/// TODO - describe class
class COMMONAGGREGATOR_LINKAGE CWinScm {
public:
	typedef enum {
		ERcSucceeded,
		ERcFailed,
		ERcAccessDenied
	} ERc;

	struct SServiceConfig {
		DWORD		_serviceType;
		DWORD		_startType;
		DWORD		_errorControl;
		std::string	_binaryPathName;
		std::string	_loadOrderGroup;
		DWORD		_tagId;
		std::string	_dependencies;
		std::string	_serviceStartName;
		std::string	_displayName;
	};
	CAF_DECLARE_SMART_POINTER(SServiceConfig);

public:
	typedef std::vector<std::string> CvecDependencies;
	typedef std::deque<std::string> CdeqDependentOnMe;

public:
	CWinScm();
	CWinScm(const std::string& serviceName);
	virtual ~CWinScm();

public:
	///  Initializes the object.
	void initialize(
		const std::string& serviceName,
		const std::string& machineName);

public:
	///  Creates the Service.
	void createService(
		const std::string& crstrServiceFilename,
		const DWORD cdwStartType = SERVICE_DEMAND_START,
		const CvecDependencies cvecDependencies = CvecDependencies());

	///  Creates the Service.
	void createService(
		const std::string& crstrServiceFilename,
		const std::string& crstrServiceDisplayName,
		const std::string& crstrServiceDescription,
		const std::string& crstrServiceAccountName,
		const std::string& crstrServiceAccountPasswd,
		const DWORD cdwStartType = SERVICE_DEMAND_START,
		const CvecDependencies cvecDependencies = CvecDependencies());

public:
	void changeService(
		const std::string& crstrServiceFilename,
		const std::string& crstrServiceDisplayName = std::string(),
		const std::string& crstrServiceDescription = std::string(),
		const std::string& crstrServiceAccountName = std::string(),
		const std::string& crstrServiceAccountPasswd = std::string(),
		const DWORD cdwStartupType = SERVICE_NO_CHANGE);

	void changeServiceRecoveryProperties(
		const std::string& crstrServiceFilename,
		const DWORD cdwFirstFailureAction,
		const DWORD cdwSecondFailureAction,
		const DWORD cdwSubsequentFailureAction,
		const DWORD cdwResetFailureCountAfter_days,
		const DWORD cdwRestartServiceAfter_minutes,
		const LPCWSTR clpstrCommandLineToRun,
		const DWORD cdwRebootComputerAfter_minutes,
		const LPCWSTR clpstrRestartMessage );

public:
	void deleteService(
		const DWORD cdwStopRetryMax = s_stopRetryMax,
		const DWORD cdwStopRetryIntervalMillisecs = s_stopRetryIntervalMillisecs,
		const DWORD cdwServicePid = 0);

public:
	SERVICE_STATUS startService(
		const DWORD cdwStartPollMax = s_startPollMax,
		const DWORD cdwStartPollIntervalMillisecs = s_startPollIntervalMillisecs,
		const DWORD cdwStartRetryMax = s_startRetryMax,
		const DWORD cdwStartRetryIntervalMillisecs = s_startRetryIntervalMillisecs);

	SERVICE_STATUS stopService(
		const DWORD cdwStopRetryMax = s_stopRetryMax,
		const DWORD cdwStopRetryIntervalMillisecs = s_stopRetryIntervalMillisecs,
		const DWORD cdwServicePid = 0);

public:
	SERVICE_STATUS getServiceStatus(
		const bool cbIsExceptionOnMissingService = true);

	SERVICE_STATUS controlService(
		const DWORD cdwCommand);

	bool openService(
		const bool cbIsExceptionOnMissingService,
		const DWORD scmDesiredAccess = SC_MANAGER_ALL_ACCESS,
		const DWORD desiredAccess = SERVICE_ALL_ACCESS
		);

	void setStatus(
		const SERVICE_STATUS_HANDLE chSrv,
		const DWORD cdwState,
		const DWORD cdwExitCode,
		const DWORD cdwProgress = 0,
		const DWORD cdwWaitHintMilliseconds = 3000 );

public:
	void getDependentServices(
		CdeqDependentOnMe& rdeqDependentOnMe);

	ERc stopDependentServices(
		CdeqDependentOnMe& rdeqDependentOnMe,
		const DWORD cdwStopRetryMax = s_stopRetryMax,
		const DWORD cdwStopRetryIntervalMillisecs = s_stopRetryIntervalMillisecs);

	SmartPtrSServiceConfig getServiceConfig(
		const bool cbIsExceptionOnMissingService = true);

private:
	void closeHandle(SC_HANDLE& rhService);

	SERVICE_STATUS startServiceInternal(
		const DWORD cdwStartPollMax = s_startPollMax,
		const DWORD cdwStartPollIntervalMillisecs = s_startPollIntervalMillisecs);

	void attachScmRequired(
		const DWORD desiredAccess = SC_MANAGER_ALL_ACCESS);

	void attachScmOptional(
		const DWORD desiredAccess = SC_MANAGER_ALL_ACCESS);

	ERc attachScm(
		const DWORD cdwDesiredAccess = SC_MANAGER_ALL_ACCESS,
		const bool cbIsExceptionOnFailure = true);

	void grantPrivilege(
		const std::wstring& privilegeWide);

private:
	// The maximum number of times to wait for the service to stop and time
	// to wait between each attempt.
	static const DWORD s_stopRetryMax;
	static const DWORD s_stopRetryIntervalMillisecs;

	static const DWORD s_startPollMax;
	static const DWORD s_startPollIntervalMillisecs;
	static const DWORD s_startRetryMax;
	static const DWORD s_startRetryIntervalMillisecs;

private:
	bool _isInitialized;
	std::string _serviceName;
	std::string _machineName;
	SC_HANDLE _hSCM;
	SC_HANDLE _hService;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CWinScm);
};

CAF_DECLARE_SMART_POINTER(CWinScm);

}

#endif // #ifndef CWinScm_h_
