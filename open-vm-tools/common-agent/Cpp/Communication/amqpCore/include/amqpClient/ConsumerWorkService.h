/*
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CONSUMERWORKSERVICE_H_
#define CONSUMERWORKSERVICE_H_


#include "Common/CManagedThreadPool.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief This class wraps a thread pool for executing channel worker threads
 */
class ConsumerWorkService {
public:
	ConsumerWorkService();
	virtual ~ConsumerWorkService();

	/**
	 * @brief Initializer
	 * @param threadPool the thread pool used to execute channel threads
	 */
	void init(const SmartPtrCManagedThreadPool& threadPool);

	/**
	 * @brief Add a worker thread to the pool
	 * @param task the task to add
	 */
	void addWork(const CManagedThreadPool::SmartPtrIThreadTask& task);

	/**
	 * @brief Respond to a connection-closed notification by terminating the thread pool
	 */
	void notifyConnectionClosed();

private:
	bool _isInitialized;
	SmartPtrCManagedThreadPool _threadPool;
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(ConsumerWorkService);
};
CAF_DECLARE_SMART_POINTER(ConsumerWorkService);

}}

#endif /* CONSUMERWORKSERVICE_H_ */
