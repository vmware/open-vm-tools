/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CAutoMutex.h"
#include "Integration/Core/CIntException.h"
#include "Integration/Core/CSimpleAsyncTaskExecutorState.h"
#include "Integration/IErrorHandler.h"
#include "Integration/IRunnable.h"
#include "Integration/ITaskExecutor.h"
#include "Integration/Core/CSimpleAsyncTaskExecutor.h"
#include "Exception/CCafException.h"

using namespace Caf;

CSimpleAsyncTaskExecutor::CSimpleAsyncTaskExecutor() :
	_isInitialized(false),
	_thread(NULL),
	CAF_CM_INIT_LOG("CSimpleAsyncTaskExecutor") {
	CAF_CM_FUNCNAME("CSimpleAsyncTaskExecutor");

	try {
		CAF_CM_INIT_THREADSAFE;
		CAF_THREADSIGNAL_INIT;
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

CSimpleAsyncTaskExecutor::~CSimpleAsyncTaskExecutor() {
	CAF_CM_FUNCNAME("~CSimpleAsyncTaskExecutor");

	try {
		cancel(0);
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	try {
		if (_thread) {
			CThreadUtils::join(_thread);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

void CSimpleAsyncTaskExecutor::initialize(
	const SmartPtrIRunnable& runnable,
	const SmartPtrIErrorHandler& errorHandler) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(runnable);
	CAF_CM_VALIDATE_INTERFACE(errorHandler);

	_state.CreateInstance();
	_state->initialize(runnable, errorHandler);

	_isInitialized = true;
}

void CSimpleAsyncTaskExecutor::execute(
	const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("execute");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (ITaskExecutor::ETaskStateNotStarted == _state->getState()) {
		// The mutex must be locked before calling waitForStart
		// and before letting the thread spin up to avoid
		// a race condition.  See g_cond_timed_wait()
		//
		// Scope the CAF_THREADSIGNAL_LOCK_UNLOCK so that the
		// mutex is guaranteed to unlock
		{
			CAF_CM_LOG_DEBUG_VA0("Starting the thread");
			CAF_THREADSIGNAL_LOCK_UNLOCK;
			CThreadData threadData = std::make_pair(
					CAF_THREADSIGNAL_MUTEX,
					_state.GetNonAddRefedInterface());
			CAF_CM_VALIDATE_NULLPTR(_thread);
			_thread = CThreadUtils::startJoinable(threadFunc, &threadData);

			CAF_CM_UNLOCK_LOCK;
			_state->waitForStart(CAF_THREADSIGNAL_MUTEX, timeoutMs);
		}
		if (_state->getState() != ITaskExecutor::ETaskStateStarted) {
			CAF_CM_EXCEPTION_VA1(ERROR_INVALID_STATE,
					"Not Started: %s", _state->getStateStr().c_str());
		}
	} else if (ITaskExecutor::ETaskStateStarted != _state->getState()) {
		CAF_CM_EXCEPTION_VA1(ERROR_INVALID_STATE,
				"Invalid State: %s", _state->getStateStr().c_str());
	}

	CAF_CM_LOG_INFO_VA0("Started");
}

void CSimpleAsyncTaskExecutor::cancel(
	const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("cancel");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (ITaskExecutor::ETaskStateStarted == _state->getState()) {
		CAF_CM_LOG_INFO_VA0("Stopping");
		_state->setState(ITaskExecutor::ETaskStateStopping);
	}

	if (ITaskExecutor::ETaskStateStopping == _state->getState()) {
		_state->getRunnable()->cancel();

		// The mutex must be locked before calling waitForStop
		// and before letting the thread spin up to avoid
		// a race condition.  See g_cond_timed_wait()
		//
		// Scope the CAF_THREADSIGNAL_LOCK_UNLOCK so that the
		// mutex is guaranteed to unlock
		{
			CAF_THREADSIGNAL_LOCK_UNLOCK;
			CAF_CM_UNLOCK_LOCK;
			while (! _state->getHasThreadExited()) {
				_state->waitForStop(CAF_THREADSIGNAL_MUTEX, timeoutMs);
			}
		}

		if (_thread) {
			CThreadUtils::join(_thread);
			_thread = NULL;
		}

		if (_state->getState() != ITaskExecutor::ETaskStateFinished) {
			CAF_CM_EXCEPTION_VA1(ERROR_INVALID_STATE, "Not Stopped: %s",
				_state->getStateStr().c_str());
		}
	} else if (!((ITaskExecutor::ETaskStateFinished == _state->getState()) ||
		(ITaskExecutor::ETaskStateFailed == _state->getState()))) {
		CAF_CM_EXCEPTION_VA1(ERROR_INVALID_STATE, "Invalid State: %s",
			_state->getStateStr().c_str());
	}

	CAF_CM_LOG_INFO_VA0("Stopped");
}

ITaskExecutor::ETaskState CSimpleAsyncTaskExecutor::getState() const {
	CAF_CM_FUNCNAME_VALIDATE("getState");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _state->getState();
}

void* CSimpleAsyncTaskExecutor::threadFunc(void* data) {
	CAF_CM_STATIC_FUNC_LOG("CSimpleAsyncTaskExecutor", "threadFunc");

	SmartPtrCSimpleAsyncTaskExecutorState state;
	SmartPtrCAutoMutex mutex;
	ITaskExecutor::ETaskState stateState = ITaskExecutor::ETaskStateFailed;

	try {
		CAF_CM_VALIDATE_PTR(data);
		CThreadData *threadData = static_cast<CThreadData*>(data);
		CAF_CM_VALIDATE_PTR(threadData->first);
		CAF_CM_VALIDATE_PTR(threadData->second);
		mutex = threadData->first;
		state = static_cast<CSimpleAsyncTaskExecutorState*>(threadData->second);
		CAF_CM_VALIDATE_SMARTPTR(state);

		try {
			state->setState(ITaskExecutor::ETaskStateStarted);

			CAF_CM_LOCK_UNLOCK1(mutex);
			state->signalStart();
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;

		if (!CAF_CM_ISEXCEPTION) {
			try {
				state->getRunnable()->run();
				stateState = ITaskExecutor::ETaskStateFinished;
			}
			CAF_CM_CATCH_ALL;
			CAF_CM_LOG_CRIT_CAFEXCEPTION;
		}

		try {
			if (CAF_CM_ISEXCEPTION) {
				SmartPtrCIntException intException;
				intException.CreateInstance();
				intException->initialize(CAF_CM_GETEXCEPTION);
				state->getErrorHandler()->handleError(intException, SmartPtrIIntMessage());

				CAF_CM_CLEAREXCEPTION;
			}
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	if (! state.IsNull()) {
		try {
			state->setState(stateState);
			state->setThreadExited();

			CAF_CM_LOCK_UNLOCK1(mutex);
			state->signalStop();
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}

	CAF_CM_LOG_INFO_VA0("**** Thread exiting ****");

	return NULL;
}
