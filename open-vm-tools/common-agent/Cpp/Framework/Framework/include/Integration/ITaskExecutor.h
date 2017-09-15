/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_ITaskExecutor_h_
#define _IntegrationContracts_ITaskExecutor_h_

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	ITaskExecutor : public ICafObject
{
	CAF_DECL_UUID("4ab38314-fd31-49fc-bfce-173abc53f1a8")

	typedef enum {
		ETaskStateNotStarted,
		ETaskStateStarted,
		ETaskStateStopping,
		ETaskStateFinished,
		ETaskStateFailed
	} ETaskState;

	virtual void execute(const uint32 timeoutMs) = 0;
	virtual void cancel(const uint32 timeoutMs) = 0;
	virtual ETaskState getState() const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(ITaskExecutor);

}

#endif // #ifndef _IntegrationContracts_ITaskExecutor_h_

