/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CSimpleAsyncTaskExecutorState_h_
#define CSimpleAsyncTaskExecutorState_h_

#include "Common/CThreadSignal.h"

#include "Common/CAutoMutex.h"
#include "Integration/IErrorHandler.h"
#include "Integration/IRunnable.h"
#include "Integration/ITaskExecutor.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CSimpleAsyncTaskExecutorState {
public:
	CSimpleAsyncTaskExecutorState();
	virtual ~CSimpleAsyncTaskExecutorState();

public:
	void initialize(
		const SmartPtrIRunnable& runnable,
		const SmartPtrIErrorHandler& errorHandler);

	SmartPtrIRunnable getRunnable() const;
	SmartPtrIErrorHandler getErrorHandler() const;

	ITaskExecutor::ETaskState getState() const;
	std::string getStateStr() const;
	void setState(const ITaskExecutor::ETaskState runnableState);

	bool getHasThreadExited();
	void setThreadExited();

	void signalStart();
	void waitForStart(SmartPtrCAutoMutex& mutex, const uint32 timeoutMs);

	void signalStop();
	void waitForStop(SmartPtrCAutoMutex& mutex, const uint32 timeoutMs);

private:
	bool _isInitialized;
	bool _hasThreadExited;
	ITaskExecutor::ETaskState _runnableState;
	SmartPtrIRunnable _runnable;
	SmartPtrIErrorHandler _errorHandler;
	std::string _exceptionMessage;

	CThreadSignal _threadSignalStart;
	CThreadSignal _threadSignalStop;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CSimpleAsyncTaskExecutorState);
};

CAF_DECLARE_SMART_POINTER(CSimpleAsyncTaskExecutorState);

}

#endif // #ifndef CSimpleAsyncTaskExecutorState_h_
