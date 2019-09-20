/*********************************************************
 * Copyright (C) 2002-2019 VMware, Inc. All rights reserved.
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
 * procMgr.h --
 *
 *    Process management library.
 *
 */


#ifndef __PROCMGR_H__
#   define __PROCMGR_H__

#include "vm_basic_types.h"
#include "auth.h"
#include "dynarray.h"
#if !defined(_WIN32)
#  include <sys/types.h>
#endif
#include <time.h>

/*
 * Keeps track of the platform-specific handle(s) to an asynchronous process.
 */
typedef struct ProcMgr_AsyncProc ProcMgr_AsyncProc;

#if defined(_WIN32)
   typedef DWORD ProcMgr_Pid;
#else /* POSIX */
   typedef pid_t ProcMgr_Pid;
#endif

/*
 * Process information structure.
 * This holds basic information we return per process
 * when listing process information inside the guest.
 */

typedef struct ProcMgrProcInfo {
   ProcMgr_Pid procId;
   char *procCmdName;             // UTF-8
#if defined(__linux__) || defined(_WIN32)
   char *procCmdAbsPath;          // UTF-8
#endif
   char *procCmdLine;             // UTF-8
   char *procOwner;               // UTF-8
#if defined(_WIN32)
   Bool procDebugged;
#endif
   time_t procStartTime;
} ProcMgrProcInfo;

DEFINE_DYNARRAY_TYPE(ProcMgrProcInfo);


typedef struct ProcMgr_ProcArgs {
#if defined(_WIN32)
   /*
    * If a caller needs to use a non-default set of arguments for
    * CreateProcess[AsUser] in ProcMgr_Exec[A]sync, this structure should be used. 
    *
    * - If 'userArgs' is NULL, defaults are used:
    *   - bInheritHandles defaults to TRUE
    *   - lpStartupInfo is instantiated and initialized with:
    *     - cb initialized to size of the object
    *     - dwFlags initialized to STARTF_USESHOWWINDOW
    *     - wShowWindow initialized to SW_MINIMIZE.
    *   - defaults for all other parameters are NULL/FALSE
    *
    * - If 'userArgs' is not NULL, the values in the 'userArgs' object are used
    *   according to the following rules:
    *   - If lpStartupInfo is NULL, it is instantiated and initialized with:
    *     - cb initialized to size of the object
    *     - dwFlags initialized to STARTF_USESHOWWINDOW
    *     - wShowWindow initialized to SW_MINIMIZE.
    *     - The caller would need to do some of this initialization if they set
    *       lpStartupInfo.
    *   - If hToken is set:
    *     - if lpStartupInfo->lpDesktop is not NULL, then it is used directly. Otherwise,
    *       lpStartupInfo->lpDesktop is initialized appropriately.
    *
    *     XXX: Make it more convenient for callers(like ToolsDaemonTcloRunProgramImpl) 
    *     to set just wShowWindow without needing to instantiate and initialize a 
    *     STARTUPINFO object. 
    */
   HANDLE hToken;

   LPCWSTR lpApplicationName;
   LPSECURITY_ATTRIBUTES lpProcessAttributes;
   LPSECURITY_ATTRIBUTES lpThreadAttributes;
   BOOL bInheritHandles;
   DWORD dwCreationFlags;
   LPVOID lpEnvironment;
   LPCWSTR lpCurrentDirectory;
   LPSTARTUPINFO lpStartupInfo;
#else
   /*
    * The environment variables to run the program with. If NULL, use the current
    * environment.
    */
   char **envp;

   /*
    * If non-NULL, the directory to be changed to before the process is
    * started.
    */
   char *workingDirectory;
#endif
} ProcMgr_ProcArgs;




typedef void ProcMgr_Callback(Bool status, void *clientData);

#if defined(_WIN32)
typedef HANDLE Selectable;
#else
typedef int Selectable;
#endif

ProcMgrProcInfoArray *ProcMgr_ListProcesses(void);

#if defined(_WIN32)
ProcMgrProcInfoArray *ProcMgr_ListProcessesEx(void);
#endif

void ProcMgr_FreeProcList(ProcMgrProcInfoArray *procList);
Bool ProcMgr_KillByPid(ProcMgr_Pid procId);


Bool ProcMgr_ExecSync(char const *cmd,       // UTF-8
                      ProcMgr_ProcArgs *userArgs);
ProcMgr_AsyncProc *ProcMgr_ExecAsync(char const *cmd,     // UTF-8
                                     ProcMgr_ProcArgs *userArgs);
#if defined(__linux__)
Bool ProcMgr_ExecSyncWithExitCode(char const *cmd,
                                  ProcMgr_ProcArgs *userArgs,
                                  Bool *validExitCode,
                                  int *exitCode);
#endif
void ProcMgr_Kill(ProcMgr_AsyncProc *asyncProc);
Selectable ProcMgr_GetAsyncProcSelectable(ProcMgr_AsyncProc *asyncProc);
ProcMgr_Pid ProcMgr_GetPid(ProcMgr_AsyncProc *asyncProc);
Bool ProcMgr_IsAsyncProcRunning(ProcMgr_AsyncProc *asyncProc);
int ProcMgr_GetExitCode(ProcMgr_AsyncProc *asyncProc, int *result);
void ProcMgr_Free(ProcMgr_AsyncProc *asyncProc);
#if !defined(_WIN32)
Bool ProcMgr_ImpersonateUserStart(const char *user,      // UTF-8
                                  AuthToken token);
Bool ProcMgr_ImpersonateUserStop(void);
#endif
Bool ProcMgr_GetImpersonatedUserInfo(char **username, char **homeDir);

#endif /* __PROCMGR_H__ */
