/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * scriptOps.c --
 *
 * Functions for handling freeze/thaw scripts.
 */

#include "vmBackup.h"

#include <stdlib.h>
#include <string.h>

#include "vm_basic_defs.h"
#include "debug.h"
#include "dynbuf.h"
#include "file.h"
#include "guestApp.h"
#include "procMgr.h"
#include "syncDriver.h"
#include "str.h"
#include "util.h"


/* Totally arbitrary limit. */
#define MAX_SCRIPTS 256

typedef struct VmBackupScript {
   char *path;
   ProcMgr_AsyncProc *proc;
} VmBackupScript;


typedef struct VmBackupScriptOp {
   VmBackupOp callbacks;
   VmBackupScript scripts[MAX_SCRIPTS];
   unsigned int current;
   Bool canceled;
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
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static const char *
VmBackupGetScriptPath(void)
{
   static char scriptPath[FILE_MAXPATH] = { '\0' };

   if (*scriptPath == '\0') {
      const char *installPath;
      installPath = GuestApp_GetInstallPath();
      Str_Strcat(scriptPath, installPath, sizeof scriptPath);
      Str_Strcat(scriptPath, DIRSEPS, sizeof scriptPath);
      Str_Strcat(scriptPath, "backupScripts.d", sizeof scriptPath);
   }

   return (*scriptPath != '\0') ? scriptPath : NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupStringCompare --
 *
 *    Comparison function used to sort the script list.
 *
 * Result
 *    The result of strcmp() on the strings.
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
   VmBackupScript *currScript = &(op->scripts[op->current]);

   if (op->canceled) {
      ret = VMBACKUP_STATUS_CANCELED;
      goto exit;
   } else if (currScript->proc == NULL) {
      ret = VMBACKUP_STATUS_FINISHED;
      goto exit;
   }

   if (!ProcMgr_IsAsyncProcRunning(currScript->proc)) {
      int exitCode;

      if (ProcMgr_GetExitCode(currScript->proc, &exitCode) != 0 ||
          exitCode != 0) {
         // XXX: log error.
         ret = VMBACKUP_STATUS_ERROR;
         goto exit;
      }

      ProcMgr_Free(currScript->proc);
      currScript->proc = NULL;

      /*
       * If there's another script to execute, start it. Otherwise, just
       * say we're finished.
       */
      if (op->current < MAX_SCRIPTS - 1 && op->scripts[op->current+1].path != NULL) {
         op->current += 1;
         currScript = &(op->scripts[op->current]);
         currScript->proc = ProcMgr_ExecAsync(currScript->path, NULL);
         if (currScript->proc == NULL) {
            // XXX : log error
            ret = VMBACKUP_STATUS_ERROR;
         }
      } else {
         ret = VMBACKUP_STATUS_FINISHED;
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
   int i;
   VmBackupScriptOp *op = (VmBackupScriptOp *) _op;

   for (i = 0; i < MAX_SCRIPTS && op->scripts[i].path != NULL; i++) {
      free(op->scripts[i].path);
      if (op->scripts[i].proc != NULL) {
         ProcMgr_Free(op->scripts[i].proc);
      }
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
   VmBackupScript *currScript = &(op->scripts[op->current]);
   ProcMgr_Pid pid;

   ASSERT(currScript->proc != NULL);

   pid = ProcMgr_GetPid(currScript->proc);
   if (!ProcMgr_KillByPid(pid)) {
      // XXX: what to do in this situation? other than log and cry?
   } else {
      int exitCode;
      ProcMgr_GetExitCode(currScript->proc, &exitCode);
   }

   op->canceled = TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupNewScriptOp --
 *
 *    Creates a new state object to monitor the execution of OnFreeze or
 *    OnThaw scripts. This will identify all the scripts in the given
 *    directory and add them to an execution queue.
 *
 * Result
 *    A pointer to the operation state, or NULL on failure.
 *
 * Side effects:
 *    If there are scripts to be executed, the first one is started.
 *
 *-----------------------------------------------------------------------------
 */

static VmBackupOp *
VmBackupNewScriptOp(const char *scriptDir, Bool freeze)  // IN
{
   VmBackupScriptOp *op = NULL;

   op = Util_SafeMalloc(sizeof *op);
   memset(op, 0, sizeof *op);

   op->callbacks.queryFn = VmBackupScriptOpQuery;
   op->callbacks.cancelFn = VmBackupScriptOpCancel;
   op->callbacks.releaseFn = VmBackupScriptOpRelease;

   Debug("Trying to run scripts from %s\n", scriptDir);

   if (File_IsDirectory(scriptDir)) {
      int i, cnt, numFiles;
      char **fileList = NULL;

      numFiles = File_ListDirectory(scriptDir, &fileList);

      if (numFiles > 1) {
         qsort(fileList, (size_t) numFiles, sizeof *fileList, VmBackupStringCompare);
      }

      cnt = 0;
      for (i = 0; i < numFiles && cnt < MAX_SCRIPTS; i++) {
         /* Just run files in the scripts dir. Don't recurse into directories. */
         char script[FILE_MAXPATH + sizeof " freeze"];
         Str_Sprintf(script, sizeof script, "%s%c%s",
                     scriptDir, DIRSEPC, fileList[i]);
         if (File_IsFile(script)) {
            char *escaped;

            Debug("adding script for execution: %s\n", fileList[i]);

            escaped = Str_Asprintf(NULL, "\"%s\" %s",
                                   script, (freeze) ? " freeze" : " thaw");
            ASSERT_MEM_ALLOC(escaped);
            op->scripts[cnt++].path = escaped;
         } else {
            Debug("ignoring non-file entry: %s\n", fileList[i]);
         }
         free(fileList[i]);
      }
      free(fileList);

      if (cnt >= MAX_SCRIPTS) {
         Debug("Too many scripts to run, ignoring past %d.\n", MAX_SCRIPTS);
      }


      /* Start the first script if there are scripts to be executed. */
      if (cnt > 0) {
         op->scripts[0].proc = ProcMgr_ExecAsync(op->scripts[0].path, NULL);
         if (op->scripts[0].proc == NULL) {
            // XXX : log error
            VmBackup_Release((VmBackupOp *) op);
            op = NULL;
         }
      }
   } else {
      Debug("Cannot find script directory.\n");
   }

   return (VmBackupOp *) op;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupOnFreezeScripts --
 *
 *    Run the "on freeze" scripts available in the configuration directory.
 *
 * Result
 *    An object that can be used to track the progress of the operation,
 *    or NULL if an error happened.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

VmBackupOp *
VmBackupOnFreezeScripts(void)
{
   const char *scriptDir;

   scriptDir = VmBackupGetScriptPath();
   return (scriptDir != NULL) ? VmBackupNewScriptOp(scriptDir, TRUE) : NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupOnThawScripts --
 *
 *    Run the "on thaw" scripts available in the configuration directory.
 *
 * Result
 *    An object that can be used to track the progress of the operation.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

VmBackupOp *
VmBackupOnThawScripts(void)
{
   const char *scriptDir;

   scriptDir = VmBackupGetScriptPath();
   return (scriptDir != NULL) ? VmBackupNewScriptOp(scriptDir, FALSE) : NULL;
}

