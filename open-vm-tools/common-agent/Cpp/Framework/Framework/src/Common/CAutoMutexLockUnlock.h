/*
 *	 Author: mdonahue
 *  Created: Jan 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAUTOMUTEXLOCKUNLOCK_H_
#define CAUTOMUTEXLOCKUNLOCK_H_


#include "Common/CAutoMutex.h"
#include "Common/CAutoRecMutex.h"

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CAutoMutexLockUnlock {
public:
	CAutoMutexLockUnlock(
			SmartPtrCAutoMutex& mutex,
			const char* className = NULL,
			const char* funcName = NULL,
			const int32 lineNumber = 0);

	CAutoMutexLockUnlock(
			SmartPtrCAutoRecMutex& recMutex,
			const char* className = NULL,
			const char* funcName = NULL,
			const int32 lineNumber = 0);

	~CAutoMutexLockUnlock();

private:
	SmartPtrCAutoMutex _mutex;
	SmartPtrCAutoRecMutex _recMutex;

	std::string _className;
	std::string _funcName;
	int32 _lineNumber;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CAutoMutexLockUnlock);
};
}

#endif /* CAUTOMUTEXLOCKUNLOCK_H_ */
