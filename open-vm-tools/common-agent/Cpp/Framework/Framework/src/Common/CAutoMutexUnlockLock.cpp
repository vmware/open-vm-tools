/*
 *	 Author: bwilliams
 *  Created: Oct 21, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CAutoMutex.h"
#include "Common/CAutoRecMutex.h"
#include "CAutoMutexUnlockLock.h"

using namespace Caf;

CAutoMutexUnlockLock::CAutoMutexUnlockLock(
		SmartPtrCAutoMutex& mutex,
		const char* className,
		const char* funcName,
		const int32 lineNumber) :
				_lineNumber(lineNumber),
				CAF_CM_INIT("CAutoMutexUnlockLock") {
	CAF_CM_FUNCNAME_VALIDATE("CAutoMutexUnlockLock");
	CAF_CM_VALIDATE_SMARTPTR(mutex);

	_mutex = mutex;

	if ((NULL != className) && (NULL != funcName)) {
		_className = className;
		_funcName = funcName;
	}

	_mutex->unlock(className, funcName, lineNumber);
}

CAutoMutexUnlockLock::CAutoMutexUnlockLock(
		SmartPtrCAutoRecMutex& recMutex,
		const char* className,
		const char* funcName,
		const int32 lineNumber) :
				_lineNumber(lineNumber),
				CAF_CM_INIT("CAutoMutexUnlockLock") {
	CAF_CM_FUNCNAME_VALIDATE("CAutoMutexUnlockLock");
	CAF_CM_VALIDATE_SMARTPTR(recMutex);

	_recMutex = recMutex;

	if ((NULL != className) && (NULL != funcName)) {
		_className = className;
		_funcName = funcName;
	}

	_recMutex->unlock(className, funcName, lineNumber);
}

CAutoMutexUnlockLock::~CAutoMutexUnlockLock() {
	if (_className.empty() && _funcName.empty()) {
		if (!_mutex.IsNull()) {
			_mutex->lock();
		}
		if (!_recMutex.IsNull()) {
			_recMutex->lock();
		}
	} else {
		if (!_mutex.IsNull()) {
			_mutex->lock(_className.c_str(), _funcName.c_str(), _lineNumber);
		}
		if (!_recMutex.IsNull()) {
			_recMutex->lock(_className.c_str(), _funcName.c_str(), _lineNumber);
		}
	}
}
