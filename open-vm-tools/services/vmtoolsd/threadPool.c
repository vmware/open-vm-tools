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

/**
 * @file threadPool.c
 *
 * Implementation of the shared thread pool defined in threadPool.h.
 */

#include <limits.h>
#include <string.h>
#include "vmware.h"
#include "toolsCoreInt.h"
#include "serviceObj.h"
#include "vmware/tools/threadPool.h"

#define DEFAULT_MAX_IDLE_TIME       5000
#define DEFAULT_MAX_THREADS         5
#define DEFAULT_MAX_UNUSED_THREADS  0

typedef struct ThreadPoolState {
   ToolsCorePool  funcs;
   gboolean       active;
   ToolsAppCtx   *ctx;
   GThreadPool   *pool;
   GQueue        *workQueue;
   GPtrArray     *threads;
   GMutex         lock;
   guint          nextWorkId;
} ThreadPoolState;


typedef struct WorkerTask {
   guint             id;
   guint             srcId;
   ToolsCorePoolCb   cb;
   gpointer          data;
   GDestroyNotify    dtor;
} WorkerTask;


typedef struct StandaloneTask {
   gboolean          active;
   ToolsCorePoolCb   cb;
   ToolsCorePoolCb   interrupt;
   gpointer          data;
   GThread          *thread;
   GDestroyNotify    dtor;
} StandaloneTask;


static ThreadPoolState gState;


/*
 *******************************************************************************
 * ToolsCorePoolCompareTask --                                            */ /**
 *
 * Compares two WorkerTask instances.
 *
 * @param[in] p1  Pointer to WorkerTask.
 * @param[in] p2  Pointer to WorkerTask.
 *
 * @return > 0, 0, < 0  if p1's ID is less than, equal, or greater than p2's.
 *
 *******************************************************************************
 */

static gint
ToolsCorePoolCompareTask(gconstpointer p1,
                         gconstpointer p2)
{
   const WorkerTask *t1 = p1;
   const WorkerTask *t2 = p2;

   if (t1 != NULL && t2 != NULL) {
      return (t2->id - t1->id);
   }

   if (t1 == NULL && t2 == NULL) {
      return 0;
   }

   return (t1 != NULL) ? -1 : 1;
}


/*
 *******************************************************************************
 * ToolsCorePoolDestroyThread --                                          */ /**
 *
 * Releases resources associated with a StandaloneTask, joining the thread
 * that's executing it.
 *
 * @param[in] data   A StandaloneTask.
 *
 *******************************************************************************
 */

static void
ToolsCorePoolDestroyThread(gpointer data)
{
   StandaloneTask *task = data;
   g_thread_join(task->thread);
   if (task->dtor != NULL) {
      task->dtor(task->data);
   }
   g_free(task);
}


/*
 *******************************************************************************
 * ToolsCorePoolDestroyTask --                                            */ /**
 *
 * Frees memory associated with a WorkerTask, calling its destructor if one is
 * registered.
 *
 * @param[in] data   A WorkerTask.
 *
 *******************************************************************************
 */

static void
ToolsCorePoolDestroyTask(gpointer data)
{
   WorkerTask *work = data;
   if (work->dtor != NULL) {
      work->dtor(work->data);
   }
   g_free(work);
}


/*
 *******************************************************************************
 * ToolsCorePoolDoWork --                                                 */ /**
 *
 * Execute a work item.
 *
 * @param[in] data   A WorkerTask.
 *
 * @return FALSE
 *
 *******************************************************************************
 */

static gboolean
ToolsCorePoolDoWork(gpointer data)
{
   WorkerTask *work = data;

   /*
    * In single threaded mode, remove the task being executed from the queue.
    * In multi-threaded mode, the thread pool callback already did this.
    */
   if (gState.pool == NULL) {
      g_mutex_lock(&gState.lock);
      g_queue_remove(gState.workQueue, work);
      g_mutex_unlock(&gState.lock);
   }

   work->cb(gState.ctx, work->data);
   return FALSE;
}


/*
 *******************************************************************************
 * ToolsCorePoolNoOp --                                                   */ /**
 *
 * Idle callback for destroying a standalone thread. Does nothing, since the
 * actual destruction is done by ToolsCorePoolDestroyThread.
 *
 * @param[in] data   Unused.
 *
 * @return FALSE
 *
 *******************************************************************************
 */

static gboolean
ToolsCorePoolNoOp(gpointer data)
{
   return FALSE;
}


/*
 *******************************************************************************
 * ToolsCorePoolRunThread --                                              */ /**
 *
 * Standalone thread runner. Executes the task associated with the thread, and
 * schedule a task to clean up the thread state when done.
 *
 * @param[in] data   A StandaloneTask.
 *
 * @return NULL
 *
 *******************************************************************************
 */

static gpointer
ToolsCorePoolRunThread(gpointer data)
{
   StandaloneTask *task = data;

   task->cb(gState.ctx, task->data);
   task->active = FALSE;

   g_mutex_lock(&gState.lock);
   /* If not active, the shutdown function will clean things up. */
   if (gState.active) {
      g_ptr_array_remove(gState.threads, task);
      g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                      ToolsCorePoolNoOp,
                      task,
                      ToolsCorePoolDestroyThread);
   }
   g_mutex_unlock(&gState.lock);

   return NULL;
}


/*
 *******************************************************************************
 * ToolsCorePoolRunWorker --                                              */ /**
 *
 * Thread pool callback function. Dequeues the next work item from the work
 * queue and execute it.
 *
 * @param[in] state        Description of state.
 * @param[in] clientData   Description of clientData.
 *
 *******************************************************************************
 */

static void
ToolsCorePoolRunWorker(gpointer state,
                       gpointer clientData)
{
   WorkerTask *work;

   g_mutex_lock(&gState.lock);
   work = g_queue_pop_tail(gState.workQueue);
   g_mutex_unlock(&gState.lock);

   ASSERT(work != NULL);

   ToolsCorePoolDoWork(work);
   ToolsCorePoolDestroyTask(work);
}


/*
 *******************************************************************************
 * ToolsCorePoolSubmit --                                                 */ /**
 *
 * Submits a new task for execution in one of the shared worker threads.
 *
 * @see ToolsCorePool_SubmitTask()
 *
 * @param[in] ctx    Application context.
 * @param[in] cb     Function to execute the task.
 * @param[in] data   Opaque data for the task.
 * @param[in] dtor   Destructor for the task data.
 *
 * @return New task's ID, or 0 on error.
 *
 *******************************************************************************
 */

static guint
ToolsCorePoolSubmit(ToolsAppCtx *ctx,
                    ToolsCorePoolCb cb,
                    gpointer data,
                    GDestroyNotify dtor)
{
   guint id = 0;
   WorkerTask *task = g_malloc0(sizeof *task);

   task->srcId = 0;
   task->cb = cb;
   task->data = data;
   task->dtor = dtor;

   g_mutex_lock(&gState.lock);

   if (!gState.active) {
      g_free(task);
      goto exit;
   }

   /*
    * XXX: a reeeeeeeeeally long running task could cause clashes (e.g., reusing
    * the same task ID after the counter wraps). That shouldn't really happen in
    * practice (and is an abuse of the thread pool, and could cause issues if
    * someone sets the pool size to 0 or 1), but it might be good to have more
    * fail-safe code here.
    */
   if (gState.nextWorkId + 1 == UINT_MAX) {
      task->id = UINT_MAX;
      gState.nextWorkId = 0;
   } else {
      task->id = ++gState.nextWorkId;
   }

   id = task->id;

   /*
    * We always add the task to the queue, even in single threaded mode, so
    * that it can be canceled. In single threaded mode, it's unlikely someone
    * will be able to cancel it before it runs, but they can try.
    */
   g_queue_push_head(gState.workQueue, task);

   if (gState.pool != NULL) {
      GError *err = NULL;

      /* The client data pointer is bogus, just to avoid passing NULL. */
      g_thread_pool_push(gState.pool, &gState, &err);
      if (err == NULL) {
         goto exit;
      } else {
         g_warning("error sending work request, executing in service thread: %s",
                   err->message);
         g_clear_error(&err);
      }
   }

   /* Run the task in the service's thread. */
   task->srcId = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                 ToolsCorePoolDoWork,
                                 task,
                                 ToolsCorePoolDestroyTask);

exit:
   g_mutex_unlock(&gState.lock);
   return id;
}


/*
 *******************************************************************************
 * ToolsCorePoolCancel --                                                 */ /**
 *
 * Cancels a queue task.
 *
 * @see ToolsCorePool_CancelTask()
 *
 * @param[in] id  Task ID.
 *
 *******************************************************************************
 */

static void
ToolsCorePoolCancel(guint id)
{
   GList *taskLnk;
   WorkerTask *task = NULL;
   WorkerTask search = { id, };

   g_return_if_fail(id != 0);

   g_mutex_lock(&gState.lock);
   if (!gState.active) {
      goto exit;
   }

   taskLnk = g_queue_find_custom(gState.workQueue, &search, ToolsCorePoolCompareTask);
   if (taskLnk != NULL) {
      task = taskLnk->data;
      g_queue_delete_link(gState.workQueue, taskLnk);
   }

exit:
   g_mutex_unlock(&gState.lock);

   if (task != NULL) {
      if (task->srcId > 0) {
         g_source_remove(task->srcId);
      } else {
         ToolsCorePoolDestroyTask(task);
      }
   }
}


/*
 *******************************************************************************
 * ToolsCorePoolStart --                                                  */ /**
 *
 * Start a new task in a dedicated thread.
 *
 * @see ToolsCorePool_StartThread()
 *
 * @param[in] ctx        Application context.
 * @param[in] threadName Name for the new thread.
 * @param[in] cb         Callback that executes the task.
 * @param[in] interrupt  Callback that interrupts the task.
 * @param[in] data       Opaque data.
 * @param[in] dtor       Destructor for the task data.
 *
 * @return TRUE iff thread was successfully started.
 *
 *******************************************************************************
 */

static gboolean
ToolsCorePoolStart(ToolsAppCtx *ctx,
                   const gchar *threadName,
                   ToolsCorePoolCb cb,
                   ToolsCorePoolCb interrupt,
                   gpointer data,
                   GDestroyNotify dtor)
{
   GError *err = NULL;
   StandaloneTask *task = NULL;

   g_mutex_lock(&gState.lock);
   if (!gState.active) {
      goto exit;
   }

   task = g_malloc0(sizeof *task);
   task->active = TRUE;
   task->cb = cb;
   task->interrupt = interrupt;
   task->data = data;
   task->dtor = dtor;
   task->thread = g_thread_try_new(threadName, ToolsCorePoolRunThread, task, &err);

   if (err == NULL) {
      g_ptr_array_add(gState.threads, task);
   } else {
      g_warning("failed to start thread: %s.", err->message);
      g_clear_error(&err);
      g_free(task);
      task = NULL;
   }

exit:
   g_mutex_unlock(&gState.lock);
   return task != NULL;
}


/*
 *******************************************************************************
 * ToolsCorePool_Init --                                                  */ /**
 *
 * Initializes the shared thread pool. Reads configuration data from the
 * container-specific section of the config dictionary, so different containers
 * can have different configuration. Exports the thread pool functions through
 * the service's object.
 *
 * @param[in] ctx Application context.
 *
 *******************************************************************************
 */

void
ToolsCorePool_Init(ToolsAppCtx *ctx)
{
   gint maxThreads;
   GError *err = NULL;

   ToolsServiceProperty prop = { TOOLS_CORE_PROP_TPOOL };

   gState.funcs.submit = ToolsCorePoolSubmit;
   gState.funcs.cancel = ToolsCorePoolCancel;
   gState.funcs.start = ToolsCorePoolStart;
   gState.ctx = ctx;

   maxThreads = g_key_file_get_integer(ctx->config, ctx->name,
                                       "pool.maxThreads", &err);
   if (err != NULL) {
      maxThreads = DEFAULT_MAX_THREADS;
      g_clear_error(&err);
   }

   if (maxThreads > 0) {
      gState.pool = g_thread_pool_new(ToolsCorePoolRunWorker,
                                      NULL, maxThreads, FALSE, &err);
      if (err == NULL) {
         gint maxIdleTime;
         gint maxUnused;

         maxIdleTime = g_key_file_get_integer(ctx->config, ctx->name,
                                              "pool.maxIdleTime", &err);
         if (err != NULL || maxIdleTime <= 0) {
            maxIdleTime = DEFAULT_MAX_IDLE_TIME;
            g_clear_error(&err);
         }

         maxUnused = g_key_file_get_integer(ctx->config, ctx->name,
                                            "pool.maxUnusedThreads", &err);
         if (err != NULL || maxUnused < 0) {
            maxUnused = DEFAULT_MAX_UNUSED_THREADS;
            g_clear_error(&err);
         }

         g_thread_pool_set_max_idle_time(maxIdleTime);
         g_thread_pool_set_max_unused_threads(maxUnused);
      } else {
         g_warning("error initializing thread pool, running single threaded: %s",
                   err->message);
         g_clear_error(&err);
      }
   }

   gState.active = TRUE;
   g_mutex_init(&gState.lock);
   gState.threads = g_ptr_array_new();
   gState.workQueue = g_queue_new();

   ToolsCoreService_RegisterProperty(ctx->serviceObj, &prop);
   g_object_set(ctx->serviceObj, TOOLS_CORE_PROP_TPOOL, &gState.funcs, NULL);
}


/*
 *******************************************************************************
 * ToolsCorePool_Shutdown --                                              */ /**
 *
 * Shuts down the shared thread pool. This function will interrupt any running
 * threads (by calling their registered interrupt function), and wait for all
 * running tasks to finish before cleaning the remaining tasks and shared state.
 *
 * @param[in] ctx Application context.
 *
 *******************************************************************************
 */

void
ToolsCorePool_Shutdown(ToolsAppCtx *ctx)
{
   guint i;

   g_mutex_lock(&gState.lock);
   gState.active = FALSE;
   g_mutex_unlock(&gState.lock);

   /* Notify all spawned threads to stop. */
   for (i = 0; i < gState.threads->len; i++) {
      StandaloneTask *task = g_ptr_array_index(gState.threads, i);
      if (task->active && task->interrupt) {
         task->interrupt(gState.ctx, task->data);
      }
   }

   /* Stop the thread pool. */
   if (gState.pool != NULL) {
      g_thread_pool_free(gState.pool, TRUE, TRUE);
   }

   /* Join all spawned threads. */
   for (i = 0; i < gState.threads->len; i++) {
      StandaloneTask *task = g_ptr_array_index(gState.threads, i);
      ToolsCorePoolDestroyThread(task);
   }

   /* Destroy all pending tasks. */
   while (1) {
      WorkerTask *task = g_queue_pop_tail(gState.workQueue);
      if (task != NULL) {
         ToolsCorePoolDestroyTask(task);
      } else {
         break;
      }
   }

   /* Cleanup. */
   g_ptr_array_free(gState.threads, TRUE);
   g_queue_free(gState.workQueue);
   g_mutex_clear(&gState.lock);
   memset(&gState, 0, sizeof gState);
   g_object_set(ctx->serviceObj, TOOLS_CORE_PROP_TPOOL, NULL, NULL);
}

