/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
#include <stdio.h>
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
#if defined(__linux__) || defined(__FreeBSD__) || defined(HAVE_SYS_USER_H)
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
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/user.h>
#include "posix.h"
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
#include "dynarray.h"
#include "su.h"
#include "str.h"
#include "strutil.h"
#include "codeset.h"
#include "unicode.h"
#include "logToHost.h"

#ifdef USERWORLD
#include <vm_basic_types.h>
#include <vmkuserstatus.h>
#include <vmkusercompat.h>
#endif


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
   int fd;                   // fd to write to when the child is done
   Bool validExitCode;
   int exitCode;
};

static pid_t ProcMgrStartProcess(char const *cmd,
                                 char * const  *envp,
                                 char const *workingDir);

static Bool ProcMgrWaitForProcCompletion(pid_t pid,
                                         Bool *validExitCode,
                                         int *exitCode);

static int ProcMgrKill(pid_t pid,
                       int sig,
                       int timeout);

#if defined(__APPLE__)
static int ProcMgrGetCommandLineArgs(long pid,
                                     DynBuf *argsBuf,
                                     char **procCmdName);

Bool ProcMgr_PromoteEffectiveToReal(void);
#endif

#ifdef sun
#define  BASH_PATH "/usr/bin/bash"
#else
#define  BASH_PATH "/bin/bash"
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

#if defined(__linux__)
int
ProcMgr_ReadProcFile(int fd,                       // IN
                     char **contents)              // OUT
{
   int size = 0;
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
   return size;
}


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
 *      A ProcMgrProcInfoArray.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

ProcMgrProcInfoArray *
ProcMgr_ListProcesses(void)
{
   ProcMgrProcInfoArray *procList = NULL;
   ProcMgrProcInfo procInfo;
   Bool failed = TRUE;
   DIR *dir;
   struct dirent *ent;
   static time_t hostStartTime = 0;
   static unsigned long long hertz = 100;
   int numberFound;

   procList = Util_SafeCalloc(1, sizeof *procList);
   ProcMgrProcInfoArray_Init(procList, 0);
   procInfo.procCmdName = NULL;
   procInfo.procCmdAbsPath = NULL;
   procInfo.procCmdLine = NULL;
   procInfo.procOwner = NULL;

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
         char *savedLocale;

         /*
          * Set the locale such that floats are delimited with ".".
          */
         realLocale = setlocale(LC_NUMERIC, NULL);
         /*
          * On Linux, the returned locale can point to static data,
          * so make a copy.
          */
         savedLocale = Util_SafeStrdup(realLocale);
         setlocale(LC_NUMERIC, "C");
         numberFound = fscanf(uptimeFile, "%lf", &secondsSinceBoot);
         setlocale(LC_NUMERIC, savedLocale);
         free(savedLocale);

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
      goto abort;
   }

   while ((ent = readdir(dir))) {
      struct stat fileStat;
      char cmdFilePath[1024];
      int statResult;
      int numRead = 0;   /* number of bytes that read() actually read */
      int cmdFd;
      int replaceLoop;
      struct passwd *pwd;
      char *cmdLineTemp = NULL;
      char *cmdStatTemp = NULL;
      size_t strLen = 0;
      unsigned long long dummy;
      unsigned long long relativeStartTime;
      char *stringBegin;
      Bool cmdNameLookup = TRUE;

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

      if (snprintf(cmdFilePath,
                   sizeof cmdFilePath,
                   "/proc/%s/exe",
                   ent->d_name) != -1) {
         int exeLen;
         char exeRealPath[1024];

         exeLen = readlink(cmdFilePath, exeRealPath, sizeof exeRealPath -1);
         if (exeLen != -1) {
            exeRealPath[exeLen] = '\0';
            procInfo.procCmdAbsPath =
               Unicode_Alloc(exeRealPath, STRING_ENCODING_DEFAULT);
         }
      }

      if (numRead > 0) {
         for (replaceLoop = 0 ; replaceLoop < numRead ; replaceLoop++) {
            if ('\0' == cmdLineTemp[replaceLoop] ||
                replaceLoop == numRead - 1) {
               if (cmdNameLookup) {
                  /*
                   * Store the command name.
                   * Find the last path separator, to get the cmd name.
                   * If no separator is found, then use the whole name.
                   * This needs to be done only if there is an absolute
                   * path for the binary. Else, the parsing may result
                   * in incorrect results. Following are few examples:
                   *
                   *   sshd: root@pts/1
                   *   gdm-session-worker [pam/gdm-autologin]
                   *
                   */
                  char *cmdNameBegin = strrchr(cmdLineTemp, '/');
                  if (NULL != cmdNameBegin && cmdLineTemp[0] == '/') {
                     /*
                      * Skip over the last separator.
                      */
                     cmdNameBegin++;
                  } else {
                     cmdNameBegin = cmdLineTemp;
                  }
                  procInfo.procCmdName = Unicode_Alloc(cmdNameBegin, STRING_ENCODING_DEFAULT);
                  if (procInfo.procCmdAbsPath == NULL &&
                      cmdLineTemp[0] == '/') {
                     procInfo.procCmdAbsPath =
                        Unicode_Alloc(cmdLineTemp, STRING_ENCODING_DEFAULT);
                  }
                  cmdNameLookup = FALSE;
               }

               /*
                * In /proc/{PID}/cmdline file, the command and the
                * arguments are separated by '\0'. We need to replace
                * only the intermediate '\0' with ' ' and not the trailing
                * NUL characer.
                */
               if (replaceLoop < (numRead - 1)) {
                  cmdLineTemp[replaceLoop] = ' ';
               }
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
            /*
             * Store the command name.
             */
            procInfo.procCmdName = Unicode_Alloc(cmdLineTemp, STRING_ENCODING_DEFAULT);
            if (procInfo.procCmdAbsPath == NULL &&
                cmdLineTemp[0] == '/') {
               procInfo.procCmdAbsPath = Unicode_Alloc(cmdLineTemp, STRING_ENCODING_DEFAULT);
            }
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

      /*
       * Store the command line string pointer in dynbuf.
       */
      if (cmdLineTemp) {
         int i;

         /*
          * Chop off the trailing whitespace characters.
          */
         for (i = strlen(cmdLineTemp) - 1 ;
              i >= 0 && cmdLineTemp[i] == ' ' ;
              i--) {
            cmdLineTemp[i] = '\0';
         }

         procInfo.procCmdLine = Unicode_Alloc(cmdLineTemp, STRING_ENCODING_DEFAULT);
      } else {
         procInfo.procCmdLine = Unicode_Alloc("", STRING_ENCODING_UTF8);
      }

      /*
       * Store the pid in dynbuf.
       */
      procInfo.procId = (pid_t) atoi(ent->d_name);

      /*
       * Store the owner of the process.
       */
      pwd = getpwuid(fileStat.st_uid);
      procInfo.procOwner = (NULL == pwd)
                           ? Str_SafeAsprintf(&strLen, "%d", (int) fileStat.st_uid)
                           : Unicode_Alloc(pwd->pw_name, STRING_ENCODING_DEFAULT);

      /*
       * Store the time that the process started.
       */
      procInfo.procStartTime = hostStartTime + (relativeStartTime / hertz);

      /*
       * Store the process info pointer into a list buffer.
       */
      if (!ProcMgrProcInfoArray_Push(procList, procInfo)) {
         Warning("%s: failed to expand DynArray - out of memory\n",
                 __FUNCTION__);
         free(cmdLineTemp);
         free(cmdStatTemp);
         goto abort;
      }
      procInfo.procCmdName = NULL;
      procInfo.procCmdAbsPath = NULL;
      procInfo.procCmdLine = NULL;
      procInfo.procOwner = NULL;

next_entry:
      free(procInfo.procCmdName);
      procInfo.procCmdName = NULL;
      free(procInfo.procCmdAbsPath);
      procInfo.procCmdAbsPath = NULL;

      free(cmdLineTemp);
      free(cmdStatTemp);
   } // while readdir

   if (0 < ProcMgrProcInfoArray_Count(procList)) {
      failed = FALSE;
   }

abort:
   closedir(dir);

   free(procInfo.procCmdName);
   free(procInfo.procCmdAbsPath);
   free(procInfo.procCmdLine);
   free(procInfo.procOwner);

   if (failed) {
      ProcMgr_FreeProcList(procList);
      procList = NULL;
   }

   return procList;
}
#endif // defined(__linux__)


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
 *      A ProcMgrProcInfoArray.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

#if defined(__FreeBSD__)
ProcMgrProcInfoArray *
ProcMgr_ListProcesses(void)
{
   ProcMgrProcInfoArray *procList = NULL;
   ProcMgrProcInfo procInfo;
   Bool failed = TRUE;
   static kvm_t *kd;
   struct kinfo_proc *kp;
   char errbuf[_POSIX2_LINE_MAX];
   int i;
   int nentries=-1;
   int flag=0;

   procList = Util_SafeCalloc(1, sizeof *procList);
   procInfo.procCmdName = NULL;
   procInfo.procCmdLine = NULL;
   procInfo.procOwner = NULL;

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
   if (kp == NULL || nentries <= 0) {
      Warning("%s: failed to get proc infos with error: %s\n",
              __FUNCTION__, kvm_geterr(kd));
      goto abort;
   }

   /*
    * Pre-allocate the dynamic array of required size.
    */
   if (!ProcMgrProcInfoArray_Init(procList, nentries)) {
      Warning("%s: failed to create DynArray - out of memory\n",
              __FUNCTION__);
      goto abort;
   }

   /*
    * Iterate through the list of process entries
    */
   for (i = 0; i < nentries; ++i, ++kp) {
      struct passwd *pwd;
      char **cmdLineTemp = NULL;
      char *cmdNameBegin = NULL;
      Bool cmdNameLookup = TRUE;

      /*
       * Store the pid of the process.
       */
      procInfo.procId = kp->ki_pid;

      /*
       * Store the owner of the process.
       */
      pwd = getpwuid(kp->ki_uid);
      procInfo.procOwner = (NULL == pwd)
                           ? Str_SafeAsprintf(NULL, "%d", (int) kp->ki_uid)
                           : Unicode_Alloc(pwd->pw_name, STRING_ENCODING_DEFAULT);

      /*
       * If the command name in the kinfo_proc struct is strictly less than the
       * maximum allowed size, then we can save it right now. Else we shall
       * need to try and parse it from the entire command line.
       */
      if (strlen(kp->ki_comm) + 1 < sizeof kp->ki_comm) {
         procInfo.procCmdName = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
         cmdNameLookup = FALSE;
      }

      /*
       * Store the command line string of the process.
       */
      cmdLineTemp = kvm_getargv(kd, kp, 0);
      if (cmdLineTemp != NULL) {
         /*
          * Flatten the argument list to get cmd & all params.
          */
         DynBuf dbuf;

         DynBuf_Init(&dbuf);
         while (*cmdLineTemp != NULL) {
            if (!DynBuf_Append(&dbuf, *cmdLineTemp, strlen(*cmdLineTemp))) {
               Warning("%s: failed to append cmd/args in DynBuf - no memory\n",
                       __FUNCTION__);
               goto abort;
            }
            if (cmdNameLookup) {
               /*
                * Store the command name of the process.
                * Find the last path separator, to get the cmd name.
                * If no separator is found, then use the whole name.
                */
               cmdNameBegin = strrchr(*cmdLineTemp, '/');
               if (NULL == cmdNameBegin) {
                  cmdNameBegin = *cmdLineTemp;
               } else {
                  /*
                   * Skip over the last separator.
                   */
                  cmdNameBegin++;
               }
               procInfo.procCmdName = Unicode_Alloc(cmdNameBegin, STRING_ENCODING_DEFAULT);
               cmdNameLookup = FALSE;
            }
            cmdLineTemp++;
            if (*cmdLineTemp != NULL) {
               /*
                * Add the whitespace between arguments.
                */
               if (!DynBuf_Append(&dbuf, " ", 1)) {
                  Warning("%s: failed to append ' ' in DynBuf - no memory\n",
                          __FUNCTION__);
                  goto abort;
               }
            }
         }
         /*
          * Add the NUL term.
          */
         if (!DynBuf_Append(&dbuf, "", 1)) {
            Warning("%s: failed to append NUL in DynBuf - out of memory\n",
                    __FUNCTION__);
            goto abort;
         }
         DynBuf_Trim(&dbuf);
         procInfo.procCmdLine = DynBuf_Detach(&dbuf);
         DynBuf_Destroy(&dbuf);
      } else {
         procInfo.procCmdLine = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
         if (cmdNameLookup) {
            procInfo.procCmdName = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
            cmdNameLookup = FALSE;
         }
      }

      /*
       * Store the start time of the process
       */
      procInfo.procStartTime = kp->ki_start.tv_sec;

      /*
       * Store the process info pointer into a list buffer.
       */
      *ProcMgrProcInfoArray_AddressOf(procList, i) = procInfo;
      procInfo.procCmdLine = NULL;
      procInfo.procCmdName = NULL;
      procInfo.procOwner = NULL;

   } // for nentries

   failed = FALSE;

abort:
   if (kd != NULL) {
      kvm_close(kd);
   }

   free(procInfo.procCmdLine);
   free(procInfo.procCmdName);
   free(procInfo.procOwner);

   if (failed) {
      ProcMgr_FreeProcList(procList);
      procList = NULL;
   }

   return procList;
}
#endif // defined(__FreeBSD__)


#if defined(__APPLE__)
/*
 *----------------------------------------------------------------------
 *
 * ProcMgrGetCommandLineArgs --
 *
 *      Fetch all the command line arguments for a given process id.
 *      The argument names shall all be UTF-8 encoded.
 *
 * Results:
 *      Number of arguments retrieved.
 *      Buffer is returned with argument names, if any.
 *      Command name is returned.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static int
ProcMgrGetCommandLineArgs(long pid,               // IN:  process id
                          DynBuf *argsBuf,        // OUT: Buffer with arguments
                          char **procCmdName)     // OUT: Command name string
{
   int argCount = 0;
   int argNum;
   char *argUnicode = NULL;
   char *cmdNameBegin = NULL;
   char *cmdLineRaw = NULL;
   char *cmdLineTemp;
   char *cmdLineEnd;
   size_t maxargs = 0;
   size_t maxargsSize;
   int maxargsName[] = {CTL_KERN, KERN_ARGMAX};
   int argName[] = {CTL_KERN, KERN_PROCARGS2, pid};
   Bool cmdNameLookup = TRUE;
   Bool failed = TRUE;

   if (NULL != procCmdName) {
      *procCmdName = NULL;
   }

   /*
    * Get the sysctl kern argmax.
    */
   maxargsSize = sizeof maxargs;
   if (sysctl(maxargsName, ARRAYSIZE(maxargsName),
              &maxargs, &maxargsSize, NULL, 0) < 0) {
      Warning("%s: failed to get the kernel max args with errno = %d\n",
               __FUNCTION__, errno);
      goto abort;
   }

   /*
    * Fetch the raw command line
    */
   cmdLineRaw = Util_SafeCalloc(maxargs, sizeof *cmdLineRaw);
   if (sysctl(argName, ARRAYSIZE(argName), cmdLineRaw, &maxargs, NULL, 0) < 0) {
      Debug("%s: No command line args for pid = %ld\n", __FUNCTION__, pid);
      goto abort;
   }
   cmdLineEnd = &cmdLineRaw[maxargs];

   /*
    * Format of the raw command line (without line breaks):
    * <argc value><full command path>
    * <one or more '\0' for alignment of first arg>
    * <arg-0 = command as typed><'\0'>
    * <arg-1><'\0'>... <arg-(argc-1))><'\0'>
    * <env-0><'\0'>... <env-n><'\0'>
    * where:
    * arg = command line args we want.
    * env = environment vars we ignore.
    */

   /*
    * Save the number of arguments.
    */
   memcpy(&argNum, cmdLineRaw, sizeof argNum);
   if (0 >= argNum) {
      Debug("%s: Invalid number of command line args (=%d) for pid = %ld\n",
             __FUNCTION__, argNum, pid);
      goto abort;
   }

   /*
    * Skip over the number of args (argc value) and
    * full path to command name in the command line.
    * (Please refer to format in comment above.)
    */
   cmdLineTemp = cmdLineRaw + sizeof argNum;
   cmdLineTemp += strlen(cmdLineTemp) + 1;

   /*
    * Save the arguments one by one
    */
   while (cmdLineTemp < cmdLineEnd && argCount < argNum) {
      /*
       * Skip over leading '\0' chars to reach new arg.
       */
      while (cmdLineTemp < cmdLineEnd && '\0' == *cmdLineTemp) {
         ++cmdLineTemp;
      }
      /*
       * If we are pointing to a valid arg, save it.
       */
      if (cmdLineTemp < cmdLineEnd && '\0' != *cmdLineTemp) {
         /*
          * KERN_PROCARGS2 is not guaranteed to provide argument names in UTF-8.
          * As long as we find UTF-8 argument names, we keep adding to our list.
          * As soon as we see any non UTF-8 argument, we ignore that argument
          * and return the list we have built so far.
          * NOTE: On MacOS, STRING_ENCODING_DEFAULT will default to UTF-8.
          */
         if (Unicode_IsBufferValid(cmdLineTemp, -1, STRING_ENCODING_DEFAULT) &&
             (argUnicode = Unicode_Alloc(cmdLineTemp,
                                          STRING_ENCODING_DEFAULT)) != NULL) {
            /*
             * Add the whitespace between arguments.
             */
            if (0 < argCount) {
               if (!DynBuf_Append(argsBuf, " ", 1)) {
                  Warning("%s: failed to append ' ' in DynBuf\
                           - no memory\n", __FUNCTION__);
                  goto abort;
               }
            }
            /*
             * Add the argument.
             */
            if (!DynBuf_Append(argsBuf, argUnicode, strlen(argUnicode))) {
               Warning("%s: failed to append cmd/args in DynBuf\
                        - no memory\n", __FUNCTION__);
               goto abort;
            }
            free(argUnicode);
            argUnicode = NULL;
         } else {
            break;
         }
         ++argCount;
         /*
          * If this is the command name, retrieve it.
          */
         if (NULL != procCmdName && cmdNameLookup) {
            /*
             * Store the command name of the process.
             * Find the last path separator, to get the cmd name.
             * If no separator is found, then use the whole name.
             */
            cmdNameBegin = strrchr(cmdLineTemp, '/');
            if (NULL == cmdNameBegin) {
               cmdNameBegin = cmdLineTemp;
            } else {
               /*
                * Skip over the last separator.
                */
               cmdNameBegin++;
            }
            *procCmdName = Unicode_Alloc(cmdNameBegin, STRING_ENCODING_DEFAULT);
            cmdNameLookup = FALSE;
         }
      }
      /*
       * Skip over the current arg that we just saved.
       */
      while (cmdLineTemp < cmdLineEnd && '\0' != *cmdLineTemp) {
         ++cmdLineTemp;
      }
   }

   /*
    * Add the NULL term.
    */
   if (!DynBuf_Append(argsBuf, "", 1)) {
      Warning("%s: failed to append NUL in DynBuf - out of memory\n",
              __FUNCTION__);
      goto abort;
   }
   DynBuf_Trim(argsBuf);

   failed = FALSE;
abort:
   free(cmdLineRaw);
   free(argUnicode);
   if (failed) {
      if (NULL != procCmdName) {
         free(*procCmdName);
         *procCmdName = NULL;
      }
      argCount = 0;
   }

   return argCount;
}


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
 *      A ProcMgrProcInfoArray.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

ProcMgrProcInfoArray *
ProcMgr_ListProcesses(void)
{
   ProcMgrProcInfoArray *procList = NULL;
   ProcMgrProcInfo procInfo;
   Bool failed = TRUE;
   struct kinfo_proc *kptmp;
   struct kinfo_proc *kp = NULL;
   size_t procsize;
   int i;
   int nentries;
   int procName[] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};

   procList = Util_SafeCalloc(1, sizeof *procList);
   procInfo.procCmdLine = NULL;
   procInfo.procCmdName = NULL;
   procInfo.procOwner = NULL;

   /*
    * Get the number of process info structs in the entire list.
    */
   if (sysctl(procName, ARRAYSIZE(procName), NULL, &procsize, NULL, 0) < 0) {
      Warning("%s: failed to get the size of the process struct\
               list with errno = %d\n", __FUNCTION__, errno);
      goto abort;
   }
   nentries = (int)(procsize / sizeof *kp);

   /*
    * Get the list of process info structs.
    */
   kp = Util_SafeCalloc(nentries, sizeof *kp);
   if (sysctl(procName, ARRAYSIZE(procName), kp, &procsize, NULL, 0) < 0) {
      Warning("%s: failed to get the process struct list (errno = %d)\n",
               __FUNCTION__, errno);
      goto abort;
   }

   /*
    * Recalculate the number of entries as they may have changed.
    */
   if (0 >= (nentries = (int)(procsize / sizeof *kp))) {
      goto abort;
   }

   /*
    * Pre-allocate the dynamic array of required size.
    */
   if (!ProcMgrProcInfoArray_Init(procList, nentries)) {
      Warning("%s: failed to create DynArray - out of memory\n",
              __FUNCTION__);
      goto abort;
   }

   /*
    * Iterate through the list of process entries
    */
   for (i = 0, kptmp = kp; i < nentries; ++i, ++kptmp) {
      DynBuf argsBuf;
      char buffer[BUFSIZ];
      struct passwd pw;
      struct passwd *ppw = &pw;
      int error;
      int argCount = 0;
      Bool cmdNameLookup = TRUE;

      /*
       * Store the pid of the process
       */
      procInfo.procId = kptmp->kp_proc.p_pid;

      /*
       * Store the owner of the process.
       */
      error = Posix_Getpwuid_r(kptmp->kp_eproc.e_pcred.p_ruid,
                               &pw, buffer, sizeof buffer, &ppw);
      procInfo.procOwner = (0 != error || NULL == ppw)
                           ? Str_SafeAsprintf(NULL, "%d", (int) kptmp->kp_eproc.e_pcred.p_ruid)
                           : Unicode_Alloc(ppw->pw_name, STRING_ENCODING_DEFAULT);

      /*
       * If the command name in the kinfo_proc struct is strictly less than the
       * maximum allowed size, then we can save it right now. Else we shall
       * need to try and parse it from the entire command line.
       */
      if (strlen(kptmp->kp_proc.p_comm) + 1 < sizeof kptmp->kp_proc.p_comm) {
         procInfo.procCmdName = Unicode_Alloc(kptmp->kp_proc.p_comm, STRING_ENCODING_DEFAULT);
         cmdNameLookup = FALSE;
      }

      /*
       * Store the command line arguments of the process.
       * If no arguments are found, use the full command name.
       */
      DynBuf_Init(&argsBuf);
      if (cmdNameLookup) {
         argCount = ProcMgrGetCommandLineArgs(kptmp->kp_proc.p_pid, &argsBuf, &procInfo.procCmdName);
      } else {
         argCount = ProcMgrGetCommandLineArgs(kptmp->kp_proc.p_pid, &argsBuf, NULL);
      }
      if (0 < argCount) {
         procInfo.procCmdLine = DynBuf_Detach(&argsBuf);
         /*
          * cmdName would have been filled up by the function ProcMgrGetCommandLineArgs().
          */
         cmdNameLookup = FALSE;
      } else {
         procInfo.procCmdLine = Unicode_Alloc(kptmp->kp_proc.p_comm, STRING_ENCODING_DEFAULT);
         if (cmdNameLookup) {
            procInfo.procCmdName = Unicode_Alloc(kptmp->kp_proc.p_comm, STRING_ENCODING_DEFAULT);
            cmdNameLookup = FALSE;
         }
      }
      DynBuf_Destroy(&argsBuf);

      /*
       * Store the start time of the process
       */
      procInfo.procStartTime = kptmp->kp_proc.p_starttime.tv_sec;

      /*
       * Store the process info pointer into a list buffer.
       */
      *ProcMgrProcInfoArray_AddressOf(procList, i) = procInfo;

      procInfo.procCmdLine = NULL;
      procInfo.procCmdName = NULL;
      procInfo.procOwner = NULL;

   } // nentries

   failed = FALSE;

abort:
   free(kp);
   free(procInfo.procCmdLine);
   free(procInfo.procCmdName);
   free(procInfo.procOwner);

   if (failed) {
      ProcMgr_FreeProcList(procList);
      procList = NULL;
   }

   return procList;
}
#endif // defined(__APPLE__)

/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_FreeProcList --
 *
 *      Free the memory occupied by ProcMgrProcInfoArray.
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
ProcMgr_FreeProcList(ProcMgrProcInfoArray *procList)
{
   int i;
   size_t procCount;

   if (NULL == procList) {
      return;
   }

   procCount = ProcMgrProcInfoArray_Count(procList);
   for (i = 0; i < procCount; i++) {
      ProcMgrProcInfo *procInfo = ProcMgrProcInfoArray_AddressOf(procList, i);
      free(procInfo->procCmdName);
#if defined(__linux__)
      free(procInfo->procCmdAbsPath);
#endif
      free(procInfo->procCmdLine);
      free(procInfo->procOwner);
   }

   ProcMgrProcInfoArray_Destroy(procList);
   free(procList);
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgrExecSync --
 *
 *      Synchronously execute a command. The command is UTF-8 encoded.
 *
 *      Note, if the caller requests the exitCode for the process to be
 *      run they must also provide the validExitCode to determine
 *      if the exitCode contains a valid value.
 *
 * Results:
 *      - TRUE when program execution completed and succeeded and
 *        *validExitCode contains TRUE if supplied and
 *        *exitCode contains 0 if supplied by the caller
 *
 *      - FALSE when program execution failed to start the process and
 *        *validExitCode contains FALSE if supplied by the caller and
          *exitCode is not valid if also supplied.
 *
 *      - FALSE when program execution completed and failed and
 *        *validExitCode contains TRUE if supplied and
 *        *exitCode contains the exit code of the failed process if supplied
 *
 * Side effects:
 *      Lots, depending on the program.
 *
 *----------------------------------------------------------------------
 */

static Bool
ProcMgrExecSync(char const *cmd,                  // IN: UTF-8 command line
                ProcMgr_ProcArgs *userArgs,       // IN: optional
                Bool *validExitCode,              // OUT: optional exit code is valid
                int *exitCode)                    // OUT: optional exit code
{
   pid_t pid;

   Debug("Executing sync command: %s\n", cmd);

   if (validExitCode != NULL) {
      *validExitCode = FALSE;
   }

   pid = ProcMgrStartProcess(cmd, userArgs ? userArgs->envp : NULL,
                             userArgs ? userArgs->workingDirectory : NULL);

   if (pid == -1) {
      return FALSE;
   }

   return ProcMgrWaitForProcCompletion(pid, validExitCode, exitCode);
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
   return ProcMgrExecSync(cmd, userArgs, NULL, NULL);
}


#if defined(__linux__)
/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_ExecSyncWithExitCode --
 *
 *      Synchronously execute a command. The command is UTF-8 encoded.
 *
 * Results:
 *      - TRUE when program execution completed and succeeded and
 *        *validExitCode contains TRUE and
 *        *exitCode contains 0
 *
 *      - FALSE when either exitCode or validExitCode are not valid
 *
 *      - FALSE when program execution failed to start the process and
 *        *validExitCode contains FALSE by the caller and
          *exitCode is not valid.
 *
 *      - FALSE when program execution completed and failed and
 *        *validExitCode contains TRUE
 *        *exitCode contains the exit code of the failed process
 *
 * Side effects:
 *	Lots, depending on the program.
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_ExecSyncWithExitCode(char const *cmd,                  // IN: UTF-8 command line
                             ProcMgr_ProcArgs *userArgs,       // IN: optional
                             Bool *validExitCode,              // OUT: exit code is valid
                             int *exitCode)                    // OUT: exit code
{
   Bool result = FALSE;

   ASSERT(exitCode != NULL && validExitCode != NULL);

   result = ProcMgrExecSync(cmd, userArgs, validExitCode, exitCode);

   Debug("Executed sync command: %s -> %d (%d)\n",
         cmd, result, *validExitCode ? *exitCode : 0);
   return result;
}
#endif


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

#ifdef USERWORLD
   do {
      static const char filePath[] = "/bin/sh";
      char * const argv[] = { "sh", "++group=host/vim/tmp",
                              "-c", cmdCurrent, NULL };
      int initFds[] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };
      int workingDirFd;
      VmkuserStatus_Code status;
      int outPid;

      workingDirFd = open(workingDir != NULL ? workingDir : "/tmp", O_RDONLY);
      status = VmkuserCompat_ForkExec(filePath,
                                      argv,
                                      envpCurrent,
                                      workingDirFd,
                                      initFds,
                                      ARRAYSIZE(initFds),
                                      geteuid(),
                                      getegid(),
                                      0,
                                      &outPid);
      if (workingDirFd >= 0) {
         close(workingDirFd);
      }
      if (VmkuserStatus_IsOK(status)) {
         pid = (pid_t)outPid;
      } else {
         VmkuserStatus_CodeToErrno(status, &errno);
         pid = -1;
      }
   } while (FALSE);
#else
   pid = fork();

   if (pid == -1) {
      Warning("Unable to fork: %s.\n\n", strerror(errno));
   } else if (pid == 0) {
      static const char bashShellPath[] = BASH_PATH;
      char *bashArgs[] = { "bash", "-c", cmdCurrent, NULL };
      static const char bourneShellPath[] = "/bin/sh";
      char *bourneArgs[] = { "sh", "-c", cmdCurrent, NULL };
      const char *shellPath;
      char **args;

      /*
       * Check bug 772203. To start the program, we start the shell
       * and specify the program using the option '-c'. We should return the
       * PID of the app that gets started.
       *
       * When the option '-c' is specified,
       * - bash shell just uses exec() to replace itself. So, 'bash' returns
       * the PID of the new application that is started.
       *
       * - bourne shell does a fork & exec. So two processes are started. We
       * see the PID of the shell and not the app that it starts. When the PID
       * is returned to a user to watch, they'll watch the wrong process.
       *
       * In order to return the proper PID, use bash if possible. If bash
       * is not available, then use the bourne shell.
       */
      if (File_Exists(bashShellPath)) {
         shellPath = bashShellPath;
         args = bashArgs;
      } else {
         shellPath = bourneShellPath;
         args = bourneArgs;
      }

      /*
       * Child
       */

#ifdef __APPLE__
      /*
       * On OS X with security fixes, we cannot revert the real uid if
       * its changed, so only the effective uid is changed.  But for
       * running programs we need both.  See comments for
       * ProcMgr_ImpersonateUserStart() for details.
       *
       * If it fails, bail since its a security issue if real uid is still
       * root.
       */
      if (!ProcMgr_PromoteEffectiveToReal()) {
         Panic("%s: Could not set real uid to effective\n", __FUNCTION__);
      }
#endif

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
#endif

   /*
    * Parent
    */

   free(cmdCurrent);
   free(workDir);
   Util_FreeStringList(envpCurrent, -1);
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
 *      Prevents zombification of the process.
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
   pid_t resultPid;
   int readFd, writeFd;

   Debug("Executing async command: '%s' in working dir '%s'\n",
         cmd, (userArgs && userArgs->workingDirectory) ? userArgs->workingDirectory : "");

   if (pipe(fds) == -1) {
      Warning("Unable to create the pipe to launch command: %s.\n", cmd);
      return NULL;
   }

   readFd = fds[0];
   writeFd = fds[1];

   pid = fork();

   if (pid == -1) {
      Warning("Unable to fork: %s.\n\n", strerror(errno));
      goto abort;
   } else if (pid == 0) {
      struct sigaction olds[ARRAYSIZE(cSignals)];
      int i, maxfd;
      Bool status = TRUE;
      pid_t childPid = -1;
      Bool validExitCode = FALSE;
      int exitCode = -1;

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
         if (i != readFd && i != writeFd) {
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

      close(readFd);

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
      if (write(writeFd, &childPid, sizeof childPid) == -1) {
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
            status ? "was" : "was not", writeFd);
      if (write(writeFd, &status, sizeof status) == -1) {
         Warning("Waiter unable to write back to parent\n");

         /*
          * This is quite bad, since the original process will block
          * waiting for data. Unfortunately, there isn't much to do
          * (other than trying some other IPC mechanism).
          */
         exit(-1);
      }

      if (write(writeFd, &exitCode, sizeof exitCode) == -1) {
         Warning("Waiter unable to write back to parent\n");

         /*
          * This is quite bad, since the original process will block
          * waiting for data. Unfortunately, there isn't much to do
          * (other than trying some other IPC mechanism).
          */
         exit(-1);
      }

      close(writeFd);

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

   close(writeFd);
   writeFd = -1;

   /*
    * Read the pid of the child's child from the pipe.
    */
   if (read(readFd, &resultPid, sizeof resultPid) != sizeof resultPid) {
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
   readFd = -1;
   asyncProc->waiterPid = pid;
   asyncProc->validExitCode = FALSE;
   asyncProc->exitCode = -1;
   asyncProc->resultPid = resultPid;

 abort:
   if (readFd != -1) {
      close(readFd);
   }
   if (writeFd != -1) {
      close(writeFd);
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
 *      1 if the process died; 0 on failure, -1 on timeout
 *
 * Side effects:
 *	Depends on the program being killed.
 *	errno set.
 *
 *----------------------------------------------------------------------
 */

static int
ProcMgrKill(pid_t pid,      // IN
            int sig,        // IN
            int timeout)    // IN: -1 will wait indefinitely
{
   if (kill(pid, sig) == -1) {
      int savedErrno = errno;
      Warning("Error trying to kill process %"FMTPID" with signal %d: %s\n",
              pid, sig, Msg_ErrString());
      errno = savedErrno;
      return 0;
   } else {
      int i;

      /* Try every 100ms until we've reached the timeout */
      for (i = 0; timeout == -1 || i < timeout * 10; i++) {
         int ret;

         ret = waitpid(pid, NULL, WNOHANG);

         if (ret == -1) {
            /*
             * if we didn't start it, we can only check if its running
             * by looking in the proc table.
             *
             * Note that this is susceptible to a race.  Its possible
             * that just as we kill the process, a new one can come
             * around and re-use the pid by the time we check on it.
             * This requires the pids have wrapped and a lot of luck.
             */
            if (ECHILD == errno) {
               if (ProcMgr_IsProcessRunning(pid)) {
                  Debug("Process %"FMTPID" is not a child, still running\n",
                        pid);
                  usleep(100000);
                  continue;
               }
               return 1;
            }
            Warning("Error trying to wait on process %"FMTPID": %s\n",
                    pid, Msg_ErrString());
         } else if (ret == 0) {
            usleep(100000);
         } else {
            Debug("Process %"FMTPID" died from signal %d on iteration #%d\n",
                  pid, sig, i);
            return 1;
         }
      }
   }

   /*
    * timed out -- system/process is incredibly unresponsive or unkillable
    */
   Warning("%s: timed out trying to kill pid %"FMTPID" with signal %d\n",
           __FUNCTION__, pid, sig);
   return -1;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_KillByPid --
 *
 *      Attempt to terminate the process of procId.
 *      First try TERM for 5 seconds, then KILL for 15
 *      if that is unsuccessful.
 *
 * Results:
 *      Bool.
 *
 * Side effects:
 *	Lots, depending on the program
 *	errno is set.
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_KillByPid(ProcMgr_Pid procId)   // IN
{
   int ret;

   ret = ProcMgrKill(procId, SIGTERM, 5);
   if (ret != 1) {
      /*
       * We can't try forever, since some processes are unkillable (eg systemd),
       * or a process could be stuck 'forever' in a disk wait.
       * 5+15 seconds should be long enough to handle very slow systems, while
       * not causing timeouts at the VMX layer or the guestInfo gathering.
       */
      ret = ProcMgrKill(procId, SIGKILL, 15);
      if (ret == -1) {
         /*
          * Timed out, so fake an errno.  Deadlock is what would happen
          * to ProcMgrKill() if it tried indefinitely.
          */
         errno = EDEADLK;
      }
   }

   return (ret == 1);
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

   return asyncProc->fd;
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

      if (read(asyncProc->fd, &status, sizeof status) != sizeof status) {
         Warning("Error reading async process status.\n");
         goto exit;
      }

      if (read(asyncProc->fd, &asyncProc->exitCode,
               sizeof asyncProc->exitCode) != sizeof asyncProc->exitCode) {
         Warning("Error reading async process status.\n");
         goto exit;
      }

      asyncProc->validExitCode = TRUE;

      Debug("Child w/ fd %x exited with code=%d\n",
            asyncProc->fd, asyncProc->exitCode);
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

   if (asyncProc != NULL && asyncProc->fd != -1) {
      close(asyncProc->fd);
   }

   free(asyncProc);
}

#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)

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
 *      Note that for OS X, we cannot set real uid.  Until a security
 *      patch for 10.10.3 (https://support.apple.com/en-us/HT204659)
 *      it worked, but since the patch once the real user has been
 *      changed, it cannot be restored.  So for OS X we set just
 *      the effective uid. This requires additional tweaks in
 *      ProcMgr_ExecAsync() to call ProcMgr_PromoteEffectiveToReal(),
 *      and preventing kill(2) from being called since it looks at
 *      the real uid.
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
   char *userLocal;

   if ((error = getpwuid_r(0, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      /*
       * getpwuid_r() and getpwnam_r() can return a 0 (success) but not
       * set the return pointer (ppw) if there's no entry for the user,
       * according to POSIX 1003.1-2003, so patch up the errno.
       */
      Warning("Failed to lookup user with uid: %" FMTUID ". Reason: %s\n", 0,
              error == 0 ? "entry not found" : Err_Errno2String(error));
      return FALSE;
   }

   root_gid = ppw->pw_gid;

   /* convert user name to local character set */
   userLocal = (char *)Unicode_GetAllocBytes(user, Unicode_GetCurrentEncoding());
   if (!userLocal) {
       Warning("Failed to convert user name %s to local character set.\n", user);
       return FALSE;
   }

   if ((error = getpwnam_r(userLocal, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      Warning("Failed to lookup user name %s. Reason: %s\n", userLocal,
              error == 0 ? "entry not found" : Err_Errno2String(error));
      free(userLocal);
      return FALSE;
   }

   free(userLocal);

   // first change group
#if defined(USERWORLD)
   ret = Id_SetREGid(ppw->pw_gid, ppw->pw_gid);
#elif defined(__APPLE__)
   ret = setegid(ppw->pw_gid);
#else
   ret = setresgid(ppw->pw_gid, ppw->pw_gid, root_gid);
#endif
   if (ret < 0) {
      WarningToGuest("Failed to set gid for user %s\n", user);
      WarningToHost("Failed to set gid\n");
      return FALSE;
   }
#ifndef USERWORLD
   ret = initgroups(ppw->pw_name, ppw->pw_gid);
   if (ret < 0) {
      WarningToGuest("Failed to initgroups() for user %s\n", user);
      WarningToHost("Failed to initgroups()\n");
      goto failure;
   }
#endif
   // now user
#if defined(USERWORLD)
   ret = Id_SetREUid(ppw->pw_uid, ppw->pw_uid);
#elif defined(__APPLE__)
   ret = seteuid(ppw->pw_uid);
#else
   ret = setresuid(ppw->pw_uid, ppw->pw_uid, 0);
#endif
   if (ret < 0) {
      WarningToGuest("Failed to set uid for user %s\n", user);
      WarningToHost("Failed to set uid\n");
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
      Warning("Failed to lookup user with uid: %" FMTUID ". Reason: %s\n", 0,
              error == 0 ? "entry not found" : Err_Errno2String(error));
      return FALSE;
   }

   // first change back user
#if defined(USERWORLD)
   ret = Id_SetREUid(ppw->pw_uid, ppw->pw_uid);
#elif defined(__APPLE__)
   ret = seteuid(ppw->pw_uid);
#else
   ret = setresuid(ppw->pw_uid, ppw->pw_uid, 0);
#endif
   if (ret < 0) {
      Warning("Failed to set uid for root\n");
      return FALSE;
   }

   // now group
#if defined(USERWORLD)
   ret = Id_SetREGid(ppw->pw_gid, ppw->pw_gid);
#elif defined(__APPLE__)
   ret = setegid(ppw->pw_gid);
#else
   ret = setresgid(ppw->pw_gid, ppw->pw_gid, ppw->pw_gid);
#endif
   if (ret < 0) {
      Warning("Failed to set gid for root\n");
      return FALSE;
   }
#ifndef USERWORLD
   ret = initgroups(ppw->pw_name, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to initgroups() for root\n");
      return FALSE;
   }
#endif

   // set env
   setenv("USER", ppw->pw_name, 1);
   setenv("HOME", ppw->pw_dir, 1);
   setenv("SHELL", ppw->pw_shell, 1);

   return TRUE;
}


#ifdef __APPLE__
/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_PromoteEffectiveToReal --
 *
 *      Sets the processes real uid and gid to match the effective.
 *      Once done, it cannot be undone.
 *
 *      See the commentary in ProcMgr_ImpersonateUserStart() for
 *      why this is needed.
 *
 * Results:
 *      TRUE on success
 *
 * Side effects:
 *       Real uid is now effective.
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_PromoteEffectiveToReal(void)
{
   int ret;
   uid_t uid = geteuid();
   gid_t gid = getegid();

   ret = setregid(gid, gid);
   if (ret < 0) {
      Warning("Failed to setregid(%d) %d\n", gid, errno);
      return FALSE;
   }
   ret = setreuid(uid, uid);
   if (ret < 0) {
      Warning("Failed to setreuid(%d) %d\n", uid, errno);
      return FALSE;
   }

   return TRUE;
}
#endif   // __APPLE__


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
      Warning("Failed to lookup user with uid: %" FMTUID ". Reason: %s\n", uid,
              error == 0 ? "entry not found" : Err_Errno2String(error));
      return FALSE;
   }

   *userName = Unicode_Alloc(ppw->pw_name, STRING_ENCODING_DEFAULT);
   *homeDir = Unicode_Alloc(ppw->pw_dir, STRING_ENCODING_DEFAULT);

   return TRUE;
}

#endif // linux || __FreeBSD__ || __APPLE__
