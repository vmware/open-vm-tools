/*
 *	Copyright (c) 2004-2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CAutoFileUnlock.h"

using namespace Caf;

CAutoFileUnlock::CAutoFileUnlock(SmartPtrCFileLock & rspcManagedLock)
{
	m_spcLock = rspcManagedLock;
}

CAutoFileUnlock::~CAutoFileUnlock()
{
	CAF_CM_STATIC_FUNC("CAutoFileUnlock", "~CAutoFileUnlock");
	try
	{
		if (m_spcLock)
		{
			m_spcLock->setLockLevel(CFileLock::UNLOCK, true);
			m_spcLock = 0;
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_CLEAREXCEPTION;
}
