/*
 *	Copyright (C) 2004-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CFileLock.h"
#include "Exception/CCafException.h"
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include <sstream>

using namespace Caf;

////////////////////////////////////////////////////////////////////////
//
//  CFileLock::CFileLock()
//
////////////////////////////////////////////////////////////////////////
CFileLock::CFileLock() :
	CAF_CM_INIT("CFileLock"),
    _isInitialized(false),
    _isFileDescriptorLocal(false),
    _fileDescriptor(-1),
    _lockLevel(UNLOCK) {
}

////////////////////////////////////////////////////////////////////////
//
//  CFileLock::~CFileLock()
//  
////////////////////////////////////////////////////////////////////////
CFileLock::~CFileLock() {
	CAF_CM_FUNCNAME("~CFileLock");
	try 	{
		if (_isInitialized) 		{
			setLockLevel(UNLOCK, true);
			if (_isFileDescriptorLocal)
				::close(_fileDescriptor);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_CLEAREXCEPTION;
}

////////////////////////////////////////////////////////////////////////
//
//  CFileLock::Initialize()
//
////////////////////////////////////////////////////////////////////////
void CFileLock::initialize(int32 iFileDescriptor) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_lockLevel = UNLOCK;
	_fileDescriptor = iFileDescriptor;
	_isFileDescriptorLocal = false;
	_isInitialized = true;
}

////////////////////////////////////////////////////////////////////////
//
//  CFileLock::Initialize()
//
////////////////////////////////////////////////////////////////////////
void CFileLock::initialize(const char* cszFilename, bool bCreateFile) {
   CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRINGPTRA(cszFilename);

	int32 iFlags = bCreateFile ? O_RDWR | O_CREAT : O_RDWR;
   int32 iFileDescriptor = ::open(cszFilename, iFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

   if (-1 != iFileDescriptor) {
		_lockLevel = UNLOCK;
		_fileDescriptor = iFileDescriptor;
		_isFileDescriptorLocal = true;
		_isInitialized = true;
	}
	else {
		int32 iRc = errno;
		CAF_CM_EXCEPTION_VA1(iRc, "Unable to open file %s", cszFilename);
	}
}

////////////////////////////////////////////////////////////////////////
//
// CFileLock::GetLockLevel()
//
////////////////////////////////////////////////////////////////////////
CFileLock::ELockLevel CFileLock::getLockLevel() const {
    CAF_CM_FUNCNAME_VALIDATE("getLockLeve");
    CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
    return _lockLevel;
}

////////////////////////////////////////////////////////////////////////
//
// CFileLock::SetLockLevel()
//
////////////////////////////////////////////////////////////////////////
void CFileLock::setLockLevel(ELockLevel eLockLevel, bool bDowngradeLock) {
   CAF_CM_FUNCNAME("setLockLeve");
 	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	bool bLockChangeOK = true;
	// determine if we are attempting to downgrade an existing lock
	if (!bDowngradeLock) {
		if (eLockLevel <= _lockLevel) {
			bLockChangeOK = false;
		}
	}

	if (bLockChangeOK && (eLockLevel != _lockLevel)) {
		struct flock stFlock;
		::memset(&stFlock, 0, sizeof(stFlock));
		stFlock.l_whence = SEEK_SET;
		stFlock.l_start = 0;
		stFlock.l_len = 1;
		if (SHARED_LOCK == eLockLevel)
			stFlock.l_type = F_RDLCK;
		else if (EXCLUSIVE_LOCK == eLockLevel)
			stFlock.l_type = F_WRLCK;
		else
			stFlock.l_type = F_UNLCK;
		
		if (-1 != ::fcntl(_fileDescriptor, F_SETLKW, &stFlock)) {
			_lockLevel = eLockLevel;
		}
		else {
			CAF_CM_EXCEPTION_VA0(errno, "Unable to modify lock");
		}
	}
}

////////////////////////////////////////////////////////////////////////
//
// CFileLock::AttemptSetLockLevel()
//
////////////////////////////////////////////////////////////////////////
bool CFileLock::attemptSetLockLevel(ELockLevel eLockLevel, bool bDowngradeLock) {
   CAF_CM_FUNCNAME("attemptSetLockLeve");
 	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	bool bRet = false;
	bool bLockChangeOK = true;

	// determine if we are attempting to downgrade an existing lock
	if (!bDowngradeLock) {
		if (eLockLevel <= _lockLevel) {
			bLockChangeOK = false;
		}
	}

	if (bLockChangeOK && (eLockLevel != _lockLevel)) {
		struct flock stFlock;
		::memset(&stFlock, 0, sizeof(stFlock));
		stFlock.l_whence = SEEK_SET;
		stFlock.l_start = 0;
		stFlock.l_len = 1;
		if (SHARED_LOCK == eLockLevel)
			stFlock.l_type = F_RDLCK;
		else if (EXCLUSIVE_LOCK == eLockLevel)
			stFlock.l_type = F_WRLCK;
		else
			stFlock.l_type = F_UNLCK;
		
		if (-1 != ::fcntl(_fileDescriptor, F_SETLK, &stFlock)) {
			_lockLevel = eLockLevel;
			bRet = true;
		}
		else if ((EACCES != errno) && (EAGAIN != errno)) {
			CAF_CM_EXCEPTION_VA0(errno, "Unable to modify lock");
		}
	}

	return bRet;
}
