/*
 *	Copyright (C) 2004-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAutoFileUnlock_h_
#define CAutoFileUnlock_h_


#include "Common/CFileLock.h"

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CAutoFileUnlock
{
public:
	CAutoFileUnlock(SmartPtrCFileLock & rspcManagedLock);
	~CAutoFileUnlock();

private:
	SmartPtrCFileLock m_spcLock;
	CAF_CM_DECLARE_NOCOPY(CAutoFileUnlock);
};

}

#endif
