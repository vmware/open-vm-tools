/*********************************************************
 * Copyright (C) 2006-2019 VMware, Inc. All rights reserved.
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
 * fileLockPosix.c --
 *
 *      Interface to host-specific file locking functions for POSIX hosts.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#include "vmware.h"
#include "posix.h"
#include "file.h"
#include "fileLock.h"
#include "fileInt.h"
#include "util.h"
#include "str.h"
#include "err.h"
#include "hostinfo.h"

#include "unicodeOperations.h"

#define LOGLEVEL_MODULE main
#include "loglevel_user.h"

#define LOG_MAX_PROC_NAME  64


/*
 * XXX
 * Most of these warnings must be turned back into Msg_Appends, or the
 * equivalent. They were changed from Msg_Appends to Warnings to facilitate
 * integration in the disklib library, but many of the warnings are very
 * important, and should be presented directly to the user, not go silently
 * into the log file.
 */

/*
 *----------------------------------------------------------------------
 *
 * FileLockIsValidProcess --
 *
 *      Determine if the process, via its pid, is valid (alive).
 *
 * Results:
 *      TRUE    Yes
 *      FALSE   No
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
FileLockIsValidProcess(int pid)  // IN:
{
   HostinfoProcessQuery value = Hostinfo_QueryProcessExistence(pid);

   if (value == HOSTINFO_PROCESS_QUERY_UNKNOWN) {
      return TRUE;  // Err on the side of caution
   }

   return (value == HOSTINFO_PROCESS_QUERY_ALIVE) ? TRUE : FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLockAppendMessage --
 *
 *      Append a detailed error message to the MsgList.
 *
 * Results:
 *      As above
 *
 * Side effects:
 *      Memory is allocated
 *
 *----------------------------------------------------------------------
 */

void
FileLockAppendMessage(MsgList **msgs,  // IN/OUT/OPT:
                      int err)         // IN: errno
{
#if defined(VMX86_TOOLS)
   Log(LGPFX "A file locking error (%d) has occurred: %s.",
       err, Err_Errno2String(err));
#else
   MsgList_Append(msgs, MSGID(fileLock.posix)
                  "A file locking error (%d) has occurred: %s.",
                  err, Err_Errno2String(err));
#endif
}


#if defined(__linux__)
/*
 *----------------------------------------------------------------------
 *
 * FileReadSlashProc --
 *
 *      Read the data in a /proc file
 *
 * Results:
 *      0    Data is available
 *      !0   Error (errno)
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
FileReadSlashProc(const char *procPath,  // IN:
                  char *buffer,          // OUT:
                  size_t bufferSize)     // IN:
{
   int fd;
   int err;
   char *p;
   size_t len;

   ASSERT(procPath != NULL);
   ASSERT(buffer != NULL);
   ASSERT(bufferSize > 0);

   fd = Posix_Open(procPath, O_RDONLY, 0);

   if (fd == -1) {
      return errno;
   }

   len = read(fd, buffer, bufferSize - 1);
   err = errno;

   close(fd);

   if (len == -1) {
      return err;
   }

   buffer[len] = '\0';

   p = strchr(buffer, '\n');

   if (p != NULL) {
      *p = '\0';
   }

   return 0;
}


/*
 *---------------------------------------------------------------------------
 *
 * FileLockProcessDescriptor --
 *
 *      Returns the process descriptor of the specified process.
 *
 *      The format of a process descriptor is as follows:
 *
 *      processID-processCreationTime(processName)
 *
 *      where the processName and processCreationTime information
 *      may be independently optional.
 *
 * Results:
 *      NULL The process does not exist.
 *     !NULL The process descriptor is returned. It is the callers
 *           responsibility to free the dynamically allocated memory.
 *
 * Side Effects:
 *     None
 *
 *---------------------------------------------------------------------------
 */

static char *
FileLockProcessDescriptor(pid_t pid)  // IN:
{
   char path[64];
   char buffer[1024];
   char *descriptor = NULL;

   if (!FileLockIsValidProcess(pid)) {
      return NULL;
   }

   Str_Sprintf(path, sizeof path, "/proc/%d/stat", pid);

   if (FileReadSlashProc(path, buffer, sizeof buffer) == 0) {
      char *p;
      char *q;
      char *rest;
      uint32 argc;
      char *argv[22];
      char *savePtr = NULL;

      /*
       * You are in a maze of twisty little fields, (virtually) all alike...
       *
       * The process creation time, in 64-bit jiffies is "out there".
       *
       * A "man 5 proc" will provide illumination concerning all of the
       * fields found on this line of text. We code for the worst case
       * and ensure that file names containing spaces or parens are
       * properly handled.
       */

      p = strchr(buffer, '(');

      if ((p == NULL) || (p == buffer) || (*(p - 1) != ' ')) {
         goto bail;
      }

      *(p - 1) = '\0';

      q = strrchr(p + 1, ')');
      if (q == NULL) {
         goto bail;
      }

      rest = q + 1;
      if (*rest != ' ') {
         goto bail;
      }

      *rest++ = '\0';

      argv[0] = strtok_r(buffer, " ", &savePtr);  // ensure no trailing spaces
      argv[1] = p;

      /* Map spaces in the process name to something benign */
      q = p;

      while ((q = strchr(q, ' ')) != NULL) {
         *q = '_';
      }

      if (strlen(p) > LOG_MAX_PROC_NAME) {
         p[LOG_MAX_PROC_NAME - 1] = ')';
         p[LOG_MAX_PROC_NAME] = '\0';
      }

      for (argc = 2; argc < 22; argc++) {
         argv[argc] = strtok_r((argc == 2) ? rest : NULL, " ", &savePtr);

         if (argv[argc] == NULL) {
            break;
         }
      }

      if (argc == 22) {
         descriptor = Str_SafeAsprintf(NULL, "%s-%s%s", argv[0], argv[21],
                                       argv[1]);
      }
   }

bail:

   if (descriptor == NULL) {
      /*
       * Accessing /proc failed in some way. Emit a valid string that also
       * provides a clue that there is/was a problem.
       */

      descriptor = Str_SafeAsprintf(NULL, "%d-0", pid);
   }

   return descriptor;
}
#elif defined(__APPLE__)
/*
 *---------------------------------------------------------------------------
 *
 * FileLockProcessCreationTime --
 *
 *      Returns the process creation time of the specified process.
 *
 * Results:
 *      TRUE  Done!
 *      FALSE Process doesn't exist
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

static Bool
FileLockProcessCreationTime(pid_t pid,                 // IN:
                            uint64 *procCreationTime)  // OUT:
{
   int err;
   int mib[4];
   size_t size;
   struct kinfo_proc info;

   ASSERT(procCreationTime != NULL);

   /* Request information about the specified process */
   mib[0] = CTL_KERN;
   mib[1] = KERN_PROC;
   mib[2] = KERN_PROC_PID;
   mib[3] = pid;

   memset(&info, 0, sizeof info);
   size = sizeof info;
   err = sysctl(mib, ARRAYSIZE(mib), &info, &size, NULL, 0);

   if (err == -1) {
      return FALSE;
   }

   *procCreationTime = (info.kp_proc.p_starttime.tv_sec * CONST64U(1000000)) +
                        info.kp_proc.p_starttime.tv_usec;

   return TRUE;
}


/*
 *---------------------------------------------------------------------------
 *
 * FileLockProcessDescriptor --
 *
 *      Returns the process descriptor of the specified process.
 *
 *      The format of a process descriptor is as follows:
 *
 *      processID-processCreationTime(processName)
 *
 *      where the processName and processCreationTime information
 *      may be independently optional.
 *
 * Results:
 *      NULL The process does not exist.
 *     !NULL The process descriptor is returned. It is the callers
 *           responsibility to free the dynamically allocated memory.
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

static char *
FileLockProcessDescriptor(pid_t pid)  // IN:
{
   uint64 procCreationTime;

   if (!FileLockIsValidProcess(pid)) {
      return NULL;
   }

   if (!FileLockProcessCreationTime(pid, &procCreationTime)) {
      return NULL;
   }

   return Str_SafeAsprintf(NULL, "%d-%"FMT64"u", pid, procCreationTime);
}
#else
/*
 *---------------------------------------------------------------------------
 *
 * FileLockProcessDescriptor --
 *
 *      Returns the process descriptor of the specified process.
 *
 *      The format of a process descriptor is as follows:
 *
 *      processID-processCreationTime(processName)
 *
 *      where the processName and processCreationTime information
 *      may be independently optional.
 *
 * Results:
 *      NULL The process does not exist.
 *     !NULL The process descriptor is returned. It is the callers
 *           responsibility to free the dynamically allocated memory.
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

static char *
FileLockProcessDescriptor(pid_t pid)  // IN:
{
   char *value;

   if (FileLockIsValidProcess(pid)) {
      value = Str_SafeAsprintf(NULL, "%u-0", (uint32) pid);
   } else {
      value = NULL;
   }

   return value;
}
#endif


/*
 *---------------------------------------------------------------------------
 *
 * FileLockGetExecutionID --
 *
 *      Returns the executionID of the caller.
 *
 * Results:
 *      The executionID of the caller. This is a dynamically allocated string;
 *      the caller is responsible for its disposal.
 *
 * Side effects:
 *      The executionID of the caller is not thread safe. Locking is currently
 *      done at the process level - all threads of a process are treated
 *      identically.
 *
 *---------------------------------------------------------------------------
 */

char *
FileLockGetExecutionID(void)
{
   char *descriptor = FileLockProcessDescriptor(getpid());

   ASSERT(descriptor != NULL);  // Must be able to describe ourselves!

   return descriptor;
}


/*
 *---------------------------------------------------------------------------
 *
 * FileLockParseProcessDescriptor --
 *
 *      Attempt to parse the specified process descriptor. Return the
 *      pieces requested.
 *
 * Results:
 *      TRUE  Process descriptor is valid.
 *      FALSE Process descriptor is invalid.
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

static Bool
FileLockParseProcessDescriptor(const char *procDescriptor,  // IN:
                               pid_t *pid,                  // OUT:
                               uint64 *procCreationTime)    // OUT:
{
   uint32 tmp;

   ASSERT(procDescriptor != NULL);
   ASSERT(pid != NULL);
   ASSERT(procCreationTime != NULL);

   if (sscanf(procDescriptor, "%u-%"FMT64"u", &tmp, procCreationTime) != 2) {
      if (sscanf(procDescriptor, "%d", &tmp) == 1) {
         *procCreationTime = 0ULL;
      } else {
         return FALSE;
      }
   }

   *pid = tmp;

   return *pid >= 0;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLockValidExecutionID --
 *
 *      Validate the execution ID.
 *
 * Results:
 *      TRUE    Yes
 *      FALSE   No
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
FileLockValidExecutionID(const char *executionID)  // IN:
{
   pid_t filePID;
   pid_t procPID;
   Bool gotFileData;
   Bool gotProcData;
   char *procDescriptor;
   uint64 fileCreationTime;
   uint64 procCreationTime;

   gotFileData = FileLockParseProcessDescriptor(executionID, &filePID,
                                                &fileCreationTime);

   if (!gotFileData) {
      Warning(LGPFX" %s parse error on '%s'. Assuming valid.\n",
              __FUNCTION__, executionID);

      return TRUE;  // Assume TRUE - preserve a lock - on parse error
   }

   procDescriptor = FileLockProcessDescriptor(filePID);

   if (procDescriptor == NULL) {
      return FALSE;  // process doesn't exist
   }

   gotProcData = FileLockParseProcessDescriptor(procDescriptor, &procPID,
                                                &procCreationTime);

   ASSERT(gotProcData);         // We built it; it had better be good
   ASSERT(procPID == filePID);  // This better match what we started with...

   Posix_Free(procDescriptor);

   if ((fileCreationTime != 0) &&
       (procCreationTime != 0) &&
       (fileCreationTime != procCreationTime)) {
      return FALSE;  // The process no longer exists
   } else {
      return TRUE;  // Looks valid...
   }
}


/*
 *---------------------------------------------------------------------------
 *
 * FileLockNormalizePath
 *
 *      Normalize the path of the file being locked. Locking a symbolic
 *      link should place the lock next to the link, not where the link
 *      points to.
 *
 * Results:
 *      The normalized path or NULL on error
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

static char *
FileLockNormalizePath(const char *filePath)  // IN:
{
   char *result;

   char *dirName = NULL;
   char *fileName = NULL;

   /*
    * If the file to be locked is a symbolic link the lock file belongs next
    * to the symbolic link, not "off" where the symbolic link points to.
    * Translation: Don't "full path" the entire path of the file to be locked;
    * "full path" the dirName of the path, leaving the fileName alone.
    */

   File_GetPathName(filePath, &dirName, &fileName);

   /*
    * Handle filePath - "xxx", "./xxx", "/xxx", and "/a/b/c".
    */

   if (*dirName == '\0') {
      if (File_IsFullPath(filePath)) {
         result = Unicode_Join(DIRSEPS, fileName, NULL);
      } else {
         result = Unicode_Join(".", DIRSEPS, fileName, NULL);
      }
   } else {
      char *fullPath = File_FullPath(dirName);

      result = (fullPath == NULL) ? NULL : Unicode_Join(fullPath, DIRSEPS,
                                                        fileName, NULL);

      Posix_Free(fullPath);
   }

   Posix_Free(dirName);
   Posix_Free(fileName);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLock_Lock --
 *
 *      Obtain a lock on a file; shared or exclusive access. Also specify
 *      how long to wait on lock acquisition - maxWaitTimeMsec
 *
 *      maxWaitTimeMsec specifies the maximum amount of time, in
 *      milliseconds, to wait for the lock before returning the "not
 *      acquired" status. A value of FILELOCK_TRYLOCK_WAIT is the
 *      equivalent of a "try lock" - the lock will be acquired only if
 *      there is no contention. A value of FILELOCK_INFINITE_WAIT
 *      specifies "waiting forever" to acquire the lock.
 *
 * Results:
 *      NULL    Lock not acquired
 *              errno to *err, msg to **msgs - when appropriate
 *      !NULL   Lock Acquired. This is the "lockToken" for an unlock.
 *
 * Side effects:
 *      Changes the host file system.
 *
 *----------------------------------------------------------------------
 */

FileLockToken *
FileLock_Lock(const char *filePath,          // IN:
              const Bool readOnly,           // IN:
              const uint32 maxWaitTimeMsec,  // IN:
              int *err,                      // OUT/OPT: returns errno
              MsgList **msgs)                // IN/OUT/OPT: add error message
{
   int res = 0;
   char *normalizedPath;
   FileLockToken *tokenPtr;

   ASSERT(filePath != NULL);

   normalizedPath = FileLockNormalizePath(filePath);

   if (normalizedPath == NULL) {
      res = EINVAL;

      tokenPtr = NULL;
   } else {
      tokenPtr = FileLockIntrinsic(normalizedPath, !readOnly, maxWaitTimeMsec,
                                   &res);

      Posix_Free(normalizedPath);
   }

   if (tokenPtr == NULL) {
      if (res == 0) {
         res = EAGAIN;  // Thank you for playing; try again
         /* Failed to acquire the lock; another has possession of it */
      }

      FileLockAppendMessage(msgs, res);
   }

   if (err != NULL) {
      *err = res;
   }

   return tokenPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLock_IsLocked --
 *
 *      Is a file currently locked (at the time of the call)?
 *
 * Results:
 *      TRUE   YES
 *      FALSE  Failure (errno to *err, msg to **msgs - when appropriate)
 *
 *----------------------------------------------------------------------
 */

Bool
FileLock_IsLocked(const char *filePath,  // IN:
                  int *err,              // OUT/OPT: returns errno
                  MsgList **msgs)        // IN/OUT/OPT: add error message
{
   int res = 0;
   Bool isLocked;
   char *normalizedPath;

   ASSERT(filePath != NULL);

   normalizedPath = FileLockNormalizePath(filePath);

   if (normalizedPath == NULL) {
      res = EINVAL;

      isLocked = FALSE;
   } else {
      isLocked = FileLockIsLocked(normalizedPath, &res);

      Posix_Free(normalizedPath);
   }

   if (err != NULL) {
      *err = res;
   }

   if (res != 0) {
      FileLockAppendMessage(msgs, res);
   }

   return isLocked;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLock_Unlock --
 *
 *      Release the lock held on the specified file.
 *
 * Results:
 *      TRUE   Success
 *      FALSE  Failure (errno to *err, msg to **msgs - when appropriate)
 *
 * Side effects:
 *      Changes the host file system.
 *
 *----------------------------------------------------------------------
 */

Bool
FileLock_Unlock(const FileLockToken *lockToken,  // IN:
                int *err,                        // OUT/OPT: returns errno
                MsgList **msgs)                  // IN/OUT/OPT: error messages
{
   int res;

   ASSERT(lockToken != NULL);

   res = FileUnlockIntrinsic((FileLockToken *) lockToken);

   if (err != NULL) {
      *err = res;
   }

   if (res != 0) {
      FileLockAppendMessage(msgs, res);
   }

   return (res == 0);
}
