/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * scriptOps.c --
 *
 * Functions for handling freeze/thaw scripts.
 */

#include "vmBackup.h"
#include "vmBackupInt.h"

#include <stdlib.h>
#include <string.h>

#include "vm_basic_defs.h"
#include "debug.h"
#include "file.h"
#include "guestApp.h"
#include "procMgr.h"
#include "str.h"
#include "util.h"
#include "codeset.h"


typedef struct VmBackupScript {
   char *path;
   ProcMgr_AsyncProc *proc;
} VmBackupScript;


typedef struct VmBackupScriptOp {
   VmBackupOp callbacks;
   Bool canceled;
   Bool thawFailed;
   VmBackupScriptType type;
   VmBackupState *state;
} VmBackupScriptOp;


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupGetScriptPath --
 *
 *    Returns the path where the scripts to be executed reside.
 *
 * Result
 *    A string with the requested path.
 *
 * Side effects:
 *    Allocates memory for the path.
 *
 *-----------------------------------------------------------------------------
 */

char *
VmBackupGetScriptPath(void)
{
   char *scriptPath = NULL;
   char *installPath = GuestApp_GetInstallPath();

   if (installPath == NULL) {
      return NULL;
   }

   scriptPath = Str_Asprintf(NULL, "%s%s%s", installPath, DIRSEPS, "backupScripts.d");
   free(installPath);

   return scriptPath;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmBackupRunNextScript --
 *
 *    Runs the next script for the given operation. If thawing (or running
 *    scripts after a failure), this function will try as much as possible
 *    to start a script, meaning that if it fails to start a script it will
 *    try to start the preceding one until one script is run, or it runs out
 *    of scripts to try.
 *
 * Results:
 *    -1: an error occurred.
 *    0: no more scripts to run.
 *    1: script was started.
 *
 * Side effects:
 *    Increments (or decrements) the "current script" index in the backup state.
 *
 *-----------------------------------------------------------------------------
 */

static int
VmBackupRunNextScript(VmBackupScriptOp *op)  // IN/OUT
{
   const char *scriptOp;
   int ret = 0;
   ssize_t index;
   VmBackupScript *scripts = op->state->scripts;

   switch (op->type) {
   case VMBACKUP_SCRIPT_FREEZE:
      index = ++op->state->currentScript;
      scriptOp = "freeze";
      break;

   case VMBACKUP_SCRIPT_FREEZE_FAIL:
      index = --op->state->currentScript;
      scriptOp = "freezeFail";
      break;

   case VMBACKUP_SCRIPT_THAW:
      index = --op->state->currentScript;
      scriptOp = "thaw";
      break;

   default:
      NOT_REACHED();
   }

   while (index >= 0 && scripts[index].path != NULL) {
      char *cmd;

      cmd = Str_Asprintf(NULL, "\"%s\" %s", scripts[index].path, scriptOp);
      if (cmd == NULL) {
         index = --op->state->currentScript;
         op->thawFailed = TRUE;
      }

      if ((scripts[index].proc = ProcMgr_ExecAsync(cmd, NULL)) == NULL) {
         if (op->type == VMBACKUP_SCRIPT_FREEZE) {
            ret = -1;
            break;
         } else {
            index = --op->state->currentScript;
            op->thawFailed = TRUE;
         }
      } else {
         ret = 1;
         break;
      }
   }

   /* This happens if all thaw/fail scripts failed to start. */
   if (index == -1 && scripts[0].proc == NULL) {
      ret = -1;
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupStringCompare --
 *
 *    Comparison function used to sort the script list in ascending order.
 *
 * Result
 *    The result of strcmp(str1, str2).
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VmBackupStringCompare(const void *str1,   // IN
                      const void *str2)   // IN
{
   return strcmp(* (char * const *) str1,* (char * const *) str2);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupScriptOpQuery --
 *
 *    Checks the status of the current running script. If the script is
 *    finished, run the next script in the queue or, if no scripts are left,
 *    return a "finished" status.
 *
 * Result
 *    The status of the operation.
 *
 * Side effects:
 *    Might start a new process.
 *
 *-----------------------------------------------------------------------------
 */

static VmBackupOpStatus
VmBackupScriptOpQuery(VmBackupOp *_op) // IN
{
   VmBackupOpStatus ret = VMBACKUP_STATUS_PENDING;
   VmBackupScriptOp *op = (VmBackupScriptOp *) _op;
   VmBackupScript *scripts = op->state->scripts;
   VmBackupScript *currScript = NULL;

   if (scripts != NULL) {
      currScript = &scripts[op->state->currentScript];
   }

   if (op->canceled) {
      ret = VMBACKUP_STATUS_CANCELED;
      goto exit;
   } else if (scripts == NULL || currScript->proc == NULL) {
      ret = VMBACKUP_STATUS_FINISHED;
      goto exit;
   }

   if (!ProcMgr_IsAsyncProcRunning(currScript->proc)) {
      int exitCode;

      /*
       * If thaw scripts fail, keep running and only notify the failure after
       * all others have run.
       */
      if (ProcMgr_GetExitCode(currScript->proc, &exitCode) != 0 ||
          exitCode != 0) {
          if (op->type == VMBACKUP_SCRIPT_FREEZE) {
             ret = VMBACKUP_STATUS_ERROR;
             goto exit;
          } else if (op->type == VMBACKUP_SCRIPT_THAW) {
             op->thawFailed = TRUE;
          }
      }

      ProcMgr_Free(currScript->proc);
      currScript->proc = NULL;

      switch (VmBackupRunNextScript(op)) {
      case -1:
         ret = VMBACKUP_STATUS_ERROR;
         break;

      case 0:
         ret = op->thawFailed ? VMBACKUP_STATUS_ERROR : VMBACKUP_STATUS_FINISHED;
         break;

      default:
         break;
      }
   }

exit:
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupScriptOpRelease --
 *
 *    Frees memory allocated for the state object. Behavior is undefined
 *    if the memory is freed before the query function says the operation
 *    if finished.
 *
 * Result
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VmBackupScriptOpRelease(VmBackupOp *_op)  // IN
{
   size_t i;
   VmBackupScriptOp *op = (VmBackupScriptOp *) _op;

   if (op->type != VMBACKUP_SCRIPT_FREEZE && op->state->scripts != NULL) {
      VmBackupScript *scripts = op->state->scripts;
      for (i = 0; scripts[i].path != NULL; i++) {
         free(scripts[i].path);
         if (scripts[i].proc != NULL) {
            ProcMgr_Free(scripts[i].proc);
         }
      }
      free(op->state->scripts);
      op->state->scripts = NULL;
      op->state->currentScript = 0;
   }

   free(op);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupScriptOpCancel --
 *
 *    Cancels the current operation. Kills any currently running script and
 *    flags the operation as canceled.
 *
 * Result
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VmBackupScriptOpCancel(VmBackupOp *_op)   // IN
{
   VmBackupScriptOp *op = (VmBackupScriptOp *) _op;
   VmBackupScript *scripts = op->state->scripts;
   VmBackupScript *currScript = NULL;
   ProcMgr_Pid pid;

   if (scripts != NULL) {
      currScript = &scripts[op->state->currentScript];
      ASSERT(currScript->proc != NULL);

      pid = ProcMgr_GetPid(currScript->proc);
      if (!ProcMgr_KillByPid(pid)) {
         // XXX: what to do in this situation? other than log and cry?
      } else {
         int exitCode;
         ProcMgr_GetExitCode(currScript->proc, &exitCode);
      }
   }

   op->canceled = TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupNewScriptOp --
 *
 *    Creates a new state object to monitor the execution of OnFreeze or
 *    OnThaw scripts. This will identify all the scripts in the backup scripts
 *    directory and add them to an execution queue.
 *
 *    Note: there is some state created when instantianting the "OnFreeze"
 *    scripts which is only released after the "OnThaw" scripts are run. So
 *    the caller has to make sure that thaw (or fail) scripts are run every
 *    time the freeze scripts are run.
 *
 * Result
 *    A pointer to the operation state, or NULL on failure.
 *
 * Side effects:
 *    If there are scripts to be executed, the first one is started.
 *
 *-----------------------------------------------------------------------------
 */

VmBackupOp *
VmBackupNewScriptOp(VmBackupScriptType type, // IN
                    VmBackupState *state)    // IN
{
   Bool fail = FALSE;
   char **fileList = NULL;
   char *scriptDir = NULL;
   int numFiles = 0;
   size_t i;
   VmBackupScriptOp *op = NULL;

   scriptDir = VmBackupGetScriptPath();
   if (scriptDir == NULL) {
      goto exit;
   }

   op = calloc(1, sizeof *op);
   if (op == NULL) {
      goto exit;
   }

   op->state = state;
   op->type = type;
   op->callbacks.queryFn = VmBackupScriptOpQuery;
   op->callbacks.cancelFn = VmBackupScriptOpCancel;
   op->callbacks.releaseFn = VmBackupScriptOpRelease;

   Debug("Trying to run scripts from %s\n", scriptDir);

   /*
    * Load the list of scripts to run when freezing. The same list will be
    * used later in case of failure, or when thawing, in reverse order.
    *
    * This logic won't recurse into directories, so only files directly under
    * the script dir will be considered.
    */
   if (type == VMBACKUP_SCRIPT_FREEZE && File_IsDirectory(scriptDir)) {
      size_t scriptCount = 0;

      state->scripts = NULL;
      state->currentScript = 0;
      numFiles = File_ListDirectory(scriptDir, &fileList);

      if (numFiles > 0) {
         VmBackupScript *scripts;

         scripts = calloc(1, (numFiles + 1) * sizeof *scripts);
         if (scripts == NULL) {
            fail = TRUE;
            goto exit;
         }

         state->scripts = scripts;

         /*
          * VmBackupRunNextScript increments the index, so need to make it point
          * to "before the first script".
          */
         state->currentScript = -1;

         if (numFiles > 1) {
            qsort(fileList, (size_t) numFiles, sizeof *fileList, VmBackupStringCompare);
         }

         for (i = 0; i < numFiles; i++) {
            char *script;

            script = Str_Asprintf(NULL, "%s%c%s", scriptDir, DIRSEPC, fileList[i]);
            if (script == NULL) {
               fail = TRUE;
               goto exit;
            } else if (File_IsFile(script)) {
               scripts[scriptCount++].path = script;
            } else {
               free(script);
            }
         }
      }
   }

   /*
    * If there are any scripts to be executed, start the first one. If we get to
    * this point, we won't free the scripts array until VmBackupScriptOpRelease
    * is called after thawing (or after the sync provider failed and the "fail"
    * scripts are run).
    */
   fail = (state->scripts != NULL && VmBackupRunNextScript(op) == -1);

exit:
   /* Free the file list. */
   for (i = 0; i < numFiles; i++) {
      free(fileList[i]);
   }
   free(fileList);

   if (fail && op != NULL) {
      VmBackup_Release((VmBackupOp *) op);
      op = NULL;
   }
   free(scriptDir);
   return (VmBackupOp *) op;
}

