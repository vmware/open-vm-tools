/*
 *	 Author: mdonahue
 *  Created: Jan 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CAutoMutexLockUnlockRaw.h"

using namespace Caf;

CAutoMutexLockUnlockRaw::CAutoMutexLockUnlockRaw(GMutex* mutex) :
	CAF_CM_INIT("CAutoMutexLockUnlockRaw") {
	_mutex = mutex;
	_recMutex = NULL;

	if (_mutex) {
		::g_mutex_lock(_mutex);
	}
}

CAutoMutexLockUnlockRaw::CAutoMutexLockUnlockRaw(GRecMutex* recMutex) :
	CAF_CM_INIT("CAutoMutexLockUnlockRaw") {
	_mutex = NULL;
	_recMutex = recMutex;

	if (_recMutex) {
		::g_rec_mutex_lock(_recMutex);
	}
}

CAutoMutexLockUnlockRaw::~CAutoMutexLockUnlockRaw() {
	if (_mutex) {
		::g_mutex_unlock(_mutex);
	}
	if (_recMutex) {
		::g_rec_mutex_unlock(_recMutex);
	}
}
