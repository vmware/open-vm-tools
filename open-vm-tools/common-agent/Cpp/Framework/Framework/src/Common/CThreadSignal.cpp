/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CAutoMutex.h"
#include "Exception/CCafException.h"
#include "Common/CThreadSignal.h"

using namespace Caf;

CThreadSignal::CThreadSignal(void) :
	_isInitialized(false),
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

	_condition.signal();
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

	bool rc = false;
	if (0 == timeoutMs) {
		_condition.wait(mutex);
		rc = true;
	} else {
		gint64 endTime;
		endTime = ::g_get_monotonic_time() + timeoutMs * G_TIME_SPAN_MILLISECOND;
		rc = _condition.waitUntil(mutex, endTime);
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
		_isInitialized = false;
	}
}
