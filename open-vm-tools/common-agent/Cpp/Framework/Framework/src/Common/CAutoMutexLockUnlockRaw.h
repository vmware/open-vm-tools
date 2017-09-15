/*
 *	 Author: mdonahue
 *  Created: Jan 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAUTOMUTEXLOCKUNLOCKRAW_H_
#define CAUTOMUTEXLOCKUNLOCKRAW_H_

namespace Caf {

class SUBSYSTEMBASE_LINKAGE CAutoMutexLockUnlockRaw {
public:
	CAutoMutexLockUnlockRaw(GMutex* mutex);

	CAutoMutexLockUnlockRaw(GRecMutex* recMutex);

	~CAutoMutexLockUnlockRaw();

private:
	GMutex* _mutex;
	GRecMutex* _recMutex;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CAutoMutexLockUnlockRaw);
};

}

#endif /* CAUTOMUTEXLOCKUNLOCKRAW_H_ */
