/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CSimpleAsyncTaskExecutor_h_
#define CSimpleAsyncTaskExecutor_h_

#include "Common/CThreadSignal.h"

#include "Integration/Core/CSimpleAsyncTaskExecutorState.h"
#include "Integration/IErrorHandler.h"
#include "Integration/IRunnable.h"

#include "Integration/ITaskExecutor.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CSimpleAsyncTaskExecutor :
	public ITaskExecutor {
public:
	CSimpleAsyncTaskExecutor();
	virtual ~CSimpleAsyncTaskExecutor();

public:
	void initialize(
		const SmartPtrIRunnable& runnable,
		const SmartPtrIErrorHandler& errorHandler);

public: // ITaskExecutor
	void execute(const uint32 timeoutMs);
	void cancel(const uint32 timeoutMs);
	ETaskState getState() const;

private:
	static void* threadFunc(void* data);

private:
	bool _isInitialized;
	GThread* _thread;
	SmartPtrCSimpleAsyncTaskExecutorState _state;
	typedef std::pair<SmartPtrCAutoMutex, CSimpleAsyncTaskExecutorState*> CThreadData;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_THREADSIGNAL_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CSimpleAsyncTaskExecutor);
};

CAF_DECLARE_SMART_POINTER(CSimpleAsyncTaskExecutor);

}

#endif // #ifndef CSimpleAsyncTaskExecutor_h_
