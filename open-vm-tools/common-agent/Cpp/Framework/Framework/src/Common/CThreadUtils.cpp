/*
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "CThreadUtils.h"

using namespace Caf;

uint32 CThreadUtils::getThreadStackSizeKb() {
	CAF_CM_STATIC_FUNC_LOG_ONLY("CThreadUtils", "getThreadStackSizeKb");

	uint32 stackSizeKb =
		AppConfigUtils::getRequiredUint32(_sAppConfigGlobalThreadStackSizeKb);

#ifdef __linux__
	// The thread stack size is the larger of
	// PTHREAD_STACK_MIN, 256K and the config file value
	stackSizeKb = std::max((int32)stackSizeKb, std::max(PTHREAD_STACK_MIN, (256 * 1024)) / 1024);
#endif
	CAF_CM_LOG_DEBUG_VA1("thread_stack_size_kb=%d", stackSizeKb);
	return stackSizeKb;
}

GThread* CThreadUtils::startJoinable(threadFunc func, void* data) {
	CAF_CM_STATIC_FUNC("CThreadUtils", "startJoinable");

	GThread *rc = g_thread_new("CThreadUtils::startJoinable", func, data);
	if(NULL == rc) {
		CAF_CM_EXCEPTION_VA0(0, "g_thread_new Failed");
	}

	return rc;
}

void CThreadUtils::join(GThread* thread) {
	(void) g_thread_join(thread);
}

void CThreadUtils::sleep(
	const uint32 milliseconds) {

	CAF_CM_ENTER {
		const int32 microseconds = milliseconds * 1000;
		g_usleep(microseconds);
	}
	CAF_CM_EXIT;
}
