/*
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CManagedThreadPool.h"
#include "amqpClient/ConsumerWorkService.h"

using namespace Caf::AmqpClient;

ConsumerWorkService::ConsumerWorkService() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("ConsumerWorkService") {
}

ConsumerWorkService::~ConsumerWorkService() {
	CAF_CM_FUNCNAME("~ConsumerWorkService");
	try {
		if (_threadPool) {
			_threadPool->term();
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

void ConsumerWorkService::init(const SmartPtrCManagedThreadPool& threadPool) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_threadPool);
	_threadPool = threadPool;
}

void ConsumerWorkService::addWork(const CManagedThreadPool::SmartPtrIThreadTask& task) {
	CAF_CM_FUNCNAME_VALIDATE("addWork");
	CAF_CM_PRECOND_ISINITIALIZED(_threadPool);
	_threadPool->enqueue(task);
}

void ConsumerWorkService::notifyConnectionClosed() {
	if (!_threadPool.IsNull()) {
		_threadPool->term();
		_threadPool = NULL;
	}
}
