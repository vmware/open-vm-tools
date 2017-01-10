/*
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMANAGEDTHREADPOOL_H_
#define CMANAGEDTHREADPOOL_H_


#include "ICafObject.h"

namespace Caf {

/**
 * @author mdonahue
 * @brief This class wraps a GThreadPool and makes it a bit more friendly to use. It
 * also allows tasks to partially complete and be requeued.
 * The shutdown behavior is to wait for all active tasks to finish. Inactive
 * (unscheduled) tasks will be aborted.
 */
class COMMONAGGREGATOR_LINKAGE CManagedThreadPool {
public:
	/**
	 * @brief Interface for task objects
	 */
	struct __declspec(novtable) IThreadTask : public ICafObject {
		/**
		 * @brief execute task
		 * @retval true the task has completed its work and will be removed from the pool
		 * @retval false the task has not completed its work and will be requeued in the pool
		 */
		virtual bool run() = 0;
	};
	CAF_DECLARE_SMART_INTERFACE_POINTER(IThreadTask);
	typedef std::deque<SmartPtrIThreadTask> TaskDeque;

public:
	CManagedThreadPool();
	virtual ~CManagedThreadPool();

	/**
	 * @brief initialize the thread pool
	 * @param poolName a friendly name for the pool to aid in debugging
	 * @param threadCount the number of task threads
	 * @param taskUpdateInterval optional task queue refresh rate in milliseconds
	 */
	void init(
			const std::string& poolName,
			uint32 threadCount,
			uint32 taskUpdateInterval = 0);

	/**
	 * @brief terminate the thread pool
	 * All active tasks will be allowed to finish before this method returns
	 */
	void term();

	/**
	 * @brief add a task to the pool
	 * @param task the task to add
	 */
	void enqueue(const SmartPtrIThreadTask& task);

	/**
	 * @brief add a collection of tasks to the pool
	 * @param tasks the tasks to add
	 */
	void enqueue(const TaskDeque& tasks);

	/** @brief A simple structure to report some statistics
	 *
	 */
	struct Stats {
		/** The number of tasks under management */
		uint32 taskCount;

		/** The number of tasks waiting to be assigned to threads for execution */
		uint32 inactiveTaskCount;

		/** The number of tasks assigned to threads for execution */
		uint32 activeTaskCount;

		/** The number of tasks that have completed execution */
		uint32 completeTaskCount;

		/** The number of tasks that have executed but need to be requeued */
		uint32 incompleteTaskCount;
	};

	/** @return the current statistics */
	Stats getStats() const;

private:
	static gpointer poolWorkerFunc(gpointer context);
	static void taskWorkerFunc(gpointer threadContext, gpointer NOT_USED);

private:
	void runPool();

private:
	/** Default task update interval in milliseconds */
	static uint32 DEFAULT_TASK_UPDATE_INTERVAL;

private:
	bool _isInitialized;
	volatile bool _isShuttingDown;
	std::string _poolName;
	GThreadPool *_threadPool;
	typedef std::set<IThreadTask*> TaskSet;
	TaskSet _tasks;
	GThread* _workerThread;
	uint32 _taskUpdateInterval;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CManagedThreadPool);
};
CAF_DECLARE_SMART_POINTER(CManagedThreadPool);

}

#endif /* CMANAGEDTHREADPOOL_H_ */
