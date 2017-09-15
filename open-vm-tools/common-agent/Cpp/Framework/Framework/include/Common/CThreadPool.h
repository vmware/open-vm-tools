/*
 * CThreadPool.h
 *
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CTHREADPOOL_H_
#define CTHREADPOOL_H_


#include "ICafObject.h"

#include "Common/CManagedThreadPool.h"

namespace Caf {

/**
 * @author mdonahue
 * @brief This class wraps a GThreadPool and makes it a bit more friendly to use.
 * This class wraps up a GThreadPool as a lifetime-managed object that can be shared.
 * The shutdown behavior is to wait for all tasks to finish.
 */
class COMMONAGGREGATOR_LINKAGE CThreadPool {
public:
	/**
	 * @brief This is the interface for tasks queued in this thread pool
	 */
	struct __declspec(novtable) IThreadTask : public ICafObject {
		/**
		 * @brief execute task
		 * @param userData the userData passed into the thread pool init() method
		 */
		virtual void run(gpointer userData) = 0;
	};
	CAF_DECLARE_SMART_INTERFACE_POINTER(IThreadTask);

public:
	CThreadPool();
	virtual ~CThreadPool();

	/**
	 * @brief Initialize the thread pool
	 * @param userData opaque data to be passed to each thread
	 * @param maxThreads the number of threads to create
	 */
	void init(gpointer userData, gint maxThreads);

	/**
	 * @brief Terminate the thread pool
	 * All tasks will be allowed to finish before this method returns
	 */
	void term();

	/**
	 * @brief Add a task to the thread pool
	 * @param task interface to the task to add
	 */
	void addTask(const SmartPtrIThreadTask& task);

private:
	static void ThreadFunc(gpointer data, gpointer userData);

private:
	bool _isInitialized;
	GThreadPool *_threadPool;
	gpointer _userData;
	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CThreadPool);
};
CAF_DECLARE_SMART_POINTER(CThreadPool);

}

#endif /* CTHREADPOOL_H_ */
