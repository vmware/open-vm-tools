/*
 *  Created on: Aug 20, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AmqpListenerWorker_h
#define AmqpListenerWorker_h

#include "Common/CThreadSignal.h"
#include "Common/IWork.h"

namespace Caf {

class AmqpListenerWorker :
	public IWork
{
public:
	AmqpListenerWorker();
	virtual ~AmqpListenerWorker();

	void doWork();
	void stopWork();

private:
	CThreadSignal _stopSignal;
	CAF_THREADSIGNAL_CREATE;
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(AmqpListenerWorker);
};
CAF_DECLARE_SMART_POINTER(AmqpListenerWorker);
}

#endif /* AmqpListenerWorker_h */
