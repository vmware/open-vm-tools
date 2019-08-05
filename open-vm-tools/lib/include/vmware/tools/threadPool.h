/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

/**
 * @file threadPool.h
 *
 * Public interface for vmtoolsd's thread pool.
 *
 * @defgroup vmtools_threads  Threading
 * @brief Thread Pooling and Monitoring
 * @{
 *
 * vmtoolsd provides a worker thread pool for use by plugins. This pool is
 * shared among all plugins, and is configurable from the Tools config file.
 * Plugins can submit tasks to the thread pool by using one of the inline
 * functions declared in this header.
 *
 * The thread pool is a shared resource, so code whose execution time may be
 * very long might want to, instead, create a dedicated thread for execution.
 * The shared thread pool also provides a facility to more easily do that,
 * with the lifecycle of the new thread managed by the thread pool so that it
 * is properly notified of service shutdown.
 *
 * Finally, depending on the configuration, the shared thread pool might not
 * be a thread pool at all: if the configuration has disabled threading, tasks
 * destined to the shared thread pool will be executed on the main service
 * thread.
 */

#include <glib-object.h>
#include "vmware/tools/plugin.h"

#define TOOLS_CORE_PROP_TPOOL "tcs_prop_thread_pool"

/** Type of callback function used to register tasks with the pool. */
typedef void (*ToolsCorePoolCb)(ToolsAppCtx *ctx,
                                gpointer data);

/**
 * @brief Public interface of the shared thread pool.
 *
 * This struct is published in the service's TOOLS_CORE_PROP_TPOOL property,
 * and contains function pointers to the internal implementation of the
 * thread pool's functions. In general, applications may prefer to use the
 * inline functions provided below instead, since they take care of some of
 * the boilerplate code.
 */
typedef struct ToolsCorePool {
   guint (*submit)(ToolsAppCtx *ctx,
                   ToolsCorePoolCb cb,
                   gpointer data,
                   GDestroyNotify dtor);
   void (*cancel)(guint id);
   gboolean (*start)(ToolsAppCtx *ctx,
                     const gchar *threadName,
                     ToolsCorePoolCb cb,
                     ToolsCorePoolCb interrupt,
                     gpointer data,
                     GDestroyNotify dtor);
} ToolsCorePool;


/*
 *******************************************************************************
 * ToolsCorePool_GetPool --                                               */ /**
 *
 * @brief Returns the thread pool instance for the service.
 *
 * @param[in] ctx Application context.
 *
 * @return The thread pool instance, or NULL if it's not available.
 *
 *******************************************************************************
 */

G_INLINE_FUNC ToolsCorePool *
ToolsCorePool_GetPool(ToolsAppCtx *ctx)
{
   ToolsCorePool *pool = NULL;
   g_object_get(ctx->serviceObj, TOOLS_CORE_PROP_TPOOL, &pool, NULL);
   return pool;
}


/*
 *******************************************************************************
 * ToolsCorePool_SubmitTask --                                            */ /**
 *
 * @brief Submits a task for execution in the thread pool.
 *
 * The task is queued in the thread pool and will be executed as soon as a
 * worker thread is available. If the thread pool is disabled, the task will
 * be executed on the main service thread as soon as the main loop is idle.
 *
 * The task data's destructor will be called after the task finishes executing,
 * or in case the thread pool is destroyed before the task is executed.
 *
 * @param[in] ctx    Application context.
 * @param[in] cb     Function to execute the task.
 * @param[in] data   Opaque data for the task.
 * @param[in] dtor   Destructor for the task data.
 *
 * @return An identifier for the task, or 0 on error.
 *
 *******************************************************************************
 */

G_INLINE_FUNC guint
ToolsCorePool_SubmitTask(ToolsAppCtx *ctx,
                         ToolsCorePoolCb cb,
                         gpointer data,
                         GDestroyNotify dtor)
{
   ToolsCorePool *pool = ToolsCorePool_GetPool(ctx);
   if (pool != NULL) {
      return pool->submit(ctx, cb, data, dtor);
   }
   return 0;
}


/*
 *******************************************************************************
 * ToolsCorePool_CancelTask --                                            */ /**
 *
 * @brief Cancels a task previously submitted to the pool.
 *
 * If the task is currently being executed, this function does nothing.
 * Otherwise, the task is removed from the task queue, and its destructor
 * (if any) is called.
 *
 * @param[in] ctx    Application context.
 * @param[in] taskId Task ID returned by ToolsCorePool_SubmitTask().
 *
 *******************************************************************************
 */

G_INLINE_FUNC void
ToolsCorePool_CancelTask(ToolsAppCtx *ctx,
                         guint taskId)
{
   ToolsCorePool *pool = ToolsCorePool_GetPool(ctx);
   if (pool != NULL) {
      pool->cancel(taskId);
   }
}


/*
 *******************************************************************************
 * ToolsCorePool_StartThread --                                           */ /**
 *
 * @brief Starts a task on its own thread.
 *
 * This function will run a task on a dedicated thread that is not part of
 * the shared thread pool. The thread will be managed by the thread pool, so
 * that it's properly cleaned up when the service is shutting down.
 *
 * Threads started by this function cannot be stopped by using the cancel
 * function. Instead, if the application itself wants to stop the thread, it
 * should call the interrupt function it provided to the thread pool, or use
 * some other method of communicating with the thread.
 *
 * @param[in] ctx          Application context.
 * @param[in] threadName   Name for the new thread.
 * @param[in] cb           Function that implements the task to execute.
 * @param[in] interrupt    A function that will request the task to be
 *                         interrupted. This will be called when the pool
 *                         needs to stop all managed threads (e.g. during
 *                         service shutdown). The task should stop what it's
 *                         doing and end the thread soon after this callback
 *                         is fired.
 * @param[in] data         Opaque data for both task callback and interrupt
 *                         functions.
 * @param[in] dtor         Destructor for the task data.
 *
 * @return TRUE iff thread was successfully started.
 *
 *******************************************************************************
 */

G_INLINE_FUNC gboolean
ToolsCorePool_StartThread(ToolsAppCtx *ctx,
                          const gchar *threadName,
                          ToolsCorePoolCb cb,
                          ToolsCorePoolCb interrupt,
                          gpointer data,
                          GDestroyNotify dtor)
{
   ToolsCorePool *pool = ToolsCorePool_GetPool(ctx);
   if (pool != NULL) {
      return pool->start(ctx, threadName, cb, interrupt, data, dtor);
   }
   return FALSE;
}

/** @} */

#endif /* _THREADPOOL_H_ */

