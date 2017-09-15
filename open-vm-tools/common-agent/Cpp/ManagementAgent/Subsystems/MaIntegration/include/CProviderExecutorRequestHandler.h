/*
 *	 Author: brets
 *  Created: Nov 20, 2015
 *
 *	Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CProviderExecutorRequestHandler_h_
#define CProviderExecutorRequestHandler_h_


#include "Integration/IRunnable.h"

#include "CProviderExecutorRequest.h"
#include "Common/CAutoMutex.h"
#include "Integration/IErrorHandler.h"
#include "Integration/ITaskExecutor.h"
#include "Integration/ITransformer.h"

namespace Caf {

/// TODO - describe class
class CProviderExecutorRequestHandler : public IRunnable {
public:
	CProviderExecutorRequestHandler();
	virtual ~CProviderExecutorRequestHandler();

public:
	void initialize(const std::string& providerUri,
			const SmartPtrITransformer beginImpersonationTransformer,
			const SmartPtrITransformer endImpersonationTransformer,
			const SmartPtrIErrorHandler errorHandler);

	void handleRequest(const SmartPtrCProviderExecutorRequest request);

public: // IRunnable
	void run();
	void cancel();

private:
	SmartPtrCProviderExecutorRequest getNextPendingRequest();

	void processRequest(const SmartPtrCProviderExecutorRequest& request) const;

	void executeRequestAsync(
			const SmartPtrCProviderExecutorRequest& request);

	std::deque<SmartPtrITaskExecutor> removeFinishedTaskExecutors(
			const std::deque<SmartPtrITaskExecutor> taskExecutors) const;

private:
	bool _isInitialized;
	bool _isCancelled;
	std::string _providerPath;
	std::string _providerUri;
	std::deque<SmartPtrITaskExecutor> _taskExecutors;
	SmartPtrCAutoMutex _mutex;
	std::deque<SmartPtrCProviderExecutorRequest> _pendingRequests;
	SmartPtrITransformer _beginImpersonationTransformer;
	SmartPtrITransformer _endImpersonationTransformer;
	SmartPtrIErrorHandler _errorHandler;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CProviderExecutorRequestHandler);
};

CAF_DECLARE_SMART_POINTER(CProviderExecutorRequestHandler);

}

#endif // #ifndef CProviderExecutorRequestHandler_h_
