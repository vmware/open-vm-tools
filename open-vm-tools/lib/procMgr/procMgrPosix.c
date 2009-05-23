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
 * procMgrPosix.c --
 *
 *    Posix implementation of the process management lib
 *
 */

#ifndef VMX86_DEVEL

#endif

// pull in setresuid()/setresgid() if possible
#define  _GNU_SOURCE
#include <unistd.h>
#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
#include <asm/param.h>
#include <locale.h>
#include <sys/stat.h>
#endif
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include <grp.h>
#include <sys/syscall.h>
#if defined(linux) || defined(HAVE_SYS_USER_H)
// sys/param.h is required on FreeBSD before sys/user.h
#   include <sys/param.h>
// Pull in PAGE_SIZE/PAGE_SHIFT defines ahead of vm_basic_defs.h
#   include <sys/user.h>
#endif

#include "vmware.h"
#include "procMgr.h"
#include "vm_assert.h"
#include "debug.h"
#include "util.h"
#include "msg.h"
#include "vmsignal.h"
#undef offsetof
#include "file.h"
#include "dynbuf.h"
#include "su.h"
#include "str.h"
#include "fileIO.h"
#include "codeset.h"


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
 * Keeps track of the posix async proc info.
 */
struct ProcMgr_AsyncProc {
   pid_t waiterPid;          // pid of the waiter process
   pid_t resultPid;          // pid of the process created for the client
   FileIODescriptor fd;      // fd to write to when the child is done
   Bool validExitCode;
   int exitCode;
};

static pid_t ProcMgrStartProcess(char const *cmd);

static Bool ProcMgrWaitForProcCompletion(pid_t pid,
                                         Bool *validExitCode,
                                         int *exitCode);

static Bool ProcMgrKill(pid_t pid,
                        int sig,
                        int timeout);

#if defined(linux) && !defined(GLIBC_VERSION_23)
/*
 * Implements the system calls (they are not wrapped by glibc til 2.3.2).
 *
 * The _syscall3 macro from the Linux kernel headers is not PIC-safe.
 * See: http://bugzilla.kernel.org/show_bug.cgi?id=7302
 *
 * (In fact, newer Linux kernels don't even define _syscall macros anymore.)
 */

static INLINE int
setresuid(uid_t ruid,
          uid_t euid,
          uid_t suid)
{
   return syscall(__NR_setresuid, ruid, euid, suid);
}


static INLINE int
setresgid(gid_t ruid,
          gid_t euid,
          gid_t suid)
{
   return syscall(__NR_setresgid, ruid, euid, suid);
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_ListProcesses --
 *
 *      List all the processes that the calling client has privilege to
 *      enumerate. The strings in the returned structure should be all
 *      UTF-8 encoded, although we do not enforce it right now.
 *
 * Results:
 *      
 *      A ProcMgr_ProcList.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

ProcMgr_ProcList *
ProcMgr_ListProcesses(void)
{
   ProcMgr_ProcList *procList = NULL;
#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
   Bool failed = FALSE;
   DynBuf dbProcId;
   DynBuf dbProcCmd;
   DynBuf dbProcStartTime;
   DynBuf dbProcOwner;
   DIR *dir;
   struct dirent *ent;
   static time_t hostStartTime = 0;
   static unsigned long long hertz = 100;
   int numberFound;

   DynBuf_Init(&dbProcId);
   DynBuf_Init(&dbProcCmd);
   DynBuf_Init(&dbProcStartTime);
   DynBuf_Init(&dbProcOwner);

   /*
    * Figure out when the system started.  We need this number to
    * compute process start times, which are relative to this number.
    * We grab the first float in /proc/uptime, convert it to an integer,
    * and then subtract that from the current time.  That leaves us
    * with the seconds since epoch that the system booted up.
    */
   if (0 == hostStartTime) {
      FILE *uptimeFile = NULL;

      uptimeFile = fopen("/proc/uptime", "r");
      if (NULL != uptimeFile) {
         double secondsSinceBoot;
         char *realLocale;

         /*
          * Set the locale such that floats are delimited with ".".
          */
         realLocale = setlocale(LC_NUMERIC, NULL);
         setlocale(LC_NUMERIC, "C");
         numberFound = fscanf(uptimeFile, "%lf", &secondsSinceBoot);
         setlocale(LC_NUMERIC, realLocale);

         /*
          * Figure out system boot time in absolute terms.
          */
         if (numberFound) {
            hostStartTime = time(NULL) - (time_t) secondsSinceBoot;
         }
         fclose(uptimeFile);
      }

      /*
       * Figure out the "hertz" value, which may be radically
       * different than the actual CPU frequency of the machine.
       * The process start time is expressed in terms of this value,
       * so let's compute it now and keep it in a static variable.
       */
#ifdef HZ
      hertz = (unsigned long long) HZ;
#else
      /*
       * Don't do anything.  Use the default value of 100.
       */
#endif
   } // if (0 == hostStartTime)

   /*
    * Scan /proc for any directory that is all numbers.
    * That represents a process id.
    */
   dir = opendir("/proc");
   if (NULL == dir) {
      Warning("ProcMgr_ListProcesses unable to open /proc\n");
      failed = TRUE;
      goto abort;
   }

   while ((ent = readdir(dir))) {
      struct stat fileStat;
      char cmdFilePath[1024];
      int statResult;
      int numRead = 0;   /* number of bytes that read() actually read */
      int cmdFd;
      pid_t pid;
      int replaceLoop;
      struct passwd *pwd;
      char cmdLineTemp[2048];
      char cmdStatTemp[2048];
      char *cmdLine;
      char *userName = NULL;
      size_t strLen = 0;
      unsigned long long dummy;
      unsigned long long relativeStartTime;
      char *stringBegin;
      time_t processStartTime;

      /*
       * We only care about dirs that look like processes.
       */
      if (strspn(ent->d_name, "0123456789") != strlen(ent->d_name)) {
         continue;
      }

      if (snprintf(cmdFilePath,
                   sizeof cmdFilePath,
                   "/proc/%s/cmdline",
                   ent->d_name) == -1) {
         Debug("Giant process id '%s'\n", ent->d_name);
         continue;
      }
      
      cmdFd = open(cmdFilePath, O_RDONLY);
      if (-1 == cmdFd) {
         /*
          * We may not be able to open the file due to the security reason.
          * In that case, just ignore and continue.
          */
         continue;
      }

      /*
       * Read in the command and its arguments.  Arguments are separated
       * by \0, which we convert to ' '.  Then we add a NULL terminator
       * at the end.  Example: "perl -cw try.pl" is read in as
       * "perl\0-cw\0try.pl\0", which we convert to "perl -cw try.pl\0".
       * It would have been nice to preserve the NUL character so it is easy
       * to determine what the command line arguments are without
       * using a quote and space parsing heuristic.  But we do this
       * to have parity with how Windows reports the command line.
       * In the future, we could keep the NUL version around and pass it
       * back to the client for easier parsing when retrieving individual
       * command line parameters is needed.
       *
       * We read at most (sizeof cmdLineTemp) - 1 bytes to leave room
       * for NUL termination at the end.
       */
      numRead = read(cmdFd, cmdLineTemp, sizeof cmdLineTemp - sizeof(char));
      close(cmdFd);

      if (numRead > 0) {
         for (replaceLoop = 0 ; replaceLoop < (numRead - 1) ; replaceLoop++) {
            if ('\0' == cmdLineTemp[replaceLoop]) {
               cmdLineTemp[replaceLoop] = ' ';
            }
         }
      } else {
         /*
          * Some procs don't have a command line text, so read a name from
          * the 'status' file (should be the first line). If unable to get a name,
          * the process is still real, so it should be included in the list, just 
          * without a name.
          */
         cmdFd = -1;
         numRead = 0;

         if (snprintf(cmdFilePath,
                      sizeof cmdFilePath,
                      "/proc/%s/status",
                      ent->d_name) != -1) {
            cmdFd = open(cmdFilePath, O_RDONLY);
         }
         if (cmdFd != -1) {
            numRead = read(cmdFd, cmdLineTemp, sizeof(cmdLineTemp) - sizeof(char));
            close(cmdFd);

            if (numRead < 0) {
               cmdLineTemp[0] = '\0';
            } else {
               cmdLineTemp[numRead] = '\0';
            }
         }
         if (numRead > 0) {
            /* 
             * Extract the part with just the name, by reading until the first
             * space, then reading the next non-space word after that, and
             * ignoring everything else. The format looks like this:
             *     "^Name:[ \t]*(.*)$"
             * for example:
             *     "Name:    nfsd"
             */
            const char* nameStart = NULL;
            char* copyItr = NULL;

            /* Skip non-whitespace. */
            for (nameStart = cmdLineTemp; *nameStart && 
                                          *nameStart != ' ' &&
                                          *nameStart != '\t' &&
                                          *nameStart != '\n'; ++nameStart);
            /* Skip whitespace. */
            for (;*nameStart && 
                  (*nameStart == ' ' ||
                   *nameStart == '\t' ||
                   *nameStart == '\n'); ++nameStart);
            /* Copy the name to the start of the string and null term it. */
            for (copyItr = cmdLineTemp; *nameStart && *nameStart != '\n';) {
               *(copyItr++) = *(nameStart++);
            }
            *copyItr = '\0';
         }
      }

      /*
       * There is an edge case where /proc/#/cmdline does not NUL terminate
       * the command.  /sbin/init (process 1) is like that on some distros.
       * So let's guarantee that the string is NUL terminated, even if
       * the last character of the string might already be NUL.
       * This is safe to do because we read at most (sizeof cmdLineTemp) - 1
       * bytes from /proc/#/cmdline -- we left just enough space to add
       * NUL termination at the end.
       */
      if (numRead < 0) {
         cmdLineTemp[0] = '\0';
      } else {
         cmdLineTemp[numRead] = '\0';
      }

      /*
       * Get the inode information for this process.  This gives us
       * the process owner.
       */
      if (snprintf(cmdFilePath,
                   sizeof cmdFilePath,
                   "/proc/%s",
                   ent->d_name) == -1) {
         Debug("Giant process id '%s'\n", ent->d_name);
         continue;
      }

      /*
       * stat() /proc/<pid> to get the owner.  We use fileStat.st_uid
       * later in this code.  If we can't stat(), ignore and continue.
       * Maybe we don't have enough permission.
       */
      statResult = stat(cmdFilePath, &fileStat);
      if (0 != statResult) {
         continue;
      }

      /*
       * Figure out the process start time.  Open /proc/<pid>/stat
       * and read the start time and compute it in absolute time.
       */
      if (snprintf(cmdFilePath,
                   sizeof cmdFilePath,
                   "/proc/%s/stat",
                   ent->d_name) == -1) {
         Debug("Giant process id '%s'\n", ent->d_name);
         continue;
      }
      cmdFd = open(cmdFilePath, O_RDONLY);
      if (-1 == cmdFd) {
         continue;
      }
      numRead = read(cmdFd, cmdStatTemp, sizeof cmdStatTemp);
      close(cmdFd);
      if (0 >= numRead) {
         continue;
      }
      /*
       * Skip over initial process id and process name.  "123 (bash) [...]".
       */
      stringBegin = strchr(cmdStatTemp, ')') + 2;
      
      numberFound = sscanf(stringBegin, "%c %d %d %d %d %d "
                           "%lu %lu %lu %lu %lu %Lu %Lu %Lu %Lu %ld %ld "
                           "%d %ld %Lu",
                           (char *) &dummy, (int *) &dummy, (int *) &dummy,
                           (int *) &dummy, (int *) &dummy,  (int *) &dummy,
                           (unsigned long *) &dummy, (unsigned long *) &dummy,
                           (unsigned long *) &dummy, (unsigned long *) &dummy,
                           (unsigned long *) &dummy,
                           (unsigned long long *) &dummy,
                           (unsigned long long *) &dummy,
                           (unsigned long long *) &dummy,
                           (unsigned long long *) &dummy,
                           (long *) &dummy, (long *) &dummy,
                           (int *) &dummy, (long *) &dummy,
                           &relativeStartTime);
      if (20 != numberFound) {
         continue;
      }
      processStartTime = hostStartTime + (relativeStartTime / hertz);

      /*
       * Store the command line string pointer in dynbuf.
       */
      cmdLine = strdup(cmdLineTemp);
      DynBuf_Append(&dbProcCmd, &cmdLine, sizeof cmdLine);

      /*
       * Store the pid in dynbuf.
       */
      pid = (pid_t) atoi(ent->d_name);
      DynBuf_Append(&dbProcId, &pid, sizeof pid);

      /*
       * Store the owner of the process.
       */
      pwd = getpwuid(fileStat.st_uid);
      userName = (NULL == pwd)
                 ? Str_Asprintf(&strLen, "%d", (int) fileStat.st_uid)
                 : Util_SafeStrdup(pwd->pw_name);
      DynBuf_Append(&dbProcOwner, &userName, sizeof userName);

      /*
       * Store the time that the process started.
       */
      DynBuf_Append(&dbProcStartTime,
                    &processStartTime,
                    sizeof processStartTime);
   } // while readdir

   closedir(dir);

   if (0 == DynBuf_GetSize(&dbProcId)) {
      failed = TRUE;
      goto abort;
   }

   /*
    * We're done adding to DynBuf.  Trim off any unused allocated space.
    * DynBuf_Trim() followed by DynBuf_Detach() avoids a memcpy().
    */
   DynBuf_Trim(&dbProcId);
   DynBuf_Trim(&dbProcCmd);
   DynBuf_Trim(&dbProcStartTime);
   DynBuf_Trim(&dbProcOwner);
   /*
    * Create a ProcMgr_ProcList and populate its fields.
    */
   procList = (ProcMgr_ProcList *) calloc(1, sizeof(ProcMgr_ProcList));
   ASSERT_NOT_IMPLEMENTED(procList);

   procList->procCount = DynBuf_GetSize(&dbProcId) / sizeof(pid_t);

   procList->procIdList = (pid_t *) DynBuf_Detach(&dbProcId);
   ASSERT_NOT_IMPLEMENTED(procList->procIdList);
   procList->procCmdList = (char **) DynBuf_Detach(&dbProcCmd);
   ASSERT_NOT_IMPLEMENTED(procList->procCmdList);
   procList->startTime = (time_t *) DynBuf_Detach(&dbProcStartTime);
   ASSERT_NOT_IMPLEMENTED(procList->startTime);
   procList->procOwnerList = (char **) DynBuf_Detach(&dbProcOwner);
   ASSERT_NOT_IMPLEMENTED(procList->procOwnerList);

abort:
   DynBuf_Destroy(&dbProcId);
   DynBuf_Destroy(&dbProcCmd);
   DynBuf_Destroy(&dbProcStartTime);
   DynBuf_Destroy(&dbProcOwner);

   if (failed) {
      ProcMgr_FreeProcList(procList);
      procList = NULL;
   }
#endif // !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)

   return procList;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_FreeProcList --
 *
 *      Free the memory occupied by ProcMgr_ProcList.
 *
 * Results:
 *      
 *      None.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
ProcMgr_FreeProcList(ProcMgr_ProcList *procList)
{
   int i;

   if (NULL == procList) {
      return;
   }

   for (i = 0; i < procList->procCount; i++) {
      free(procList->procCmdList[i]);
      free(procList->procOwnerList[i]);
   }

   free(procList->procIdList);
   free(procList->procCmdList);
   free(procList->startTime);
   free(procList->procOwnerList);
   free(procList);
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_ExecSync --
 *
 *      Synchronously execute a command. The command is UTF-8 encoded.
 *
 * Results:
 *      TRUE on success (the program had an exit code of 0)
 *      FALSE on failure or if an error occurred (detail is displayed)
 *
 * Side effects:
 *	Lots, depending on the program.
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_ExecSync(char const *cmd,                  // IN: UTF-8 command line
                 ProcMgr_ProcArgs *userArgs)       // IN: Unused
{
   pid_t pid;

   Debug("Executing sync command: %s\n", cmd);

   pid = ProcMgrStartProcess(cmd);

   if (pid == -1) {
      return FALSE;
   }

   return ProcMgrWaitForProcCompletion(pid, NULL, NULL); 
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgrStartProcess --
 *
 *      Fork and execute a command using the shell. This function returns
 *      immediately after the fork() in the parent process.
 *
 * Results:
 *      The pid of the forked process, or -1 on an error.
 *
 * Side effects:
 *	Lots, depending on the program
 *
 *----------------------------------------------------------------------
 */

static pid_t 
ProcMgrStartProcess(char const *cmd)            // IN: UTF-8 encoded cmd
{
   pid_t pid;
   char *cmdCurrent = NULL;

   if (cmd == NULL) {
      ASSERT(FALSE);
      return -1;
   }

   if (!CodeSet_Utf8ToCurrent(cmd, strlen(cmd), &cmdCurrent, NULL)) {
      Warning("Could not convert from UTF-8 to current\n");
      return -1;
   }

   pid = fork();

   if (pid == -1) {
      Warning("Unable to fork: %s.\n\n", strerror(errno));
   } else if (pid == 0) {

      /*
       * Child
       */

      execl("/bin/sh", "sh", "-c", cmdCurrent, (char *)NULL);

      /* Failure */
      Panic("Unable to execute the \"%s\" shell command: %s.\n\n",
            cmdCurrent, strerror(errno));
   }

   /*
    * Parent
    */

   free(cmdCurrent);
   return pid;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgrWaitForProcCompletion --
 *
 *      Waits until the process identified by 'pid' exits or is otherwise
 *      terminated.
 *
 * Results:
 *      TRUE on success (the program had an exit code of 0)
 *      FALSE on failure or if an error occurred (detail is displayed)
 *
 * Side effects:
 *	Prevents zombification of the process.
 *
 *----------------------------------------------------------------------
 */

static Bool 
ProcMgrWaitForProcCompletion(pid_t pid,                 // IN
                             Bool *validExitCode,       // OUT: Optional
                             int *exitCode)             // OUT: Optional
{
   Bool retVal;
   int childStatus;

   if (NULL != validExitCode) {
      *validExitCode = FALSE;
   }
   
   for (;;) {
      pid_t status;

      status = waitpid(pid, &childStatus, 0);
      if (status == pid) {
         /* Success */
         break;
      }

      if (   status == (pid_t)-1
          && errno == EINTR) {
         /* System call interrupted by a signal */
         continue;
      }

      Warning("Unable to wait for the process %"FMTPID" to terminate: "
              "%s.\n\n", pid, strerror(errno));

      return FALSE;
   }

   if ((NULL != validExitCode) && (NULL != exitCode)) {
      *validExitCode = WIFEXITED(childStatus);
      *exitCode = WEXITSTATUS(childStatus);
   }

   retVal = (WIFEXITED(childStatus) && WEXITSTATUS(childStatus) == 0);

   Debug("Done waiting for process: %"FMTPID" (%s)\n", pid,
         retVal ? "success" : "failure");

   return retVal;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_ExecAsync --
 *
 *      Execute a command in the background, returning immediately.
 *
 * Results:
 *      The async proc (must be freed) or
 *      NULL if the cmd failed to be forked.
 *
 * Side effects:
 *	The cmd is run.
 *
 *----------------------------------------------------------------------
 */

ProcMgr_AsyncProc *
ProcMgr_ExecAsync(char const *cmd,                 // IN: UTF-8 command line
                  ProcMgr_ProcArgs *userArgs)      // IN: Unused
{
   ProcMgr_AsyncProc *asyncProc = NULL;
   pid_t pid;
   int fds[2];
   Bool validExitCode;
   int exitCode;
   pid_t resultPid;
   FileIODescriptor readFd;
   FileIODescriptor writeFd;

   Debug("Executing async command: %s\n", cmd);
   
   if (pipe(fds) == -1) {
      Warning("Unable to create the pipe to launch command: %s.\n", cmd);
      return NULL;
   }

   readFd = FileIO_CreateFDPosix(fds[0], O_RDONLY);
   writeFd = FileIO_CreateFDPosix(fds[1], O_WRONLY);

   pid = fork();

   if (pid == -1) {
      Warning("Unable to fork: %s.\n\n", strerror(errno));
      goto abort;
   } else if (pid == 0) {
      struct sigaction olds[ARRAYSIZE(cSignals)];
      int i, maxfd;
      Bool status = TRUE;
      pid_t childPid = -1;

      /*
       * Child
       */

      /*
       * shut down everything but stdio and the pipe() we just made.
       * leaving all the other fds behind can cause nastiness with the X
       * connection and I/O errors, and make wait() hang.
       *
       * should probably call Hostinfo_ResetProcessState(), but that
       * does some stuff with iopl() we don't need
       */
      maxfd = sysconf(_SC_OPEN_MAX);
      for (i = STDERR_FILENO + 1; i < maxfd; i++) {
         if (i != readFd.posix && i != writeFd.posix) {
            close(i);
         }
      }

      if (Signal_SetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals),
#ifndef sun
                                 SIG_DFL
#else
                                 0
#endif
                                 ) == 0) {
         status = FALSE;
      }

      FileIO_Close(&readFd);

      /*
       * Only run the program if we have not already experienced a failure.
       */
      if (status) {
         childPid = ProcMgrStartProcess(cmd);
         status = childPid != -1;
      }

      /*
       * Send the child's pid back immediately, so that the caller can
       * report the result pid back synchronously.
       */
      if (FileIO_Write(&writeFd, &childPid, sizeof childPid, NULL) !=
          FILEIO_SUCCESS) {
         Warning("Waiter unable to write back to parent.\n");
         
         /*
          * This is quite bad, since the original process will block
          * waiting for data. Unfortunately, there isn't much to do
          * (other than trying some other IPC mechanism).
          */
         exit(-1);
      }

      if (status) {
         /*
          * If everything has gone well so far, then wait until the child
          * finishes executing.
          */
         ASSERT(pid != -1);
         status = ProcMgrWaitForProcCompletion(childPid, &validExitCode, &exitCode);
      }
      
      /* 
       * We always have to send IPC back to caller, so that it does not
       * block waiting for data we'll never send.
       */
      Debug("Writing the command %s a success to fd %x\n", 
            status ? "was" : "was not", writeFd.posix);
      if (FileIO_Write(&writeFd, &status, sizeof status, NULL) !=
          FILEIO_SUCCESS) {
         Warning("Waiter unable to write back to parent\n");
         
         /*
          * This is quite bad, since the original process will block
          * waiting for data. Unfortunately, there isn't much to do
          * (other than trying some other IPC mechanism).
          */
         exit(-1);
      }

      if (FileIO_Write(&writeFd, &exitCode, sizeof exitCode , NULL) != 
          FILEIO_SUCCESS) {
         Warning("Waiter unable to write back to parent\n");
         
         /*
          * This is quite bad, since the original process will block
          * waiting for data. Unfortunately, there isn't much to do
          * (other than trying some other IPC mechanism).
          */
         exit(-1);
      }

      FileIO_Close(&writeFd);

      if (status &&
          Signal_ResetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals)) == 0) {
         /*
          * We are too close to give up now.
          */
      }

      if (!validExitCode) {
         exitCode = 0;
      }

      exit(exitCode);
   }

   /*
    * Parent
    */

   FileIO_Close(&writeFd);

   /*
    * Read the pid of the child's child from the pipe.
    */
   if (FileIO_Read(&readFd, &resultPid, sizeof resultPid , NULL) !=
       FILEIO_SUCCESS) {
      Warning("Unable to read result pid from the pipe.\n");
      
      /*
       * We cannot wait on the child process here, since the error
       * may have just been on our end, so the child could be running
       * for some time and we probably cannot afford to block.
       * Just kill the child and move on.
       */
      ProcMgrKill(pid, SIGKILL, -1);
      goto abort;
   }

   if (resultPid == -1) {
      Warning("The child failed to fork the target process.\n");
      
      /*
       * Clean up the child process; it should exit pretty quickly.
       */
      waitpid(pid, NULL, 0);
      goto abort;
   }

   asyncProc = Util_SafeMalloc(sizeof *asyncProc);
   asyncProc->fd = readFd;
   FileIO_Invalidate(&readFd);
   asyncProc->waiterPid = pid;
   asyncProc->validExitCode = FALSE;
   asyncProc->exitCode = -1;
   asyncProc->resultPid = resultPid;

 abort:
   if (FileIO_IsValid(&readFd)) {
      FileIO_Close(&readFd);
   }
   if (FileIO_IsValid(&writeFd)) {
      FileIO_Close(&writeFd);
   }
       
   return asyncProc;
}

/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_IsProcessRunning --
 *
 *      Check to see if a pid is active
 *
 * Results:
 *      TRUE if the process exists; FALSE otherwise
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Bool
ProcMgr_IsProcessRunning(pid_t pid)
{
   return ((kill(pid, 0) == 0) || (errno == EPERM));
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgrKill --
 *
 *      Try to kill a pid & check every so often to see if it has died.
 *
 * Results:
 *      TRUE if the process died; FALSE otherwise
 *
 * Side effects:
 *	Depends on the program being killed.
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgrKill(pid_t pid,      // IN
            int sig,        // IN
            int timeout)    // IN: -1 will wait indefinitely
{
   if (kill(pid, sig) == -1) {
      Warning("Error trying to kill process %"FMTPID" with signal %d: %s\n",
              pid, sig, Msg_ErrString());
   } else {
      int i;

      /* Try every 100ms until we've reached the timeout */
      for (i = 0; timeout == -1 || i < timeout * 10; i++) {
         int ret;

         ret = waitpid(pid, NULL, WNOHANG);

         if (ret == -1) {
            /*
             * if we didn't start it, we can only check if its running
             * by looking in the proc table
             */
            if (ECHILD == errno) {
               if (ProcMgr_IsProcessRunning(pid)) {
                  Debug("Process %"FMTPID" is not a child, still running\n",
                        pid);
                  usleep(100000);
                  continue;
               }
               return TRUE;
            }
            Warning("Error trying to wait on process %"FMTPID": %s\n",
                    pid, Msg_ErrString());
         } else if (ret == 0) {
            usleep(100000);
         } else {
            Debug("Process %"FMTPID" died from signal %d on iteration #%d\n",
                  pid, sig, i);
            return TRUE;
         }
      }
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_KillByPid --
 *
 *      Terminate the process of procId.
 *
 * Results:
 *      Bool.
 *
 * Side effects:
 *	Lots, depending on the program
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_KillByPid(ProcMgr_Pid procId)   // IN
{
   Bool success = TRUE;

   if (!ProcMgrKill(procId, SIGTERM, 5)) {
      success = ProcMgrKill(procId, SIGKILL, -1);
   }

   return success;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_Kill --
 *
 *      Kill a process synchronously by first attempty to do so
 *      nicely & then whipping out the SIGKILL axe.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Depends on the program being killed.
 *
 *----------------------------------------------------------------------
 */

void
ProcMgr_Kill(ProcMgr_AsyncProc *asyncProc) // IN
{
   if ((asyncProc == NULL) || (asyncProc->waiterPid == -1)) {
      ASSERT(FALSE);
      return;
   }

   ProcMgr_KillByPid(asyncProc->waiterPid);
   asyncProc->waiterPid = -1;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_IsAsyncProcRunning --
 *
 *      Checks whether an async process is still running.
 *
 * Results:
 *      TRUE iff the process is still running.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
   
Bool
ProcMgr_IsAsyncProcRunning(ProcMgr_AsyncProc *asyncProc) // IN
{
   int maxFd;
   fd_set readFds;
   struct timeval tv;
   int status;
   Selectable fd;

   ASSERT(asyncProc);
   
   /*
    * Do a select, not a read. This procedure may be called many times,
    * while polling another program. After it returns true, then the
    * watcher program will try to read the socket to get the IPC error
    * and the exit code.
    */
   fd = ProcMgr_GetAsyncProcSelectable(asyncProc);
   FD_ZERO(&readFds);
   FD_SET(fd, &readFds);
   maxFd = fd;

   tv.tv_sec = 0;
   tv.tv_usec = 0;

   status = select(maxFd + 1, &readFds, NULL, NULL, &tv);
   if (status == -1) {
      return(FALSE); // Not running
   } else if (status > 0) {
      return(FALSE); // Not running
   } else {
      return(TRUE); // Still running
   }
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_GetAsyncProcSelectable --
 *
 *      Get the selectable fd for an async proc struct.
 *
 * Results:
 *      The fd casted to a void *.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Selectable
ProcMgr_GetAsyncProcSelectable(ProcMgr_AsyncProc *asyncProc)
{
   ASSERT(asyncProc);
   
   return asyncProc->fd.posix;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_GetPid --
 *
 *      Get the pid for an async proc struct.
 *
 * Results:
 *      
 * Side effects:
 *
 *	None.
 *
 *----------------------------------------------------------------------
 */

ProcMgr_Pid
ProcMgr_GetPid(ProcMgr_AsyncProc *asyncProc)
{
   ASSERT(asyncProc);
   
   return asyncProc->resultPid;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_GetExitCode --
 *
 *      Get the exit code status of an async process. Waits on the child
 *      process so that its resources are cleaned up.
 *
 * Results:
 *      0 if successful, -1 if not.
 *
 * Side effects:
 *	     None.
 *
 *----------------------------------------------------------------------
 */

int
ProcMgr_GetExitCode(ProcMgr_AsyncProc *asyncProc,  // IN
                    int *exitCode)                 // OUT
{
   ASSERT(asyncProc);
   ASSERT(exitCode);

   *exitCode = -1;

   if (asyncProc->waiterPid != -1) {
      Bool status;

      if (FileIO_Read(&(asyncProc->fd), &status, sizeof status, NULL) !=
          FILEIO_SUCCESS) {
         Warning("Error reading async process status.\n");
         goto exit;
      }

      if (FileIO_Read(&(asyncProc->fd), &(asyncProc->exitCode),
                      sizeof asyncProc->exitCode, NULL) !=
          FILEIO_SUCCESS) {
         Warning("Error reading async process status.\n");
         goto exit;
      }

      asyncProc->validExitCode = TRUE;

      Debug("Child w/ fd %x exited with code=%d\n",
            asyncProc->fd.posix, asyncProc->exitCode);
   }

   *exitCode = asyncProc->exitCode;

exit:
   if (asyncProc->waiterPid != -1) {
      Debug("Waiting on pid %"FMTPID" to de-zombify it\n", asyncProc->waiterPid);
      waitpid(asyncProc->waiterPid, NULL, 0);
      asyncProc->waiterPid = -1;
   }
   return (asyncProc->exitCode == -1) ? -1 : 0;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_Free --
 *
 *      Discard the state of an async process. You must call one of
 *      ProcMgr_Kill(), ProcMgr_GetAsyncStatus(), or ProcMgr_GetExitCode()
 *      before calling this function to ensure that the child process
 *      is cleaned up.
 *
 *      That clean-up cannot occur here, since blocking with a waitpid()
 *      is an excessive side effect for a Free() function.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
ProcMgr_Free(ProcMgr_AsyncProc *asyncProc) // IN
{
   /*
    * Make sure that we don't leak zombie processes.
    */
#ifdef VMX86_DEBUG
   if ((asyncProc != NULL) && (asyncProc->waiterPid != -1)) {
      /*
       * Someone did not call ProcMgr_Kill(), ProcMgr_GetAsyncStatus(),
       * or ProcMgr_GetExitCode().
       */
      Warning("Leaving process %"FMTPID" to be a zombie.\n", 
              asyncProc->waiterPid);
   }
#endif 

   if ((asyncProc != NULL) && FileIO_IsValid(&(asyncProc->fd))) {
      FileIO_Close(&(asyncProc->fd));
   }
   
   free(asyncProc);
}

#ifdef linux

/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_ImpersonateUserStart --
 *
 *      Impersonate a user.  Much like bora/lib/impersonate, but
 *      changes the real and saved uid as well, to work with syscalls
 *      (access() and kill()) that look at real UID instead of effective.
 *      The user name should be UTF-8 encoded, although we do not enforce
 *      it right now.
 *
 *      Assumes it will be called as root.
 *
 * Results:
 *      TRUE on success
 *
 * Side effects:
 *
 *      Uid/gid set to given user, saved uid/gid left as root.
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_ImpersonateUserStart(const char *user,  // IN: UTF-8 encoded user name
                             AuthToken token)   // IN
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   gid_t root_gid;
   int error;
   int ret;

   if ((error = getpwuid_r(0, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      /*
       * getpwuid_r() and getpwnam_r() can return a 0 (success) but not
       * set the return pointer (ppw) if there's no entry for the user,
       * according to POSIX 1003.1-2003, so patch up the errno.
       */
      if (error == 0) {
         error = ENOENT;
      }
      return FALSE;
   }

   root_gid = ppw->pw_gid;

   if ((error = getpwnam_r(user, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      if (error == 0) {
         error = ENOENT;
      }
      return FALSE;
   }

   // first change group
   ret = setresgid(ppw->pw_gid, ppw->pw_gid, root_gid);
   if (ret < 0) {
      Warning("Failed to setresgid() for user %s\n", user);
      return FALSE;
   }
   ret = initgroups(ppw->pw_name, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to initgroups() for user %s\n", user);
      goto failure;
   }
   // now user
   ret = setresuid(ppw->pw_uid, ppw->pw_uid, 0);
   if (ret < 0) {
      Warning("Failed to setresuid() for user %s\n", user);
      goto failure;
   }

   // set env
   setenv("USER", ppw->pw_name, 1);
   setenv("HOME", ppw->pw_dir, 1);
   setenv("SHELL", ppw->pw_shell, 1);

   return TRUE;

failure:
   // try to restore on error
   ProcMgr_ImpersonateUserStop();

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_ImpersonateUserStop --
 *
 *      Stop impersonating a user and return to root.
 *
 * Results:
 *      TRUE on success
 *
 * Side effects:
 *
 *      Uid/gid restored to root.
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_ImpersonateUserStop(void)
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   int error;
   int ret;

   if ((error = getpwuid_r(0, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      if (error == 0) {
         error = ENOENT;
      }
      return FALSE;
   }

   // first change back user
   ret = setresuid(ppw->pw_uid, ppw->pw_uid, 0);
   if (ret < 0) {
      Warning("Failed to setresuid() for root\n");
      return FALSE;
   }

   // now group
   ret = setresgid(ppw->pw_gid, ppw->pw_gid, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to setresgid() for root\n");
      return FALSE;
   }
   ret = initgroups(ppw->pw_name, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to initgroups() for root\n");
      return FALSE;
   }

   // set env
   setenv("USER", ppw->pw_name, 1);
   setenv("HOME", ppw->pw_dir, 1);
   setenv("SHELL", ppw->pw_shell, 1);

   return TRUE;
}

#endif // linux
