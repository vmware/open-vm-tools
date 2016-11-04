/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CThreadSignal_h_
#define CThreadSignal_h_

#include "Common/CThreadSignal.h"

#include "Common/CAutoMutex.h"
#include "Common/CAutoCondition.h"

namespace Caf {

// Class-level thread safety macros
#define CAF_THREADSIGNAL_CREATE \
	private: \
	mutable SmartPtrCAutoMutex _threadsync_mutex_

#define CAF_THREADSIGNAL_INIT \
	_threadsync_mutex_.CreateInstance(); \
	_threadsync_mutex_->initialize()

#define CAF_THREADSIGNAL_MUTEX \
	_threadsync_mutex_

#define CAF_THREADSIGNAL_LOCK_UNLOCK \
	Caf::CAutoMutexLockUnlock _threadsync_auto_lock(CAF_THREADSIGNAL_MUTEX)

#define CAF_THREADSIGNAL_LOCK_UNLOCK_LOG \
	Caf::CAutoMutexLockUnlock _threadsync_auto_lock(CAF_THREADSIGNAL_MUTEX, CAF_CM_GET_CLASSNAME, CAF_CM_GET_FUNCNAME)

class COMMONAGGREGATOR_LINKAGE CThreadSignal {
public:
	CThreadSignal();
	virtual ~CThreadSignal();

public:
	void initialize(const std::string& conditionName);
	bool isInitialized() const;
	void signal();
	void wait(SmartPtrCAutoMutex& mutex, const uint32 timeoutMs);
	bool waitOrTimeout(SmartPtrCAutoMutex& mutex, const uint32 timeoutMs);
	std::string getName() const;
	void close();

private:
	bool _isInitialized;
	CAutoCondition _condition;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CThreadSignal);
};

CAF_DECLARE_SMART_POINTER( CThreadSignal);

}

#endif // #ifndef CThreadSignal_h_
