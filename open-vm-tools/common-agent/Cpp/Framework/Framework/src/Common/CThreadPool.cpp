/*
 * CThreadPool.cpp
 *
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CManagedThreadPool.h"
#include "Common/CThreadPool.h"
#include "Exception/CCafException.h"

	bool _isInitialized;
	GThreadPool *_threadPool;
	gpointer _userData;

CThreadPool::CThreadPool() :
	_isInitialized(false),
	_threadPool(NULL),
	_userData(NULL),
	CAF_CM_INIT("CThreadPool") {
	CAF_CM_INIT_THREADSAFE;
}

CThreadPool::~CThreadPool() {
	if (_threadPool) {
		g_thread_pool_free(_threadPool, FALSE, TRUE);
	}
}

void CThreadPool::init(gpointer userData, gint maxThreads) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	GError *error = NULL;
	_threadPool = g_thread_pool_new(
			ThreadFunc,
			userData,
			maxThreads,
			TRUE,
			&error);
	if (error) {
		CAF_CM_THROW_GERROR(error);
	}
	_isInitialized = true;
}

void CThreadPool::term() {
	CAF_CM_FUNCNAME_VALIDATE("term");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	g_thread_pool_free(_threadPool, FALSE, TRUE);
	_threadPool = NULL;
}

void CThreadPool::addTask(const SmartPtrIThreadTask& task) {
	CAF_CM_FUNCNAME("addTask");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(task);

	if (_threadPool) {
		GError *error = NULL;
		g_thread_pool_push(
				_threadPool,
				task.GetAddRefedInterface(),
				&error);
		if (error) {
			task->Release();
			CAF_CM_THROW_GERROR(error);
		}
	} else {
		CAF_CM_EXCEPTIONEX_VA0(
				IllegalStateException,
				0,
				"The thread pool has been shut down");
	}
}

void CThreadPool::ThreadFunc(gpointer data, gpointer userData) {
	CAF_CM_STATIC_FUNC_LOG("CThreadPool", "ThreadFunc");
	CAF_CM_VALIDATE_PTR(data);
	// userData is optional
	IThreadTask *task = reinterpret_cast<IThreadTask*>(data);
	try {
		task->run(userData);
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
	task->Release();
}
