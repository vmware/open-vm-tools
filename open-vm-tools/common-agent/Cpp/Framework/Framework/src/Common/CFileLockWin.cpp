/*
 *	 Author: mdonahue
 *  Created: Nov 14 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CFileLock.h"

using namespace Caf;
 
CFileLock::CFileLock() {
}

CFileLock::~CFileLock() {
}

void CFileLock::initialize(int32 iFileDescriptor) {
}

void CFileLock::initialize(const char * cszFileName, bool bCreateFile) {
}

void CFileLock::setLockLevel(ELockLevel eLockLevel, bool bDowngradeLock) {
}

bool CFileLock::attemptSetLockLevel(ELockLevel eLockLevel, bool bDowngradeLock) {
	return false;
}

CFileLock::ELockLevel CFileLock::getLockLevel() const {
	return UNLOCK;
}
