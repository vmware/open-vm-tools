/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CSourcePollingChannelAdapter_h_
#define CSourcePollingChannelAdapter_h_

#include "Common/CThreadSignal.h"

#include "Integration/Dependencies/CPollerMetadata.h"
#include "Integration/IErrorHandler.h"
#include "Integration/IMessageHandler.h"
#include "Integration/IPollableChannel.h"

#include "Integration/IRunnable.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CSourcePollingChannelAdapter :
	public IRunnable {
public:
	CSourcePollingChannelAdapter();
	virtual ~CSourcePollingChannelAdapter();

public:
	void initialize(
		const SmartPtrIMessageHandler& messageHandler,
		const SmartPtrIPollableChannel& inputPollableChannel,
		const SmartPtrIErrorHandler& errorHandler);

	void initialize(
		const SmartPtrIMessageHandler& messageHandler,
		const SmartPtrIPollableChannel& inputPollableChannel,
		const SmartPtrIErrorHandler& errorHandler,
		const int32 timeout);

public: // IRunnable
	void run();
	void cancel();

private:
	bool getIsCancelled() const;

private:
	bool _isInitialized;
	bool _isCancelled;
	bool _isTimeoutSet;
	int32 _timeout;
	SmartPtrIMessageHandler _messageHandler;
	SmartPtrIPollableChannel _inputPollableChannel;
	SmartPtrIErrorHandler _errorHandler;
	SmartPtrCPollerMetadata _pollerMetadata;
	CThreadSignal _threadSignalCancel;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_THREADSIGNAL_CREATE;
	CAF_CM_DECLARE_NOCOPY(CSourcePollingChannelAdapter);
};

CAF_DECLARE_SMART_POINTER(CSourcePollingChannelAdapter);

}

#endif // #ifndef CSourcePollingChannelAdapter_h_
