/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CManagementAgentHostWork_h_
#define CManagementAgentHostWork_h_

#include "Common/IWork.h"

namespace Caf {

/// Sends responses/errors back to the client.
class CManagementAgentHostWork :
	public IWork
{
public:
	CManagementAgentHostWork();
	virtual ~CManagementAgentHostWork();

public:
	void initialize();

public: // IWork
	void doWork();
	void stopWork();

private:
	bool _isInitialized;
	bool _isWorking;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CManagementAgentHostWork);
};

CAF_DECLARE_SMART_POINTER(CManagementAgentHostWork);

}

#endif // #ifndef CManagementAgentHostWork_h_
