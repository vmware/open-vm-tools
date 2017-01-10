/*
 *	Copyright (C) 2004-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CFileLock_h_
#define CFileLock_h_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CFileLock
{
public:
	enum ELockLevel
	{
		UNLOCK,
		SHARED_LOCK,
		EXCLUSIVE_LOCK
	};

public:
	// Ctor
	CFileLock(); 

	// dtor
	~CFileLock();

	// initialize the lock for the specified file
	void initialize(int32 iFileDescriptor);

	// initialize the lock for the specified file
	void initialize(const char * cszFileName, bool bCreateFile = true);

	// Set the lock to the specified scope
	void setLockLevel(ELockLevel eLockLevel, bool bDowngradeLock = false);

	// Set the lock to the specified if possible
	bool attemptSetLockLevel(ELockLevel eLockLevel, bool bDowngradeLock = false);

	// Get the current lock level
	ELockLevel getLockLevel() const;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CFileLock);

    bool _isInitialized;
	bool _isFileDescriptorLocal;
	int32 _fileDescriptor;
	ELockLevel _lockLevel;
};

CAF_DECLARE_SMART_POINTER(CFileLock);

}

#endif
