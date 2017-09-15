/*
 *	Copyright (c) 2004-2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
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
