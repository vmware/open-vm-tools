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
#endif
#if !defined(sun) && !defined(__APPLE__)
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
#if defined(linux) || defined(__FreeBSD__) || defined(HAVE_SYS_USER_H)
// sys/param.h is required on FreeBSD before sys/user.h
#   include <sys/param.h>
// Pull in PAGE_SIZE/PAGE_SHIFT defines ahead of vm_basic_defs.h
#   include <sys/user.h>
#endif
#if defined (__FreeBSD__)
#include <kvm.h>
#include <limits.h>
#include <paths.h>
#include <sys/sysctl.h>
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
#include "unicode.h"


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

static pid_t ProcMgrStartProcess(char const *cmd,
                                 char * const  *envp,
                                 char const *workingDir);

static Bool ProcMgrWaitForProcCompletion(pid_t pid,
                                         Bool *validExitCode,
                                         int *exitCode);

static Bool ProcMgrKill(pid_t pid,
                        int sig,
                        int timeout);

#ifdef sun
#define  SOLARIS_BASH_PATH "/usr/bin/bash"
#endif

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
 * ProcMgr_ReadProcFile --
 *
 *    Read the contents of a file in /proc/<pid>.
 *
 *    The size is essentially unbounded because of cmdline arguments.
 *    The only way to figure out the content size is to keep reading;
 *    stat(2) and lseek(2) lie.
 *
 *    The contents are NUL terminated -- in some distros may not include
 *    a NUL for some commands (eg. pid 1, /sbin/init) -- so add
 *    one to be safe.
 *
 * Results:
 *
 *    The length of the file.
 *
 *    -1 on error.
 *
 * Side effects:
 *
 *    The returned contents must be freed by caller.
 *
 *----------------------------------------------------------------------
 */

#if !defined(sun) && !defined(__FreeBSD__)
int
ProcMgr_ReadProcFile(int fd,                       // IN
                     char **contents)              // OUT
{
   int size = 0;
#if !defined(__APPLE__)
   char tmp[512];
   int numRead;

   *contents = NULL;
   numRead = read(fd, tmp, sizeof(tmp));
   size = numRead;

   if (numRead <= 0) {
      goto done;
   }

   /*
    * handle the 99% case
    */
   if (sizeof(tmp) > numRead) {
      char *result;

      result = malloc(numRead + 1);
      if (NULL == result) {
         size = -1;
         goto done;
      }
      memcpy(result, tmp, numRead);
      result[numRead] = '\0';
      *contents = result;
      goto done;
   } else {
      DynBuf dbuf;

      DynBuf_Init(&dbuf);
      DynBuf_Append(&dbuf, tmp, numRead);
      do {
         numRead = read(fd, tmp, sizeof(tmp));
         if (numRead > 0) {
            DynBuf_Append(&dbuf, tmp, numRead);
         }
         size += numRead;
      } while (numRead > 0);
      // add the NUL term
      DynBuf_Append(&dbuf, "", 1);
      DynBuf_Trim(&dbuf);
      *contents = DynBuf_Detach(&dbuf);
      DynBuf_Destroy(&dbuf);
   }
done:
#endif
   return size;
}

#endif   // !sun && !FreeBSD

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

#if !defined(sun) && !defined(__FreeBSD__)
ProcMgr_ProcList *
ProcMgr_ListProcesses(void)
{
   ProcMgr_ProcList *procList = NULL;
#if !defined(__APPLE__)
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
      char *cmdLineTemp = NULL;
      char *cmdStatTemp = NULL;
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
       */
      numRead = ProcMgr_ReadProcFile(cmdFd, &cmdLineTemp);
      close(cmdFd);

      if (numRead < 0) {
         continue;
      }

      if (numRead > 0) {
         /*
          * Stop before we hit the final '\0'; want to leave it alone.
          */
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
            numRead = ProcMgr_ReadProcFile(cmdFd, &cmdLineTemp);
            close(cmdFd);
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
            const char *nameStart;
            char *copyItr;

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
       * Get the inode information for this process.  This gives us
       * the process owner.
       */
      if (snprintf(cmdFilePath,
                   sizeof cmdFilePath,
                   "/proc/%s",
                   ent->d_name) == -1) {
         Debug("Giant process id '%s'\n", ent->d_name);
         goto next_entry;
      }

      /*
       * stat() /proc/<pid> to get the owner.  We use fileStat.st_uid
       * later in this code.  If we can't stat(), ignore and continue.
       * Maybe we don't have enough permission.
       */
      statResult = stat(cmdFilePath, &fileStat);
      if (0 != statResult) {
         goto next_entry;
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
         goto next_entry;
      }
      cmdFd = open(cmdFilePath, O_RDONLY);
      if (-1 == cmdFd) {
         goto next_entry;
      }
      numRead = ProcMgr_ReadProcFile(cmdFd, &cmdStatTemp);
      close(cmdFd);
      if (0 >= numRead) {
         goto next_entry;
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
         goto next_entry;
      }
      processStartTime = hostStartTime + (relativeStartTime / hertz);

      /*
       * Store the command line string pointer in dynbuf.
       */
      if (cmdLineTemp) {
         cmdLine = Unicode_Alloc(cmdLineTemp, STRING_ENCODING_DEFAULT);
      } else {
         cmdLine = Unicode_Alloc("", STRING_ENCODING_UTF8);
      }
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
                 : Unicode_Alloc(pwd->pw_name, STRING_ENCODING_DEFAULT);
      DynBuf_Append(&dbProcOwner, &userName, sizeof userName);

      /*
       * Store the time that the process started.
       */
      DynBuf_Append(&dbProcStartTime,
                    &processStartTime,
                    sizeof processStartTime);

next_entry:
      free(cmdLineTemp);
      free(cmdStatTemp);
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
#endif // !defined(__APPLE__)

   return procList;
}
#endif // !defined(sun) & !defined(__FreeBSD__)


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

#if defined(__FreeBSD__)
ProcMgr_ProcList *
ProcMgr_ListProcesses(void)
{
   ProcMgr_ProcList *procList = NULL;
   Bool failed = TRUE;
   static kvm_t *kd;
   struct kinfo_proc *kp;
   char errbuf[_POSIX2_LINE_MAX];
   int i, nentries=-1, flag=0;
   DynBuf dbProcId;
   DynBuf dbProcCmd;
   DynBuf dbProcStartTime;
   DynBuf dbProcOwner;

   DynBuf_Init(&dbProcId);
   DynBuf_Init(&dbProcCmd);
   DynBuf_Init(&dbProcStartTime);
   DynBuf_Init(&dbProcOwner);

   /*
    * Get the handle to the Kernel Virtual Memory
    */
   kd = kvm_openfiles(NULL, _PATH_DEVNULL, NULL, O_RDONLY, errbuf);
   if (kd == NULL) {
      Warning("%s: failed to open kvm with error: %s\n", __FUNCTION__, errbuf);
      goto abort;
   }

   /*
    * Get the list of process info structs
    */
   kp = kvm_getprocs(kd, KERN_PROC_PROC, flag, &nentries);
   if ((kp == NULL && nentries > 0) || (kp != NULL && nentries < 0)) {
      Warning("%s: failed to get proc infos with error: %s\n",
              __FUNCTION__, kvm_geterr(kd));
      goto abort;
   }

   /*
    * Iterate through the list of process entries
    */
   for (i = 0; i < nentries; ++i, ++kp) {
      struct passwd *pwd;
      char *userName = NULL;
      char *cmdLine = NULL;
      char **cmdLineTemp = NULL;
      time_t processStartTime;

      /*
       * Store the pid of the process
       */
      if (!DynBuf_Append(&dbProcId, &(kp->ki_pid), sizeof(kp->ki_pid))) {
         Warning("%s: failed to append pid in DynBuf - out of memory\n",
                 __FUNCTION__);
         goto abort;
      }

      /*
       * Store the owner of the process.
       */
      pwd = getpwuid(kp->ki_uid);
      userName = (NULL == pwd)
                 ? Str_SafeAsprintf(NULL, "%d", (int) kp->ki_uid)
                 : Unicode_Alloc(pwd->pw_name, STRING_ENCODING_DEFAULT);
      if (!DynBuf_Append(&dbProcOwner, &userName, sizeof userName)) {
         Warning("%s: failed to append username in DynBuf - out of memory\n",
                 __FUNCTION__);
         goto abort;
      }

      /*
       * Store the command line string of the process
       */
      cmdLineTemp = kvm_getargv(kd, kp, 0);
      if (cmdLineTemp != NULL) {
         // flatten the argument list to get cmd & all params
         DynBuf dbuf;

         DynBuf_Init(&dbuf);
         while (*cmdLineTemp != NULL) {
            if (!DynBuf_Append(&dbuf, *cmdLineTemp, strlen(*cmdLineTemp))) {
               Warning("%s: failed to append cmd/args in DynBuf - no memory\n",
                       __FUNCTION__);
               goto abort;
            }
            cmdLineTemp++;
            if (*cmdLineTemp != NULL) {
               // add the whitespace between arguments
               if (!DynBuf_Append(&dbuf, " ", 1)) {
                  Warning("%s: failed to append ' ' in DynBuf - no memory\n",
                          __FUNCTION__);
                  goto abort;
               }
            }
         }
         // add the NUL term
         if (!DynBuf_Append(&dbuf, "", 1)) {
            Warning("%s: failed to append NUL in DynBuf - out of memory\n",
                    __FUNCTION__);
            goto abort;
         }
         DynBuf_Trim(&dbuf);
         cmdLine = DynBuf_Detach(&dbuf);
         DynBuf_Destroy(&dbuf);
      } else {
         cmdLine = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
      }
      if (!DynBuf_Append(&dbProcCmd, &cmdLine, sizeof cmdLine)) {
         Warning("%s: failed to append cmdline in DynBuf - out of memory\n",
                 __FUNCTION__);
         goto abort;
      }

      /*
       * Store the start time of the process
       */
      processStartTime = kp->ki_start.tv_sec;
      if (!DynBuf_Append(&dbProcStartTime,
                    &processStartTime,
                    sizeof processStartTime)) {
         Warning("%s: failed to append start time in DynBuf - out of memory\n",
                 __FUNCTION__);
         goto abort;
      }

   } // for nentries

   if (0 == DynBuf_GetSize(&dbProcId)) {
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
   procList = (ProcMgr_ProcList *) Util_SafeCalloc(1, sizeof(ProcMgr_ProcList));
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

   failed = FALSE;
abort:
   DynBuf_Destroy(&dbProcId);
   DynBuf_Destroy(&dbProcCmd);
   DynBuf_Destroy(&dbProcStartTime);
   DynBuf_Destroy(&dbProcOwner);

   if (kd != NULL) {
      /*
       * Wrap up: also deallocates memory (if) allocated by kvm_getargv()
       */
      kvm_close(kd);
   }

   if (failed) {
      ProcMgr_FreeProcList(procList);
      procList = NULL;
   }

   return procList;
}
#endif // defined(__FreeBSD__)

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
                 ProcMgr_ProcArgs *userArgs)       // IN: optional
{
   pid_t pid;

   Debug("Executing sync command: %s\n", cmd);

   pid = ProcMgrStartProcess(cmd, userArgs ? userArgs->envp : NULL,
                             userArgs ? userArgs->workingDirectory : NULL);

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
ProcMgrStartProcess(char const *cmd,            // IN: UTF-8 encoded cmd
                    char * const *envp,         // IN: UTF-8 encoded env vars
                    char const *workingDir)     // IN: UTF-8 working directory
{
   pid_t pid;
   char *cmdCurrent = NULL;
   char **envpCurrent = NULL;
   char *workDir = NULL;

   if (cmd == NULL) {
      ASSERT(FALSE);
      return -1;
   }

   /*
    * Convert the strings before the call to fork(), since the conversion
    * routines may rely on locks that do not survive fork().
    */

   if (!CodeSet_Utf8ToCurrent(cmd, strlen(cmd), &cmdCurrent, NULL)) {
      Warning("Could not convert from UTF-8 to current\n");
      return -1;
   }

   if ((NULL != workingDir) &&
       !CodeSet_Utf8ToCurrent(workingDir, strlen(workingDir), &workDir, NULL)) {
      Warning("Could not convert workingDir from UTF-8 to current\n");
      return -1;
   }

   if (NULL != envp) {
      envpCurrent = Unicode_GetAllocList(envp, -1, STRING_ENCODING_DEFAULT);
   }

   pid = fork();

   if (pid == -1) {
      Warning("Unable to fork: %s.\n\n", strerror(errno));
   } else if (pid == 0) {
#ifdef sun
      /*
       * On Solaris, /bin/sh is the Bourne shell, and it
       * doesn't appear to have the optimization that bash does -- when
       * called with -c, bash appears to just use exec() to replace itself.
       * Bourne shell does a fork & exec, so 2 processes are started.
       * This is bad for us because we then see the PID of the shell, not the
       * app that it starts.  When this PID is returned to a user to
       * watch, they'll watch the wrong process.
       *
       * So for Solaris, use bash instead if possible.  We support
       * Solaris 10 and better; it contains bash, but not in its
       * minimal 'core' package, so it may not exist.
       */
      static const char bashShellPath[] = SOLARIS_BASH_PATH;
      char *bashArgs[] = { "bash", "-c", cmdCurrent, NULL };
      static const char bourneShellPath[] = "/bin/sh";
      char *bourneArgs[] = { "sh", "-c", cmdCurrent, NULL };
      const char *shellPath;
      char **args;
#else
      static const char shellPath[] = "/bin/sh";
      char *args[] = { "sh", "-c", cmdCurrent, NULL };
#endif

#ifdef sun
      if (File_Exists(SOLARIS_BASH_PATH)) {
         shellPath = bashShellPath;
         args = bashArgs;
      } else {
         shellPath = bourneShellPath;
         args = bourneArgs;
      }
#endif

      /*
       * Child
       */

      if (NULL != workDir) {
         if (chdir(workDir) != 0) {
            Warning("%s: Could not chdir(%s) %s\n", __FUNCTION__, workDir,
                    strerror(errno));
         }
      }

      if (NULL != envpCurrent) {
         execve(shellPath, args, envpCurrent);
      } else  {
         execv(shellPath, args);
      }

      /* Failure */
      Panic("Unable to execute the \"%s\" shell command: %s.\n\n",
            cmd, strerror(errno));
   }

   /*
    * Parent
    */

   free(cmdCurrent);
   free(workDir);
   Unicode_FreeList(envpCurrent, -1);
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
                  ProcMgr_ProcArgs *userArgs)      // IN: optional
{
   ProcMgr_AsyncProc *asyncProc = NULL;
   pid_t pid;
   int fds[2];
   Bool validExitCode;
   int exitCode;
   pid_t resultPid;
   FileIODescriptor readFd;
   FileIODescriptor writeFd;

   Debug("Executing async command: '%s' in working dir '%s'\n",
         cmd, (userArgs && userArgs->workingDirectory) ? userArgs->workingDirectory : "");
   
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
         childPid = ProcMgrStartProcess(cmd,
                                        userArgs ? userArgs->envp : NULL,
                                        userArgs ? userArgs->workingDirectory : NULL);
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

#if defined(linux) || defined(__FreeBSD__)

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

/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_GetImpersonatedUserInfo --
 *
 *      Return info about the impersonated user.
 *
 * Results:
 *      TRUE on success
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_GetImpersonatedUserInfo(char **userName,            // OUT
                                char **homeDir)             // OUT
{
   uid_t uid = geteuid();
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   int error;

   *userName = NULL;
   *homeDir = NULL;
   if ((error = getpwuid_r(uid, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
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

   *userName = Unicode_Alloc(ppw->pw_name, STRING_ENCODING_DEFAULT);
   *homeDir = Unicode_Alloc(ppw->pw_dir, STRING_ENCODING_DEFAULT);

   return TRUE;
}

#endif // linux || __FreeBSD__
