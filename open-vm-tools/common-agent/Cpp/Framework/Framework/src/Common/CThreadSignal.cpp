/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (c) 2009-2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CThreadSignal.h"

using namespace Caf;

CThreadSignal::CThreadSignal(void) :
	_isInitialized(false),
	_waitCnt(0),
	CAF_CM_INIT("CThreadSignal") {
	CAF_CM_INIT_THREADSAFE
	;
}

CThreadSignal::~CThreadSignal(void) {
}

void CThreadSignal::initialize(const std::string& conditionName) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(conditionName);

	_condition.initialize(conditionName);
	_isInitialized = true;
}

bool CThreadSignal::isInitialized() const {
	CAF_CM_LOCK_UNLOCK;
	return _isInitialized;
}

void CThreadSignal::signal() {
	CAF_CM_FUNCNAME_VALIDATE("signal");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	for (uint32 iter = 0; iter < _waitCnt; iter++) {
		_condition.signal();
	}

	_waitCnt = 0;
}

void CThreadSignal::wait(SmartPtrCAutoMutex& mutex, const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("wait");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(mutex);

	const bool isSignaled = waitOrTimeout(mutex, timeoutMs);
	if (!isSignaled) {
		CAF_CM_EXCEPTION_VA1(ERROR_TIMEOUT, "Signal timed-out: %s",
			_condition.getName().c_str());
	}
}

bool CThreadSignal::waitOrTimeout(SmartPtrCAutoMutex& mutex, const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("waitOrTimeout");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(mutex);

	{
		CAF_CM_LOCK_UNLOCK;
		_waitCnt++;
	}

	bool rc = false;
	if (0 == timeoutMs) {
		_condition.wait(mutex);
		rc = true;
	} else {
		gint64 endTime;
		endTime = ::g_get_monotonic_time() + timeoutMs * G_TIME_SPAN_MILLISECOND;
		rc = _condition.waitUntil(mutex, endTime);
		if (!rc) {
			CAF_CM_LOCK_UNLOCK;
			_waitCnt--;
		}
	}

	return rc;
}

std::string CThreadSignal::getName() const {
	CAF_CM_FUNCNAME_VALIDATE("getName");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _condition.getName();
}

void CThreadSignal::close() {
	CAF_CM_LOCK_UNLOCK;

	if (_isInitialized) {
		_condition.close();
		_waitCnt = 0;
		_isInitialized = false;
	}
}
