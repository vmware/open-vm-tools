/*
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CManagedThreadPool.h"
#include "Exception/CCafException.h"

namespace Caf {

class TaskWrapper : public CManagedThreadPool::IThreadTask {
public:
	typedef enum {
		StateInactive,
		StateActive,
		StateFinishedComplete,
		StateFinishedIncomplete
	} EnumState;

public:
	TaskWrapper();
	virtual ~TaskWrapper();

	void init(const CManagedThreadPool::SmartPtrIThreadTask& task);

	void setState(EnumState state);

	EnumState getState() const;

public: // IThreadTask
	bool run();

private:
	CManagedThreadPool::SmartPtrIThreadTask _task;
	EnumState _state;

private:
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(TaskWrapper);

};
CAF_DECLARE_SMART_POINTER(TaskWrapper);

}

using namespace Caf;

uint32 CManagedThreadPool::DEFAULT_TASK_UPDATE_INTERVAL = 333;

CManagedThreadPool::CManagedThreadPool() :
	_isInitialized(false),
	_isShuttingDown(false),
	_threadPool(NULL),
	_workerThread(NULL),
	_taskUpdateInterval(DEFAULT_TASK_UPDATE_INTERVAL),
	CAF_CM_INIT_LOG("CManagedThreadPool") {
	CAF_CM_INIT_THREADSAFE;
}

CManagedThreadPool::~CManagedThreadPool() {
	CAF_CM_FUNCNAME_VALIDATE("~CManagedThreadPool");

	if (_threadPool || _workerThread || _tasks.size()) {
		const char* poolName = _poolName.empty() ? "<uninitialized>" : _poolName.c_str();
		CAF_CM_LOG_ERROR_VA1(
				"[poolName=%s] Destroying thread pool but it is "
				"still active. You really should call term() first.",
				poolName);

		CAF_CM_LOG_DEBUG_VA1("[poolName=%s] Setting shutdown flag", poolName);
		_isShuttingDown = true;

		if (_workerThread) {
			CAF_CM_LOG_DEBUG_VA1("[poolName=%s] Waiting for worker thread to stop", poolName);
			g_thread_join(_workerThread);
		}

		if (_threadPool) {
			CAF_CM_LOG_DEBUG_VA1("[poolName=%s] Waiting for thread pool to stop", poolName);
			g_thread_pool_free(
					_threadPool,
					TRUE, /* immediate - do not schedule any more tasks to threads */
					TRUE  /* wait - wait for currently running tasks to finish */ );
		}

		CAF_CM_LOG_DEBUG_VA2(
				"[poolName=%s] Pool has shut down.  Releasing %d tasks",
				poolName,
				_tasks.size());
		for (TConstIterator<TaskSet> task(_tasks); task; task++) {
			(*task)->Release();
		}
	}
}

void CManagedThreadPool::init(
		const std::string& poolName,
		uint32 threadCount,
		uint32 taskUpdateInterval) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_STRING(poolName);
	CAF_CM_VALIDATE_NOTZERO(threadCount);

	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_poolName = poolName;
	if (taskUpdateInterval) {
		_taskUpdateInterval = taskUpdateInterval;
	}

	GError *error = NULL;
	_threadPool = g_thread_pool_new(
			taskWorkerFunc,
			NULL,
			threadCount,
			TRUE,
			&error);
	if (error) {
		CAF_CM_THROW_GERROR(error);
	}
	_workerThread = CThreadUtils::startJoinable(
			poolWorkerFunc,
			this);
	_isInitialized = true;
}

void CManagedThreadPool::term() {
	CAF_CM_FUNCNAME("term");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_LOCK_UNLOCK;

	if (g_thread_self() == _workerThread) {
		CAF_CM_EXCEPTIONEX_VA1(IllegalStateException, ERROR_INVALID_STATE, "Must terminate the worker thread from a different thread - %p", _workerThread);
	}

	CAF_CM_LOG_DEBUG_VA1("[poolName=%s] Setting shutdown flag", _poolName.c_str());
	_isShuttingDown = true;

	CAF_CM_LOG_DEBUG_VA2("[poolName=%s] Waiting for worker thread to stop - workerThread: %p", _poolName.c_str(), _workerThread);
	GThread* workerThread = _workerThread;
	{
		CAF_CM_UNLOCK_LOCK;
		g_thread_join(workerThread);
	}
	_workerThread = NULL;

	CAF_CM_LOG_DEBUG_VA1("[poolName=%s] Waiting for thread pool to stop", _poolName.c_str());
	GThreadPool* threadPool = _threadPool;
	{
		CAF_CM_UNLOCK_LOCK;
		g_thread_pool_free(
				threadPool,
				TRUE, /* immediate - do not schedule any more tasks to threads */
				TRUE  /* wait - wait for currently running tasks to finish */ );
	}
	_threadPool = NULL;

	CAF_CM_LOG_DEBUG_VA2(
			"[poolName=%s] Pool has shut down.  Releasing %d tasks",
			_poolName.c_str(),
			_tasks.size());
	const TaskSet tasks = _tasks;
	{
		CAF_CM_UNLOCK_LOCK;
		for (TConstIterator<TaskSet> task(tasks); task; task++) {
			(*task)->Release();
		}
	}
	_tasks.clear();
}

void CManagedThreadPool::enqueue(const SmartPtrIThreadTask& task) {
	CAF_CM_FUNCNAME("enqueue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(task);
	CAF_CM_LOCK_UNLOCK;
	SmartPtrTaskWrapper taskWrapper;
	taskWrapper.CreateInstance();
	taskWrapper->init(task);
	if (!_tasks.insert(taskWrapper.GetAddRefedInterface()).second) {
		// This should not be possible!
		taskWrapper->Release();
		CAF_CM_EXCEPTIONEX_VA1(
				DuplicateElementException,
				0,
				"[poolName=%s] An attempt was made to add a task object with an "
				"address equal to that of an existing object. "
				"This should not be possible. Please report this bug.",
				_poolName.c_str());
	}
}

void CManagedThreadPool::enqueue(const TaskDeque& tasks) {
	CAF_CM_FUNCNAME("enqueue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_LOCK_UNLOCK;
	for (TSmartConstIterator<TaskDeque> task(tasks); task; task++) {
		SmartPtrTaskWrapper taskWrapper;
		taskWrapper.CreateInstance();
		taskWrapper->init(*task);
		if (!_tasks.insert(taskWrapper.GetAddRefedInterface()).second) {
			// This should not be possible!
			taskWrapper->Release();
			CAF_CM_EXCEPTIONEX_VA1(
					DuplicateElementException,
					0,
					"[poolName=%s] An attempt was made to add a task object with an "
					"address equal to that of an existing object. "
					"This should not be possible. Please report this bug.",
					_poolName.c_str());
		}
	}
}

CManagedThreadPool::Stats CManagedThreadPool::getStats() const {
	CAF_CM_FUNCNAME_VALIDATE("getStats");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_LOCK_UNLOCK;
	Stats stats = { 0, 0, 0, 0, 0 };
	stats.taskCount = static_cast<uint32>(_tasks.size());
	for (TConstIterator<TaskSet> task(_tasks); task; task++) {
		TaskWrapper *taskWrapper = reinterpret_cast<TaskWrapper*>(*task);
		switch (taskWrapper->getState()) {
		case TaskWrapper::StateActive:
			++stats.activeTaskCount;
			break;

		case TaskWrapper::StateInactive:
			++stats.inactiveTaskCount;
			break;

		case TaskWrapper::StateFinishedComplete:
			++stats.completeTaskCount;
			break;

		case TaskWrapper::StateFinishedIncomplete:
			++stats.incompleteTaskCount;
			break;
		}
	}
	return stats;
}

gpointer CManagedThreadPool::poolWorkerFunc(gpointer context) {
	CAF_CM_STATIC_FUNC_LOG("CManagedThreadPool", "poolWorkerFunc");
	try {
		CAF_CM_VALIDATE_PTR(context);
		CManagedThreadPool *pool = reinterpret_cast<CManagedThreadPool*>(context);
		pool->runPool();
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
	return NULL;
}

void CManagedThreadPool::runPool() {
	CAF_CM_FUNCNAME("runPool");
	CAF_CM_LOCK_UNLOCK;

	CAF_CM_LOG_DEBUG_VA1("[poolName=%s] Starting runPool() thread", _poolName.c_str());
	while (!_isShuttingDown) {
		TaskSet tasksToRun;
		TaskSet tasksRemoved;

		try {
			// Move inactive tasks to the thread pool
			for (TConstIterator<TaskSet> task(_tasks);
					!_isShuttingDown && task;
					task++) {
				TaskWrapper* taskWrapper = reinterpret_cast<TaskWrapper*>(*task);
				if (TaskWrapper::StateInactive == taskWrapper->getState()) {
					taskWrapper->setState(TaskWrapper::StateActive);
					GError *error = NULL;
					g_thread_pool_push(
							_threadPool,
							taskWrapper,
							&error);
					if (error) {
						taskWrapper->setState(TaskWrapper::StateInactive);
						CAF_CM_LOG_ERROR_VA3(
								"[poolName=%s] Unable to add task to tread pool. "
								"[%d][%s]",
								_poolName.c_str(),
								error->code,
								error->message);
						g_error_free(error);
						error = NULL;
					}
				}
			}
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;

		try {
			// Handle finished tasks.  The tasks may switch from Active to
			// FinishedComplete or FinishedIncomplete while we are reading the
			// state.  That's okay - we'll just catch it on the next go-round.
			//
			// The alternative is to protect the state variable in the task wrapper
			// with a critical section.  Seems kind of expensive for something that
			// isn't really a problem or time-critical.
			//
			// Also using naked iterators here since I'm updating the task set.
			TaskSet::iterator task = _tasks.begin();
			while (!_isShuttingDown && (task != _tasks.end())) {
				TaskWrapper* taskWrapper = reinterpret_cast<TaskWrapper*>(*task);
				if (TaskWrapper::StateFinishedComplete == taskWrapper->getState()) {
					TaskSet::iterator toErase = task;
					++task;
					_tasks.erase(toErase);
					taskWrapper->Release();
				} else if (TaskWrapper::StateFinishedIncomplete == taskWrapper->getState()) {
					taskWrapper->setState(TaskWrapper::StateInactive);
					++task;
				} else {
					++task;
				}
			}
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;

		const uint32 taskUpdateInterval = _taskUpdateInterval;
		{
			CAF_CM_UNLOCK_LOCK;
			CThreadUtils::sleep(taskUpdateInterval);
		}
	}
	CAF_CM_LOG_DEBUG_VA1("[poolName=%s] Leaving runPool() thread", _poolName.c_str());
}

void CManagedThreadPool::taskWorkerFunc(gpointer threadContext, gpointer) {
	CAF_CM_STATIC_FUNC_LOG("TaskWorkerFunc", "taskWorkerFunc");
	// Don't want to segfault so I'll test threadContext even though
	// it *cannot* be NULL.  If it is we are in really bad shape!
	try {
		CAF_CM_VALIDATE_PTR(threadContext);

		TaskWrapper *task = reinterpret_cast<TaskWrapper*>(threadContext);
		bool complete = false;
		try {
			complete = task->run();
		}
		CAF_CM_CATCH_ALL;
		if (CAF_CM_ISEXCEPTION) {
			CAF_CM_LOG_CRIT_CAFEXCEPTION;
			CAF_CM_CLEAREXCEPTION;
			task->setState(TaskWrapper::StateFinishedComplete);
		} else {
			task->setState(complete ?
					TaskWrapper::StateFinishedComplete :
					TaskWrapper::StateFinishedIncomplete);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

TaskWrapper::TaskWrapper() :
	_state(StateInactive) {
	CAF_CM_INIT_THREADSAFE;
}

TaskWrapper::~TaskWrapper() {
}

void TaskWrapper::init(const CManagedThreadPool::SmartPtrIThreadTask& task) {
	_task = task;
}

void TaskWrapper::setState(EnumState state) {
	CAF_CM_LOCK_UNLOCK;
	_state = state;
}

TaskWrapper::EnumState TaskWrapper::getState() const {
	CAF_CM_LOCK_UNLOCK;
	return _state;
}

bool TaskWrapper::run() {
	return _task->run();
}
