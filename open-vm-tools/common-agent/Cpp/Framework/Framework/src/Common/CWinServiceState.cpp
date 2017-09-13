/*
 *  Author: bwilliams
 *  Created: June 29, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CWinServiceState.h"

using namespace Caf;

CWinServiceState::CWinServiceState() :
	CAF_CM_INIT_LOG("CWinServiceState"),
	_isInitialized(false),
	_isService(true),
	_serviceHandle(NULL),
	_currentServiceState(0),
	_scmWaitHintMs(3000),
	_workerThreadStopMs(1500) {
	CAF_CM_INIT_THREADSAFE;
	_waitMutex.CreateInstance();
	_waitMutex->initialize();
}

CWinServiceState::~CWinServiceState() {
}

void CWinServiceState::initialize(
		const std::string& serviceName,
		const std::string& displayName,
		const std::string& description,
		const SmartPtrIWork& work) {
	CAF_CM_FUNCNAME("initialize");

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(serviceName);
		CAF_CM_VALIDATE_STRING(displayName);
		CAF_CM_VALIDATE_SMARTPTR(work);

		// Store the elements of the path into member variables.
		_serviceName = serviceName;
		_displayName = displayName;
		_description = description;
		_work = work;

		_serviceStopSignal.initialize("serviceStopSignal");
		_workerThreadFinishedSignal.initialize("workerThreadFinishedSignal");

		_isInitialized = true;
	}
	CAF_CM_UNLOCK_AND_EXIT;
}

std::string CWinServiceState::getServiceName() const {
	CAF_CM_FUNCNAME("getServiceName");

	std::string rc;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _serviceName;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

std::string CWinServiceState::getDisplayName() const {
	CAF_CM_FUNCNAME("getDisplayName");

	std::string rc;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _displayName;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

std::string CWinServiceState::getDescription() const {
	CAF_CM_FUNCNAME("getDescription");

	std::string rc;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _description;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

SmartPtrIWork CWinServiceState::getWork() const {
	CAF_CM_FUNCNAME("getWork");

	SmartPtrIWork rc;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _work;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

DWORD CWinServiceState::getWorkerThreadStopMs() const {
	CAF_CM_FUNCNAME("getWorkerThreadStopMs");

	DWORD rc = 0;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _workerThreadStopMs;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

DWORD CWinServiceState::getScmWaitHintMs() const {
	CAF_CM_FUNCNAME("getScmWaitHintMs");

	DWORD rc = 0;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _scmWaitHintMs;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

bool CWinServiceState::getIsService() const {
	CAF_CM_FUNCNAME("getIsService");

	bool rc = false;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _isService;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

void CWinServiceState::putIsService(
	const bool isService) {
	CAF_CM_FUNCNAME("putIsService");

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		_isService = isService;
	}
	CAF_CM_UNLOCK_AND_EXIT;
}

SERVICE_STATUS_HANDLE CWinServiceState::getServiceHandle() const {
	CAF_CM_FUNCNAME("getServiceHandle");

	SERVICE_STATUS_HANDLE rc = NULL;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _serviceHandle;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

void CWinServiceState::putServiceHandle(
	const SERVICE_STATUS_HANDLE serviceHandle) {
	CAF_CM_FUNCNAME("putServiceHandle");

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_PTR(serviceHandle);
		_serviceHandle = serviceHandle;
	}
	CAF_CM_UNLOCK_AND_EXIT;
}

DWORD CWinServiceState::getCurrentServiceState() const {
	CAF_CM_FUNCNAME("getCurrentServiceState");

	DWORD rc = 0;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _currentServiceState;
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

std::string CWinServiceState::getCurrentServiceStateStr() const {
	CAF_CM_FUNCNAME("getCurrentServiceState");

	std::string rc;

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		switch (_currentServiceState) {
			case SERVICE_CONTINUE_PENDING:
				rc = "SERVICE_CONTINUE_PENDING";
			break;
			case SERVICE_PAUSE_PENDING:
				rc = "SERVICE_PAUSE_PENDING";
			break;
			case SERVICE_PAUSED:
				rc = "SERVICE_PAUSED";
			break;
			case SERVICE_RUNNING:
				rc = "SERVICE_RUNNING";
			break;
			case SERVICE_START_PENDING:
				rc = "SERVICE_START_PENDING";
			break;
			case SERVICE_STOP_PENDING:
				rc = "SERVICE_STOP_PENDING";
			break;
			case SERVICE_STOPPED:
				rc = "SERVICE_STOPPED";
			break;
			default:
				rc = "Unknown";
			break;
		}
	}
	CAF_CM_UNLOCK_AND_EXIT;

	return rc;
}

void CWinServiceState::putCurrentServiceState(
	const DWORD currentServiceState) {
	CAF_CM_FUNCNAME("putCurrentServiceState");

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		_currentServiceState = currentServiceState;
	}
	CAF_CM_UNLOCK_AND_EXIT;
}

void CWinServiceState::signalServiceStop() {
	CAF_CM_FUNCNAME("signalServiceStop");

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		CAF_CM_LOG_DEBUG_VA1("Signal (%s)", _serviceStopSignal.getName().c_str());
		_serviceStopSignal.signal();
	}
	CAF_CM_UNLOCK_AND_EXIT;
}

bool CWinServiceState::waitForServiceStop(
	const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("waitForServiceStop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	//CAF_CM_LOG_DEBUG_VA2("Wait (%s) - waitMs: %d", _serviceStopSignal.getName().c_str(), timeoutMs);
	CAutoMutexLockUnlock waitMutexAutoLock(_waitMutex);
	const bool isSignaled = _serviceStopSignal.waitOrTimeout(_waitMutex, timeoutMs);

	return isSignaled;
}

void CWinServiceState::signalWorkerThreadFinished() {
	CAF_CM_FUNCNAME("signalWorkerThreadFinished");

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		CAF_CM_LOG_DEBUG_VA1("Signal (%s)", _workerThreadFinishedSignal.getName().c_str());
		_workerThreadFinishedSignal.signal();
	}
	CAF_CM_UNLOCK_AND_EXIT;
}

bool CWinServiceState::waitForWorkerThreadFinished(
	const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("waitForWorkerThreadFinished");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	//CAF_CM_LOG_DEBUG_VA2("Wait (%s) - waitMs: %d", _workerThreadFinishedSignal.getName().c_str(), timeoutMs);
	CAutoMutexLockUnlock waitMutexAutoLock(_waitMutex);
	const bool isSignaled = _workerThreadFinishedSignal.waitOrTimeout(_waitMutex, timeoutMs);

	return isSignaled;
}

void CWinServiceState::close() {
	CAF_CM_FUNCNAME("close");

	CAF_CM_ENTER_AND_LOCK {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		_serviceHandle = NULL;
		_isService = true;
		_currentServiceState = 0;
		_scmWaitHintMs = 3000;
		_workerThreadStopMs = 1500;

		_serviceStopSignal.close();
		_workerThreadFinishedSignal.close();

		_isInitialized = false;
	}
	CAF_CM_UNLOCK_AND_EXIT;
}

void CWinServiceState::setStatus() {
	CAF_CM_FUNCNAME("setStatus");

	CAF_CM_LOG_DEBUG_VA1("setStatus - %s", getCurrentServiceStateStr().c_str());

	CWinScm winScm(getServiceName());
	winScm.setStatus(getServiceHandle(),
			getCurrentServiceState(),
			(DWORD) NO_ERROR,
			(DWORD) 0,
			getScmWaitHintMs());
}
