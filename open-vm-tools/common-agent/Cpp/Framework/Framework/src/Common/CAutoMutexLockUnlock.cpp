/*
 *	 Author: mdonahue
 *  Created: Jan 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CAutoMutex.h"
#include "Common/CAutoRecMutex.h"
#include "CAutoMutexLockUnlock.h"

using namespace Caf;

CAutoMutexLockUnlock::CAutoMutexLockUnlock(
	SmartPtrCAutoMutex& mutex,
	const char* className,
	const char* funcName,
	const int32 lineNumber) :
			_lineNumber(0),
			CAF_CM_INIT("CAutoMutexLockUnlock") {
	CAF_CM_FUNCNAME_VALIDATE("CAutoMutexLockUnlock");
	CAF_CM_VALIDATE_SMARTPTR(mutex);

	_mutex = mutex;

	if ((NULL != className) && (NULL != funcName)) {
		_className = className;
		_funcName = funcName;
		_lineNumber = lineNumber;
	}

	_mutex->lock(className, funcName, lineNumber);
}

CAutoMutexLockUnlock::CAutoMutexLockUnlock(
	SmartPtrCAutoRecMutex& recMutex,
	const char* className,
	const char* funcName,
	const int32 lineNumber) :
			_lineNumber(0),
			CAF_CM_INIT("CAutoMutexLockUnlock") {
	CAF_CM_FUNCNAME_VALIDATE("CAutoMutexLockUnlock");
	CAF_CM_VALIDATE_SMARTPTR(recMutex);

	_recMutex = recMutex;

	if ((NULL != className) && (NULL != funcName)) {
		_className = className;
		_funcName = funcName;
		_lineNumber = lineNumber;
	}

	_recMutex->lock(className, funcName, lineNumber);
}

CAutoMutexLockUnlock::~CAutoMutexLockUnlock() {
	if (_className.empty() && _funcName.empty()) {
		if (!_mutex.IsNull()) {
			_mutex->unlock();
		}
		if (!_recMutex.IsNull()) {
			_recMutex->unlock();
		}
	} else {
		if (!_mutex.IsNull()) {
			_mutex->unlock(_className.c_str(), _funcName.c_str(), _lineNumber);
		}
		if (!_recMutex.IsNull()) {
			_recMutex->unlock(_className.c_str(), _funcName.c_str(), _lineNumber);
		}
	}
}
