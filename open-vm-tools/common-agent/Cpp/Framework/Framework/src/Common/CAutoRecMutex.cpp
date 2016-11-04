/*
 *	 Author: bwilliams
 *  Created: Oct 29, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CAutoRecMutex.h"

using namespace Caf;

CAutoRecMutex::CAutoRecMutex() :
	_isInitialized(false),
	CAF_CM_INIT("CAutoRecMutex") {
	::g_rec_mutex_init(&_mutex);
}

CAutoRecMutex::~CAutoRecMutex() {
	::g_rec_mutex_clear(&_mutex);
}

void CAutoRecMutex::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_isInitialized = true;
}

bool CAutoRecMutex::isInitialized() const {
	return _isInitialized;
}

void CAutoRecMutex::lock(
	const char* className,
	const char* funcName,
	const int32 lineNumber) {
	CAF_CM_FUNCNAME_VALIDATE("lock");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

//	if ((NULL != className) && (NULL != funcName)) {
//		CAF_CM_LOG_DEBUG_VA4("Waiting for lock - %s::%s(%d) - %p", className, funcName,
//			lineNumber, &_mutex);
//	}

	::g_rec_mutex_lock(&_mutex);

//	if ((NULL != className) && (NULL != funcName)) {
//		CAF_CM_LOG_DEBUG_VA4("Got lock - %s::%s(%d) - %p", className, funcName, lineNumber,
//			&_mutex);
//	}

}

void CAutoRecMutex::unlock(
	const char* className,
	const char* funcName,
	const int32 lineNumber) {
	CAF_CM_FUNCNAME_VALIDATE("unlock");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

//	if ((NULL != className) && (NULL != funcName)) {
//		CAF_CM_LOG_DEBUG_VA4("Unlocking lock - %s::%s(%d) - %p", className, funcName,
//			lineNumber, &_mutex);
//	}

	::g_rec_mutex_unlock(&_mutex);
}

GRecMutex* CAutoRecMutex::getNonConstPtr() {
	CAF_CM_FUNCNAME_VALIDATE("getNonConstPtr");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return &_mutex;
}
