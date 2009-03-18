/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * main.c --
 *
 *    Guest application started as a service
 *    Linux and FreeBSD implementation
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>


#include "vmware.h"
#include "toolsDaemon.h"
#include "eventManager.h"
#include "rpcout.h"
#include "rpcin.h"
#include "vm_app.h"
#include "debug.h"
#include "vmsignal.h"
#include "guestApp.h"
#include "vmcheck.h"
#include "util.h"
#include "strutil.h"
#include "str.h"
#include "vm_version.h"
#include "procMgr.h"
#include "system.h"
#include "conf.h"
#include "guestInfo.h"
#include "guestInfoServer.h"
#include "escape.h"
#include "vmstdio.h"
#include "vmBackup.h"
#include "codeset.h"

#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
#include "socketMgr.h"
#endif

/* Header to get prototype of daemon() implementation for Solaris. */
#ifdef sun
#   include "miscSolaris.h"
#endif

#include "procMgr.h"
#include "guestd_version.h"

#include "embed_version.h"
/*
 * XXX VM_EMBED_VERSION is ELF-specific, and Mac OS doesn't grok that.
 */
#ifndef __APPLE__
VM_EMBED_VERSION(GUESTD_VERSION_STRING);
#endif

/*
 * Global constants
 */

#define DEFAULT_PIDFILE             "/var/run/vmware-guestd.pid"
#define EXEC_LOG                    "/var/log/vmware-tools-guestd"
#define UPGRADER_FILENAME           "vmware-tools-upgrader"


/*
 * All signals that:
 * . Can terminate the process
 * . May occur even if the program has no bugs
 */
static int const cSignals[] = {
   SIGHUP,
   SIGINT,
   SIGQUIT,
   SIGTERM,
   SIGUSR1,
   SIGUSR2,
};


/*
 * Global variables
 */


/*
 * What a pity that the signal API doesn't allow to pass a clientData
 * parameter :( --hpreg
 */
static int gDaemonSignal;
static int gCommandLineRpciSignal;


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdCommandLineRpciSignal --
 *
 *    Command line RPCI signal handler
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestdCommandLineRpciSignal(int signal)
{
   Debug("Received signal %d\n", signal);

   /*
    * Sending a command line RPCI doesn't take a long time. Delay the handling
    * of the signal until we have closed the RpcOut object --hpreg
    */

   if (gCommandLineRpciSignal == 0) {
      /*
       * This is the first signal we receive
       */

      ASSERT(signal);
      gCommandLineRpciSignal = signal;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdCommandLineRpci --
 *
 *    Make VMware execute a RPCI string command, and output the string result
 *    on stdout
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdCommandLineRpci(char const *cmd) // IN: RPCI command
{
   struct sigaction olds[ARRAYSIZE(cSignals)];
   char *result = NULL;
   Bool status = FALSE;
   Bool signaled = FALSE;

   gCommandLineRpciSignal = 0;

   if (Signal_SetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals),
                              GuestdCommandLineRpciSignal) == 0) {
      return FALSE;
   }

   status = RpcOut_sendOne(&result, NULL, "%s", cmd);

   if (gCommandLineRpciSignal) {
      fprintf(stderr, "Interrupted by signal %d.\n\n", gCommandLineRpciSignal);
      signaled = TRUE;
   }

   if ((Signal_ResetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals)) == 0) ||
       signaled) {
      status = FALSE;
   } else if (!status) {
      fprintf(stderr, "%s\n", result ? result : "NULL");
   } else {
      printf("%s\n", result);
   }

   free(result);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdDaemonSignal --
 *
 *    Daemon signal handler
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestdDaemonSignal(int signal)
{
   Debug("Received signal %d\n", signal);

   /*
    * This code can be executed at any time, and can preempt the "normal" flow
    * of execution.
    *
    * To avoid re-entrancy and concurrency issues in the "normal" code, we
    * defer the handling of the signal until we are in a well-known context
    *
    *   --hpreg
    */

   if (gDaemonSignal == 0) {
      /*
       * This is the first signal we receive
       */

      ASSERT(signal);
      gDaemonSignal = signal;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdExecute --
 *
 *      Callback-able wrappers to execute halt/reboot commands.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdExecute(void *clientData, const char *confName)
{
   GuestApp_Dict **pConfDict = (GuestApp_Dict **) clientData;
   const char *cmd;

   ASSERT(confName);
   ASSERT(pConfDict);
   ASSERT(*pConfDict);

   cmd = GuestApp_GetDictEntry(*pConfDict, confName);
   ASSERT(cmd);

   return ProcMgr_ExecSync(cmd, NULL);
}

static Bool
GuestdExecuteHalt(void *clientData)
{
   return GuestdExecute(clientData, CONFNAME_HALT);
}

static Bool
GuestdExecuteReboot(void *clientData)
{
   return GuestdExecute(clientData, CONFNAME_REBOOT);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdCreateUpgrader --
 *
 *      Spawn a new process that manages the auto-upgrade procedure.
 *
 * Results:
 *      Return value specifies result of fork/exec.
 *
 * Side effects:
 *      Spawns a new process.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdCreateUpgrader(char const **result,     // OUT
                     size_t *resultLen,       // OUT
                     const char *name,        // Ignored
                     const char *args,        // Ignored
                     size_t argsSize,         // Ignored
                     void *clientData)        // Ignored
{
   int32 version;
   unsigned int index = 0;
   const char *upgraderPath = NULL;
   char *upgraderCmd = NULL;
   ProcMgr_AsyncProc *upgraderProc = NULL;

   if (StrUtil_GetNextIntToken(&version, &index, args, " ")) {
      /* New protocol. Host sent 'upgrader.create <version>' */
      if (version == 1) {
         upgraderPath = ToolsDaemon_GetGuestTempDirectory();
         if (upgraderPath == NULL) {
            Log("ToolsDaemon_GetGuestTempDirectory failed.\n");
            return RpcIn_SetRetVals(result, resultLen,
                                    "ToolsDaemon_GetGuestTempDirectory failed", FALSE);
         }

         upgraderCmd = Str_Asprintf(NULL, "bash %s%srun_upgrader.sh",
                                    upgraderPath, DIRSEPS);
         if (upgraderCmd == NULL) {
            Log("Str_Asprintf failed.\n");
            return RpcIn_SetRetVals(result, resultLen,
                                    "Str_Asprintf failed", FALSE);
         }
      } else {
         return RpcIn_SetRetVals(result, resultLen, "Unknown protocol version", FALSE);
      }
   } else {
      /* Old protocol. Host sent 'upgrader.create' */
      upgraderCmd = Util_SafeStrdup(UPGRADER_FILENAME);
   }

   upgraderProc = ProcMgr_ExecAsync(upgraderCmd, NULL);
   free(upgraderCmd);
   if (upgraderProc == NULL) {
      Warning("Failed to start upgrader.\n");
      return RpcIn_SetRetVals(result, resultLen, "ProcMgr_ExecAsync failed", FALSE);
   }
   ProcMgr_Free(upgraderProc);
   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdSleep --
 *
 *      Sleep in a non-blocking way for the given number of
 *      micro-seconds. The callback is called if the async proc
 *      exits.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestdSleep(uint64 numUsecs,                 // IN
            const ToolsDaemon_Data *tdData)  // IN
{
   static ProcMgr_AsyncProc *curAsyncProc = NULL;
   static int asyncFd;              /*
                                     * Fd for asyncProc. Only meaningful
                                     * if (curAsyncProc != NULL).
                                     */
   int maxFd;                       /* Max fd of all Fd sets */
   fd_set readFds;                  /* Read fd set to select on */
   fd_set writeFds;
#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
   SocketSelectable *sockReadFds = NULL;
   SocketSelectable *sockWriteFds = NULL;
   int numSockReadFds = 0;
   int numSockWriteFds = 0;
   int index;
#endif
   struct timeval tv;
   int status;

   ASSERT(tdData);

   /* If there is a new async proc, init the fd set & fd max */
   if (tdData->asyncProc && !curAsyncProc) {
      asyncFd = ProcMgr_GetAsyncProcSelectable(tdData->asyncProc);
      curAsyncProc = tdData->asyncProc;
   } else {
      /*
       * Make sure the caller doesn't try to change the asyncProc before
       * its fd has been selected.
       */
      ASSERT(tdData->asyncProc == curAsyncProc);
   }

   /* Init readFds & writeFds */
   FD_ZERO(&readFds);
   FD_ZERO(&writeFds);
   maxFd = -1;

   if (curAsyncProc) {
      FD_SET(asyncFd, &readFds);
      maxFd = asyncFd;
   }

#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
   SocketMgr_GetSelectables(SOCKETMGR_IN,
                            &sockReadFds,
                            &numSockReadFds);
   for (index = 0; index < numSockReadFds; index++) {
      FD_SET(sockReadFds[index], &readFds);
      if (sockReadFds[index] > maxFd) {
         maxFd = sockReadFds[index];
      }
   }

   SocketMgr_GetSelectables(SOCKETMGR_OUT,
                            &sockWriteFds,
                            &numSockWriteFds);
   for (index = 0; index < numSockWriteFds; index++) {
      FD_SET(sockWriteFds[index], &writeFds);
      if (sockWriteFds[index] > maxFd) {
         maxFd = sockWriteFds[index];
      }
   }
#endif

   tv.tv_sec = numUsecs / 1000000L;
   tv.tv_usec = numUsecs % 1000000L;

   status = select(maxFd + 1, &readFds, &writeFds, NULL, &tv);

   if (status == -1) {
      Debug("Select encountered an error: %s\n", strerror(errno));
   } else if (status > 0) {
      Debug("Select returned status > 0\n");

      if (curAsyncProc && FD_ISSET(asyncFd, &readFds)) {
         /* The async proc fd was written to */

         Bool ret;

         ASSERT(tdData->asyncProcCb);

         if (!ProcMgr_GetAsyncStatus(curAsyncProc, &ret)) {
            ret = FALSE;
            Debug("Failed to get return status for async process.\n");
         }

         tdData->asyncProcCb(ret, tdData->asyncProcCbData);
         Debug("Done executing asynchronous cmd\n");

         /* Reinitialize */
         curAsyncProc = NULL;
      }

#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
      for (index = 0; index < numSockReadFds; index++) {
         if (FD_ISSET(sockReadFds[index], &readFds)) {
            SocketMgr_ProcessSelectable(sockReadFds[index], SOCKETMGR_IN);
         }
      }

      for (index = 0; index < numSockWriteFds; index++) {
         if (FD_ISSET(sockWriteFds[index], &writeFds)) {
            SocketMgr_ProcessSelectable(sockWriteFds[index], SOCKETMGR_OUT);
         }
      }
#endif
   }

#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
   free((void *) sockReadFds);
   free((void *) sockWriteFds);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdDaemon --
 *
 *    Setup a TCLO channel with VMware, and run the event loop
 *
 * Return value:
 *    TRUE on normal exit (when sent SIGTERM)
 *    FALSE otherwise (detail is displayed)
 *
 * Side effects:
 *    if a signal is trapped, gDaemonSignalPtr is given its value
 *      & the method exits
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdDaemon(GuestApp_Dict **pConfDict,       // IN/OUT
             int *gDaemonSignalPtr)           // IN/OUT
{
   Bool guestInfoEnabled = FALSE;
   ToolsDaemon_Data *data = NULL;
   VmBackupSyncProvider *syncProvider = NULL;

   data = ToolsDaemon_Init(pConfDict, EXEC_LOG,
                           GuestdExecuteHalt, pConfDict,
                           GuestdExecuteReboot, pConfDict,
                           NULL, NULL,
                           NULL, NULL,
                           NULL, NULL);
   if (!data) {
      Warning("Unable to start guestd.\n");
      goto out;
   }

   ASSERT(data->in);

   /* Set up the guest info 'server'. */
   if (!GuestInfoServer_Init(ToolsDaemonEventQueue)) {
      Warning("Unable to start guest info server.\n");
   } else {
      guestInfoEnabled = TRUE;
      GuestInfoServer_DisableDiskInfoQuery(
         GuestApp_GetDictEntryBool(*pConfDict, CONFNAME_DISABLEQUERYDISKINFO));
   }

   /*
    * Start listening for VMX requests to create the upgrader.
    */
   RpcIn_RegisterCallback(data->in, "upgrader.create",
                          GuestdCreateUpgrader, NULL);

   /*
    * Initialize the vmbackup subsystem, if it's supported in the current
    * platform.
    */
   syncProvider = VmBackup_NewSyncDriverProvider();
   if (syncProvider != NULL) {
      Bool loggingEnabled = GuestApp_GetDictEntryBool(*pConfDict, CONFNAME_LOG);
      VmBackup_Init(data->in, ToolsDaemonEventQueue, syncProvider,
                    loggingEnabled);
   } else {
      Debug("No vmBackup implementation available!\n");
   }

   /*
    * Event loop
    */

   for (;;) {
      int nr = 0;
      uint64 sleepUsecs = 0;

      nr = EventManager_ProcessNext(ToolsDaemonEventQueue, &sleepUsecs);
      if (nr != 1) {
         fprintf(stderr,
                 "Unexpected end of the main loop: returned value is %d\n",
                 nr);
         goto out;
      }

      /* Reap our zombie children. */
      waitpid(-1, NULL, WNOHANG);

      if (*gDaemonSignalPtr) {
         /*
          * We are in a well-known context: the processing of the previous
          * event is done, and we haven't started to process the next event.
          *
          * In particular, if the previous event handler executed a TCLO
          * command that ended up sending a signal to us, we are sure that the
          * reply message for that command has been crafted. So it is the right
          * time to stop 'in', which will send this last reply back to VMware.
          *
          *   --hpreg
          */

         fprintf(stderr, "Interrupted by signal %d.\n\n", *gDaemonSignalPtr);
         goto out;
      }

      if (!ToolsDaemon_CheckReset(data, &sleepUsecs)) {
         goto out;
      }
      GuestdSleep(sleepUsecs, data);
   }

   NOT_REACHED();

out:
   if (guestInfoEnabled) {
      GuestInfoServer_Cleanup();
   }

   if (syncProvider != NULL) {
      VmBackup_Shutdown(data->in);
   }

   if (data) {
      ToolsDaemon_Cleanup(data);
   }

   return *gDaemonSignalPtr == SIGTERM ? TRUE : FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdDaemonWrapper --
 *
 *    Wrap the call to GuestdDaemon so the signal handler gets both set
 *    & reset.
 *
 * Return value:
 *    TRUE on success (never happens)
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdDaemonWrapper(GuestApp_Dict **pConfDict) // IN
{
   Bool returnVal;
   struct sigaction olds[ARRAYSIZE(cSignals)];

   gDaemonSignal = 0;

   /*
    * Do not check return value.
    * setsid() only fails if we are already process group leader.
    */
   setsid();

   if (Signal_SetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals),
                              GuestdDaemonSignal) == 0) {
      return FALSE;
   }

   returnVal = GuestdDaemon(pConfDict, &gDaemonSignal);

   if (Signal_ResetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals)) == 0) {
      return FALSE;
   }

   return returnVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdAlreadyRunning --
 *
 *    Check if there is an instance of guestd already running.
 *
 *    Note that we used to use pgrep(1) but that approach produces false
 *    positives when the init script that starts guestd has the same name as
 *    the guestd binary, as is done for open-vm-tools packages.
 *
 * Return value:
 *    TRUE if there is another guestd running.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdAlreadyRunning(char const *pidFileName) // IN
{
   FILE *pidFile = NULL;
   pid_t pid = 0;

   ASSERT(pidFileName);

   pidFile = fopen(pidFileName, "r");
   if (pidFile) {
      int ret;
      ret = fscanf(pidFile, "%"FMTPID"\n", &pid);
      fclose(pidFile);
      if (ret != 1) {
        return FALSE;
      }

      /*
       * XXX There is an assumption that if the process with pid is alive,
       * the process is just guestd. Actually the process name should be
       * also checked because it is possible that there is another process
       * with same pid. 2 reasons it is not checked. First we can not find
       * a cross-platform method to check the process name. Second is that
       * the possibility is very low in our case because the PID file should
       * always be with guestd process. Even user manually kills the guestd,
       * the PID file will also be removed. Perhaps longer term we should
       * add a function like System_GetProcessName(pid_t) to
       * bora-vmsoft/lib/system that will hide the platform-specific
       * messiness.
       */
      if (pid != getpid() && kill(pid, 0) == 0) {
         return TRUE;
      }
      /*
       * If process with pid is dead, the PID file will be removed. If pid
       * is same as getpid(), PID file will also be removed.
       */
      unlink(pidFileName);
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdUsage --
 *
 *   Write an help message on stderr and exit
 *
 * Results:
 *   None
 *
 * Side effects:
 *   None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestdUsage(char const *prog, // IN: Name of this program
            int exitCode)     // IN
{
   fprintf(stderr,
      "Usage:\n"
      "\n"
      "   %s --help\n"
      "      Display this help message\n"
      "\n"
      "   %s --cmd <command>\n"
      "      Make the %s execute a command\n"
      "\n"
      "   %s\n"
      "      Run in daemon mode\n"
      "\n"
      "      [--background <PID file>]\n"
      "         Start in the background, and write the PID of the background\n"
      "         process in a file.\n"
      "\n"
      "      [--halt-command <command>]\n"
      "         Command to use to halt the system.\n"
      "         The default is \"%s\"\n"
      "\n"
      "      [--reboot-command <command>]\n"
      "         Command to use to reboot the system.\n"
      "         The default is \"%s\"\n"
      "\n",
      prog, prog, PRODUCT_LINE_NAME, prog, CONFVAL_HALT_DEFAULT,
      CONFVAL_REBOOT_DEFAULT);

   exit(exitCode);
}


/*
 *-----------------------------------------------------------------------------
 *
 * main --
 *
 *   Run the program: parse the command line and do the requested job
 *
 * Results:
 *   Exit code
 *
 * Side effects:
 *   None
 *
 *-----------------------------------------------------------------------------
 */

int
main(int argc,    // IN: Number of command line arguments
     char **argv) // IN: Array of arguments
{
   Bool retVal;
   Bool rpci;
   Bool help;
   char const *pidFile;
   GuestApp_Dict *confDict;

   int index;
   Bool parseOptions;
   const char *arguments[1];
   int argumentIndex;
   int expectedArgumentIndex;

   if (!VmCheck_IsVirtualWorld()) {
#ifndef ALLOW_TOOLS_IN_FOREIGN_VM
      Warning("The VMware service must be run from within a virtual machine.\n");
      return 0;
#else
      runningInForeignVM = TRUE;
#endif
   }

   confDict = Conf_Load();

   /*
    * Parse the command line.
    *
    * We do it by hand because getopt() doesn't handle long options, and
    * getopt_long is a GNU extension --hpreg
    *
    * argv[0] is the program name, as usual
    */

   /*
    * Optional arguments
    */

   /* Default values */
   rpci = FALSE;
   help = FALSE;
   pidFile = NULL;

   argumentIndex = 0;
   parseOptions = TRUE;
   for (index = 1; index < argc; index++) {
      Bool isOption;

      if (parseOptions) {
         if (strcmp(argv[index], "--") == 0) {
            /*
             * Special options to specify the end of options (in order to pass
             * arguments that begin with '-'
             */

            parseOptions = FALSE;
            continue;
	 }

         isOption = strncmp(argv[index], "-", 1) == 0;
      } else {
         isOption = FALSE;
      }

      if (isOption) {
	 char const *option;

         option = argv[index] + 1;

         if (strcmp(option, "-cmd") == 0) {
            rpci = TRUE;
	 } else if (strcmp(option, "-help") == 0) {
            help = TRUE;
	 } else if (strcmp(option, "-background") == 0) {
            if (index + 1 == argc) {
               fprintf(stderr, "The \"%s\" option on the command line requires an "
                       "argument.\n\n", option);
               GuestdUsage(argv[0], 1);
            }

            index++;
            pidFile = argv[index];
	 } else if (strcmp(option, "-halt-command") == 0) {
            if (index + 1 == argc) {
               fprintf(stderr, "The \"%s\" option on the command line requires an "
                       "argument.\n\n", option);
               GuestdUsage(argv[0], 1);
            }

            index++;
            GuestApp_SetDictEntry(confDict, CONFNAME_HALT, argv[index]);
	 } else if (strcmp(option, "-reboot-command") == 0) {
            if (index + 1 == argc) {
               fprintf(stderr, "The \"%s\" option on the command line requires an "
                       "argument.\n\n", option);
               GuestdUsage(argv[0], 1);
            }

            index++;
            GuestApp_SetDictEntry(confDict, CONFNAME_REBOOT, argv[index]);
	 } else {
            fprintf(stderr, "Invalid \"%s\" option on the command line.\n\n", option);
            GuestdUsage(argv[0], 1);
	 }
      } else {
         if (argumentIndex >= ARRAYSIZE(arguments)) {
            fprintf(stderr, "Too many mandatory argument(s) on the command line. The "
                    "maximum is %"FMTSZ"u.\n\n", ARRAYSIZE(arguments));
            GuestdUsage(argv[0], 1);
         }

	 arguments[argumentIndex++] = argv[index];
      }
   }

   /*
    * Mandatory arguments
    */

   if (rpci) {
      expectedArgumentIndex = 1;
   } else {
      expectedArgumentIndex = 0;
   }

   if (argumentIndex != expectedArgumentIndex) {
      fprintf(stderr, "Incorrect number of mandatory argument(s) "
                      "on the command line: %u instead of %u.\n\n",
                      argumentIndex, expectedArgumentIndex);
      GuestdUsage(argv[0], 1);
   }

   /*
    * Do the requested job
    */

   if (help) {
      GuestdUsage(argv[0], 0);
   }

   if (rpci) {
      return GuestdCommandLineRpci(arguments[0]) ? 0 : 1;
   }


   /*
    * We must (attempt to) check for another instance running, even when the
    * '--background <PID file>' option wasn't specified (fix for bug 8098).
    * In such cases, we'll assume that the PID file can be found at
    * DEFAULT_PIDFILE, which should work for Linux, Solaris, and FreeBSD guests.
    */
   if (GuestdAlreadyRunning(pidFile ? pidFile : DEFAULT_PIDFILE)) {
      fprintf(stderr, "Guestd is already running, exiting.\n");
      GuestApp_FreeDict(confDict);
      /*
       * Here we still should return 0 otherwise if vmware-tools.sh
       * get an error return, it will ask user to run config.pl
       * again. We should quit here silently.
       */
      exit(0);
   }

   if (pidFile) {
      if (!System_Daemon(FALSE, FALSE, pidFile)) {
         fprintf(stderr, "Unable to daemonize: %s\n", strerror(errno));
         GuestApp_FreeDict(confDict);
         exit(1);
      }
   }

   retVal = GuestdDaemonWrapper(&confDict) ? 0 : 1;

   if (pidFile) {
      unlink(pidFile);
   }

   GuestApp_FreeDict(confDict);

   return retVal;
}
