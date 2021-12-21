/*********************************************************
 * Copyright (C) 2021 VMware, Inc. All rights reserved.
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

/*
 * componentMgrInstallManager.c --
 *
 * This file contains all the neccessary functions and handling of performing
 * check status operation and add/remove of a component.
 * The operations are triggerred as an async process and GSource timers are
 * created to monitor the execution status of the async process.
 * After successful completion of the async process, it's resources are
 * released to make way for a new async process.
 * Contains functions related to creation of new async process, monitoring of
 * an async process and freeing of a async process resources.
 *
 */

#include "componentMgrPlugin.h"


/*
 *****************************************************************************
 * ComponentMgr_FreeAsyncProc --
 *
 * This function frees the async process resources.
 * First we kill the commandline process which is child's child by pid.
 * Then we kill the child process which is vmtoolsd.
 *
 * @param[in] asyncProcessInfo Asynchronous process resources to be freed.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      Destroys and frees the async process resources.
 *
 *****************************************************************************
 */

void
ComponentMgr_FreeAsyncProc(AsyncProcessInfo *procInfo) // IN
{
   int componentIndex = procInfo->componentIndex;
#if defined(__linux__)
   if (ProcMgr_IsAsyncProcRunning(procInfo->asyncProc)) {
      ProcMgr_Pid procPid = ProcMgr_GetPid(procInfo->asyncProc);
      ProcMgr_KillByPid(procPid);
   }
#endif
   ProcMgr_Kill(procInfo->asyncProc);
   ProcMgr_Free(procInfo->asyncProc);
   g_free(procInfo);

   // Reset the async process info in the components array since the async
   // process is no longer available.
   ComponentMgr_ResetComponentAsyncProcInfo(componentIndex);
}


/*
 *****************************************************************************
 * ComponentMgrCheckStatusMonitor --
 *
 * This function monitors the state the async process running check status
 * command for a component. On completion of async process, the exit code
 * is captured and set in the components structure for that component.
 * On expiry of timer the async process will be killed.
 *
 * @param[in] data asyncProcessInfo pointer containing async process and
                   component info.
 *
 * @return
 *      G_SOURCE_CONTINUE To continue polling.
 *      G_SOURCE_REMOVE to stop polling.
 *
 * Side effects:
 *      Monitor the asyncProcess for its successful termination by polling.
 *      Kill the asyncProcess if it's taking long to terminate.
 *
 *****************************************************************************
 */

static gboolean
ComponentMgrCheckStatusMonitor(void *data) // IN
{
   ProcMgr_Pid procPid;
   int componentIndex;
   const char *componentName;
   void (*callbackFunction)(int compIndex) = NULL;

   AsyncProcessInfo *procInfo = (AsyncProcessInfo*)data;
   ASSERT(procInfo->asyncProc != NULL);

   /*
    * For every timeout callback from the timeout source, decrease
    * the remaining execution time for the component.
    */
   procInfo->backoffTimer -= COMPONENTMGR_ASYNC_CHECK_STATUS_POLL_INTERVAL;
   procPid = ProcMgr_GetPid(procInfo->asyncProc);
   componentIndex = procInfo->componentIndex;
   componentName = ComponentMgr_GetComponentName(componentIndex);

   g_debug("%s: Callback received for process ID %d and component %s."
           " Remaining time before termination %ds.\n", __FUNCTION__, procPid,
           componentName, procInfo->backoffTimer);

   if (!ProcMgr_IsAsyncProcRunning(procInfo->asyncProc)) {
      int exitCode = -1;
#if defined(__linux__)
      if (ProcMgr_GetExitCode(procInfo->asyncProc, &exitCode) || exitCode == -1) {
         exitCode = SCRIPTFAILED;
      }
#else
      if (ProcMgr_GetExitCode(procInfo->asyncProc, &exitCode)) {
         exitCode = SCRIPTFAILED;
      }
#endif
     g_debug("%s: Checking status of a component has terminated gracefully"
             " with exit code %d.\n", __FUNCTION__, exitCode);

     ComponentMgr_SetStatusComponentInfo(procInfo->ctx,
                                         exitCode,
                                         procInfo->componentIndex);
     callbackFunction = procInfo->callbackFunction;

     /*
      * At this stage free the asyncProcInfo object to make way for new async
      * process. Source timer for a component will be no longer valid from here
      * set it to NULL for next async process.
      */
     ComponentMgr_FreeAsyncProc(procInfo);
     ComponentMgr_ResetComponentGSourceTimer(componentIndex);

     /*
      * After checkstatus operation has completed sucessfully, we can have a
      * next sequence of operations to be executed on a component.
      */
     if (callbackFunction != NULL) {
        callbackFunction(componentIndex);
     }

     return G_SOURCE_REMOVE;
   } else {
      /*
       * At this stage the async process seems to be still running check
       * status operation on a component. If the backoff timer value is not
       * reached for a component, we proceed and wait for the async process
       * to terminate. If the backoff timer has reached 0, the timed wait
       * for the component is completed hence we kill the async process.
       * First we kill the command process which is child's child by pid.
       * Then we kill the child process which is vmtoolsd.
       */
      g_debug("%s: Process still running for component %s.\n", __FUNCTION__,
              componentName);

      if (procInfo->backoffTimer == 0) {
         g_warning("%s: Backoff timer expired for process %d running check "
                   "status for component %s. Async process will be killed.",
                   __FUNCTION__, procPid, componentName);

         ComponentMgr_SetStatusComponentInfo(procInfo->ctx,
                                             SCRIPTTERMINATED,
                                             componentIndex);

         /*
          * At this point the async process has timed out, so we need to
          * kill the async process to make way for a new async process.
          * Source timer for a component will be no longer valid from here
          * set it to NULL for the next async process.
          */
         ComponentMgr_FreeAsyncProc(procInfo);
         ComponentMgr_ResetComponentGSourceTimer(componentIndex);
         return G_SOURCE_REMOVE;
      }
   }

   return G_SOURCE_CONTINUE;
}


/*
 *****************************************************************************
 * ComponentMgrProcessMonitor --
 *
 * This function monitors the async process running the present/absent action
 * on a component.
 *
 * @param[in] data asyncProcessInfo pointer containing async process and
                   component info.
 *
 * @return
 *      G_SOURCE_CONTINUE To continue polling.
 *      G_SOURCE_REMOVE To stop polling.
 *
 * Side effects:
 *      Monitor the async process for its successful termination by polling.
 *      Kill the async process if it's taking long to terminate.
 *
 *****************************************************************************
 */

static gboolean
ComponentMgrProcessMonitor(void *data) // IN
{
   ProcMgr_Pid procPid;
   int componentIndex;
   char *commandline;
   const char *componentName;

   AsyncProcessInfo *procInfo = (AsyncProcessInfo*)data;
   ASSERT(procInfo->asyncProc != NULL);

   procInfo->backoffTimer -= COMPONENTMGR_ASYNCPROCESS_POLL_INTERVAL;
   procPid = ProcMgr_GetPid(procInfo->asyncProc);
   componentIndex = procInfo->componentIndex;
   componentName = ComponentMgr_GetComponentName(componentIndex);

   g_debug("%s: Callback received for process ID %d and component %s."
           " Remaining time before termination %ds.\n", __FUNCTION__, procPid,
           componentName, procInfo->backoffTimer);

   if (!ProcMgr_IsAsyncProcRunning(procInfo->asyncProc)) {
      /*
       * At this stage the async process has completed its execution.
       * Free all the async process resources and destroy the GSource timer.
       */
      g_debug("%s: Async process has exited.\n", __FUNCTION__);

      ComponentMgr_FreeAsyncProc(procInfo);
      ComponentMgr_ResetComponentGSourceTimer(componentIndex);

      commandline = ComponentMgr_CheckStatusCommandLine(componentIndex);
      if (commandline == NULL) {
         g_info("%s: Unable to construct commandline instruction to run check "
                "status for the component %s\n", __FUNCTION__,
                ComponentMgr_GetComponentName(componentIndex));
         ComponentMgr_SetStatusComponentInfo(ComponentMgr_GetToolsAppCtx(),
                                             SCRIPTTERMINATED, componentIndex);
         return G_SOURCE_REMOVE;
      }

      /*
       * At this stage the async process running present/absent has completed.
       * We need to check the status of a component asynchronously and set the
       * component status.
       */
      ComponentMgr_AsynchronousComponentCheckStatus(ComponentMgr_GetToolsAppCtx(),
                                                    commandline,
                                                    componentIndex,
                                                    NULL);
      free(commandline);
      return G_SOURCE_REMOVE;
   } else {
      /*
       * At this stage the async process seems to be still running for a
       * component. If the backoff timer value has not reached 0,
       * we proceed and wait for the async process to terminate.
       * If the backoff timer has reached 0, the timed wait
       * has reached limit for the component and we kill the async process.
       */
      g_debug("%s: Process still running for component %s.\n", __FUNCTION__,
              componentName);

      if (procInfo->backoffTimer == 0) {
         g_warning("%s: Backoff timer expired for process %d running action for"
                   "component %s. Async process will be killed.",
                   __FUNCTION__, procPid, componentName);

         /*
          * At this point the async process has to be terminated. We can
          * free the structure to make way for newer async process.
          * Source timer for a component will be no longer valid from here
          * set it to NULL for next async process.
          */
         ComponentMgr_FreeAsyncProc(procInfo);
         ComponentMgr_ResetComponentGSourceTimer(componentIndex);

         commandline = ComponentMgr_CheckStatusCommandLine(componentIndex);
         if (commandline == NULL) {
            g_info("%s: Unable to construct commandline instruction to run check "
                   "status for the component %s\n", __FUNCTION__,
                   ComponentMgr_GetComponentName(componentIndex));
            ComponentMgr_SetStatusComponentInfo(ComponentMgr_GetToolsAppCtx(),
                                                SCRIPTTERMINATED, componentIndex);
            return G_SOURCE_REMOVE;
         }

         ComponentMgr_AsynchronousComponentCheckStatus(ComponentMgr_GetToolsAppCtx(),
                                                       commandline,
                                                       componentIndex,
                                                       NULL);

         free(commandline);
         return G_SOURCE_REMOVE;
      }
   }
   /*
    * The async process has not yet completed its action. So poll again
    * using the same GSource timer for that particular component.
    */
   return G_SOURCE_CONTINUE;
}


/*
 *****************************************************************************
 * ComponentMgrCreateAsyncProcessInfo --
 *
 * This function creates the asynProcessInfo object related to an async process
 *
 * @param[in] asyncProc A ProcMgr_AsyncProc pointer containing information
                        about the created async process.
 * @param[in] ctx Tools application context.
 * @param[in] backoffTimer A timer value after expiry of which the async
                           process will be killed.
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 * @param[in] callbackFunction A callback function to sequence the operation
 *                             after async process finishes.
 *
 * @return
 *      An asyncProcessInfo object.
 *
 * Side effects:
 *      The created asyncProcesInfo object should be freed after process
 *      is completed or terminated.
 *
 *****************************************************************************
 */

static AsyncProcessInfo*
ComponentMgrCreateAsyncProcessInfo(ProcMgr_AsyncProc *asyncProc,                 // IN
                                   ToolsAppCtx *ctx,                             // IN
                                   int backoffTimer,                             // IN
                                   int componentIndex,                           // IN
                                   void (*callbackFunction)(int componentIndex)) // IN
{
   AsyncProcessInfo *procInfo;
   procInfo = g_malloc(sizeof *procInfo);
   procInfo->asyncProc = asyncProc;
   procInfo->ctx = ctx;
   procInfo->backoffTimer = backoffTimer;
   procInfo->componentIndex = componentIndex;
   procInfo->callbackFunction = callbackFunction;

   return procInfo;
}


/*
 *****************************************************************************
 * ComponentMgr_AsynchronousComponentCheckStatus --
 *
 * This function launches an async process to check the current status of the
 * component on the system.
 *
 * @param[in] ctx Tools application context.
 * @param[in] commandline <component_script> <action_arguments> command to be
 *                        executed to add/remove the component.
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 * @param[in] callbackFunction A callback function to sequence the operation
 *                             after async call completes.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      Async process creation may fail for many reasons.
 *
 *****************************************************************************
 */

void
ComponentMgr_AsynchronousComponentCheckStatus(ToolsAppCtx *ctx,                // IN
                                              const char *commandline,         // IN
                                              int componentIndex,              // IN
                                              void (*callback)(int compIndex)) // IN
{
   ProcMgr_ProcArgs userArgs;
   GSource *sourceTimer;
   AsyncProcessInfo *procInfo;
   ProcMgr_AsyncProc *asyncProc;

   /*
    * If an async process is already running for the component.
    * Do not spin another async process.
    */
   ASSERT(commandline != NULL);
   ASSERT(!ComponentMgr_IsAsyncProcessRunning(componentIndex));

   memset(&userArgs, 0, sizeof userArgs);
   asyncProc = ProcMgr_ExecAsync(commandline, &userArgs);
   if (asyncProc == NULL) {
      g_warning("%s: Failed to create process", __FUNCTION__);
      return;
   }

   /*
    * We need information about the async process, component and backoff
    * timer value, hence populate the same and store for future references.
    */
   procInfo = ComponentMgrCreateAsyncProcessInfo(asyncProc, ctx,
                                                 COMPONENTMGR_ASYNC_CHECK_STATUS_TERMINATE_PERIOD,
                                                 componentIndex,
                                                 callback);
   /*
    * Set the asyncProcInfo field and GSource timer in the components array
    * to cache information of a running async process for a component.
    */
   sourceTimer = g_timeout_source_new(COMPONENTMGR_ASYNC_CHECK_STATUS_POLL_INTERVAL * 1000);
   ComponentMgr_SetComponentAsyncProcInfo(procInfo, componentIndex);
   ComponentMgr_SetComponentGSourceTimer(sourceTimer, componentIndex);
   VMTOOLSAPP_ATTACH_SOURCE(ctx, sourceTimer, ComponentMgrCheckStatusMonitor,
                            procInfo, NULL);
   g_source_unref(sourceTimer);
}


/*
 *****************************************************************************
 * ComponentMgr_AsynchronousComponentActionStart --
 *
 * This function invokes the component script as an async process to perform
 * present/absent action and a GSource timer to poll the progress.
 *
 * @param[in] ctx Tools application context.
 * @param[in] commandline <component_script> <action_arguments> command to be
 *                        executed to add/remove the component.
 * @param[in] componentIndex Index of component in global array of components.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      Async process creation may fail for many reasons.
 *
 *****************************************************************************
 */

void
ComponentMgr_AsynchronousComponentActionStart(ToolsAppCtx *ctx,        // IN
                                              const char *commandline, // IN
                                              int componentIndex)      // IN
{
   ProcMgr_ProcArgs userArgs;
   GSource *sourceTimer;
   AsyncProcessInfo *procInfo;
   ProcMgr_AsyncProc *asyncProc;

   /*
    * If an async process is already running for the component.
    * Do not spin another async process.
    */
   ASSERT(commandline != NULL);
   ASSERT(!ComponentMgr_IsAsyncProcessRunning(componentIndex));

   memset(&userArgs, 0, sizeof userArgs);
   asyncProc = ProcMgr_ExecAsync(commandline, &userArgs);
   if (asyncProc == NULL) {
      g_warning("%s: Failed to create process", __FUNCTION__);
      return;
   }

   /*
    * We need information about the async process, component and backoff
    * timer value, hence populate the same and store for future references.
    */
   procInfo = ComponentMgrCreateAsyncProcessInfo(asyncProc, ctx,
                                                 COMPONENTMGR_ASYNCPROCESS_TERMINATE_PERIOD,
                                                 componentIndex, NULL);
   /*
    * Set the asyncProcInfo field and GSource timer field in the components
    * array to cache info of running async process for a component.
    */
   sourceTimer = g_timeout_source_new(COMPONENTMGR_ASYNCPROCESS_POLL_INTERVAL * 1000);
   ComponentMgr_SetComponentAsyncProcInfo(procInfo, componentIndex);
   ComponentMgr_SetComponentGSourceTimer(sourceTimer, componentIndex);
   VMTOOLSAPP_ATTACH_SOURCE(ctx, sourceTimer,
                            ComponentMgrProcessMonitor, procInfo, NULL);
   g_source_unref(sourceTimer);
}
