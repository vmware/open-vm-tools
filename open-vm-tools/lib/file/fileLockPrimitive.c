/*********************************************************
 * Copyright (C) 2007-2021 VMware, Inc. All rights reserved.
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
 * fileLockPrimitive.c --
 *
 *      Portable file locking via Lamport's Bakery algorithm.
 *
 *      This implementation relies upon a remove directory operation failing
 *      if the directory contains any files.
 */

#define _GNU_SOURCE /* For O_NOFOLLOW */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/param.h>
#endif
#include "vmware.h"
#include "hostinfo.h"
#include "util.h"
#include "err.h"
#include "log.h"
#include "str.h"
#include "fileIO.h"
#include "fileLock.h"
#include "fileInt.h"
#include "random.h"
#include "vm_atomic.h"
#include "util.h"
#include "hostType.h"

#include "unicodeOperations.h"

#define LOGLEVEL_MODULE main
#include "loglevel_user.h"

#define LOCK_SHARED     "S"
#define LOCK_EXCLUSIVE  "X"
#define FILELOCK_PROGRESS_DEARTH 8000 // Dearth of progress time in milliseconds
#define FILELOCK_PROGRESS_SAMPLE 200  // Progress sampling time in milliseconds

static char implicitReadToken;

#define PARSE_TABLE_UINT   0
#define PARSE_TABLE_STRING 1

typedef struct parse_table
{
   int    type;
   char  *name;
   void  *valuePtr;
} ParseTable;

/*
 * The lock token. This is returned by the lock operation and must be sent
 * to the unlock operation.
 */

#define FILELOCK_TOKEN_SIGNATURE 0x4B434C46  // 'FLCK' in memory

struct FileLockToken
{
   uint32  signature;
   Bool    portable;
   char   *pathName;
   union {
      struct {
         FileIODescriptor lockFd;
      } mandatory;
      struct {
         char *lockFilePath;  // &implicitReadToken for implicit read locks
      } portable;
   } u;
};


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockSleeper --
 *
 *      Have the calling thread sleep "for a while". The duration of the
 *      sleep is determined by the count that is passed in. Checks are
 *      also done for exceeding the maximum wait time.
 *
 * Results:
 *      0       slept
 *      EAGAIN  maximum sleep time exceeded
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockSleeper(LockValues *myValues)  // IN/OUT:
{
   VmTimeType ageMsec;
   uint32 maxSleepTimeMsec;

   if (myValues->maxWaitTimeMsec == FILELOCK_TRYLOCK_WAIT) {
      return EAGAIN;
   }

   ageMsec = Hostinfo_SystemTimerMS() - myValues->startTimeMsec;

   if ((myValues->maxWaitTimeMsec != FILELOCK_INFINITE_WAIT) &&
       (ageMsec >= myValues->maxWaitTimeMsec)) {
      return EAGAIN;
   }

   if (ageMsec <= 2000) {
      /* Most locks are "short" */
      maxSleepTimeMsec = 100;
   } else {
      /*
       * The lock has been around a while; use a continuously increasing back
       * off with an upper bound.
       */

      maxSleepTimeMsec = MIN(ageMsec / 10, 2000);
   }

   /*
    * Randomize the time slept. This will prevent any potential cadence issues
    * (thundering herds).
    */

   (void) FileSleeper(maxSleepTimeMsec / 2, maxSleepTimeMsec);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockRemoveLockingFile --
 *
 *      Remove the specified file.
 *
 * Results:
 *      0       success
 *      > 0     failure (errno)
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockRemoveLockingFile(const char *lockDir,   // IN:
                          const char *fileName)  // IN:
{
   int err;
   char *path;

   ASSERT(lockDir != NULL);
   ASSERT(fileName != NULL);

   path = Unicode_Join(lockDir, DIRSEPS, fileName, NULL);

   err = FileDeletionRobust(path, FALSE);

   if (err != 0) {
      if (err == ENOENT) {
         /* Not there anymore; locker unlocked or timed out */
         err = 0;
      } else {
         Warning(LGPFX" %s of '%s' failed: %s\n", __FUNCTION__,
                 path, Err_Errno2String(err));
      }
   }

   Posix_Free(path);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockParseArgs --
 *
 *      Parse the property list arguments of a lock file. The ParseTable
 *      contains names of properies that are interesting to the caller;
 *      only those values associated with the interesting names will be
 *      extracted, the others will be ignored.
 *
 * Results:
 *      TRUE    An error was detected
 *      FALSE   All is well
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
FileLockParseArgs(char *argv[],       // IN:
                  uint32 argCount,    // IN:
                  ParseTable *table,  // IN:
                  uint32 tableSize)   // IN:
{
   uint32 argPos = 5;  // The property list always starts with this argument

   while (argCount) {
      uint32 i;
      char *p = strchr(argv[argPos], '=');

      /* Validate the "name=value" form */
      if ((p == NULL) || (p == argv[argPos]) || (p[1] == '\0')) {
         return TRUE;
      }

      *p = '\0';

      /* Unknown names are ignored without error */
      for (i = 0; i < tableSize; i++) {
         if (strcmp(argv[argPos], table[i].name) == 0) {
            switch (table[i].type) {
            case PARSE_TABLE_UINT:
               if (sscanf(&p[1], "%u", (uint32 *) table[i].valuePtr) != 1) {
                  return TRUE;
               }
               break;

            case PARSE_TABLE_STRING:
               *((char **) table[i].valuePtr) = &p[1];
               break;
            }
         }
      }

      *p = '=';

      argPos++;
      argCount--;
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockMemberValues --
 *
 *      Returns the values associated with lock directory file.
 *
 * Results:
 *      0       Valid lock file; values have been returned
 *      > 0     Lock file problem (errno); values have not been returned
 *
 * Side effects:
 *      The lock file may be deleted if it is invalid
 *
 *-----------------------------------------------------------------------------
 */

#define FL_MAX_ARGS 16

int
FileLockMemberValues(const char *lockDir,       // IN:
                     const char *fileName,      // IN:
                     char *buffer,              // OUT:
                     size_t requiredSize,       // IN:
                     LockValues *memberValues)  // OUT:
{
   size_t len;
   int access;
   char *path;
   FileData fileData;
   FileIOResult result;
   FileIODescriptor desc;
   char *argv[FL_MAX_ARGS];

   int err = 0;
   uint32 argc = 0;
   char *saveptr = NULL;

   ParseTable table[] = {
                           { PARSE_TABLE_STRING,
                             "lc",
                             (void *) &memberValues->locationChecksum
                           }
                        };

   ASSERT(lockDir != NULL);
   ASSERT(fileName != NULL);

   path = Unicode_Join(lockDir, DIRSEPS, fileName, NULL);

   FileIO_Invalidate(&desc);

   access = FILEIO_OPEN_ACCESS_READ;

#if defined(_WIN32)
   access |= FILEIO_OPEN_SHARE_DELETE;
#endif

   result = FileIOCreateRetry(&desc, path, access, FILEIO_OPEN, 0444,
                              FILE_MAX_WAIT_TIME_MS);

   if (!FileIO_IsSuccess(result)) {
      err = FileMapErrorToErrno(__FUNCTION__, Err_Errno());

      /*
       * A member file may "disappear" if is deleted (unlinked on POSIXen)
       * due to an unlock immediately after a directory scan but before the
       * scan is processed. Since this is a "normal" thing, ENOENT will be
       * suppressed.
       */

      if (err != ENOENT) {
         Warning(LGPFX" %s open failure on '%s': %s\n", __FUNCTION__,
                 path, Err_Errno2String(err));
      }

      goto bail;
   }

   /* Attempt to obtain the lock file attributes now that it is opened */
   err = FileAttributesRobust(path, &fileData);

   if (err != 0) {
      /*
       * A member file may "disappear" if is deleted (unlinked on POSIXen)
       * due to an unlock immediately after a directory scan but before the
       * scan is processed. Since this is a "normal" thing, ENOENT will be
       * suppressed.
       */

      if (err != ENOENT) {
         Warning(LGPFX" %s file size failure on '%s': %s\n", __FUNCTION__,
                 path, Err_Errno2String(err));
      }

      FileIO_Close(&desc);

      goto bail;
   }

   /* Complain if the lock file is not the proper size */
   if (fileData.fileSize != requiredSize) {
      Warning(LGPFX" %s file '%s': size %"FMT64"u, required size %"FMTSZ"d\n",
              __FUNCTION__, path, fileData.fileSize, requiredSize);

      FileIO_Close(&desc);

      goto corrupt;
   }

   /* Attempt to read the lock file data and validate how much was read. */
   result = FileIO_Read(&desc, buffer, requiredSize, &len);

   FileIO_Close(&desc);

   if (!FileIO_IsSuccess(result)) {
      err = FileMapErrorToErrno(__FUNCTION__, Err_Errno());

      Warning(LGPFX" %s read failure on '%s': %s\n",
              __FUNCTION__, path, Err_Errno2String(err));

      goto bail;
   }

   if (len != requiredSize) {
      Warning(LGPFX" %s read length issue on '%s': %"FMTSZ"d and %"FMTSZ"d\n",
              __FUNCTION__, path, len, requiredSize);

      err = EIO;
      goto bail;
   }

fixedUp:

   /* Extract and validate the lock file data. */
   for (argc = 0; argc < FL_MAX_ARGS; argc++) {
      argv[argc] = strtok_r((argc == 0) ? buffer : NULL, " ", &saveptr);

      if (argv[argc] == NULL) {
         break;
      }
   }

   /*
    * Lock file arguments are space separated. There is a minimum of 5
    * arguments - machineID, executionID, Lamport number, lock type
    * and process creation time. The maximum number of arguments is
    * FL_MAX_ARGS.
    *
    * Additional arguments, if present, form a property list - one or more
    * "name=value" pairs.
    *
    * Here is picture of valid forms:
    *
    * 0 1 2 3 4 5 6   Comment
    *-------------------------
    * A B C D E       No property list
    * A B C D E x     One property
    * A B C D E x y   Two properties
    */

   memberValues->locationChecksum = NULL;

   if ((argc < 5) || ((argc == FL_MAX_ARGS) &&
                       (strtok_r(NULL, " ", &saveptr) != NULL))) {
      goto corrupt;
   }

   if ((argc > 5) && FileLockParseArgs(argv, argc - 5,
                                       table, ARRAYSIZE(table))) {
      goto corrupt;
   }

   /*
    * Check for an old style lock file; if found, upgrade it (internally).
    *
    * The new style lock always has an executionID that is minimally
    * processID-processCreationTime (the '-' is the critical difference).
    */

   if ((strchr(argv[1], '-') == NULL) &&
       (strchr(argv[1], '(') == NULL) &&
       (strchr(argv[1], ')') == NULL) &&
       (argc == 6) &&
       !FileLockParseArgs(argv, argc - 5, table, ARRAYSIZE(table))) {
         char *newBuffer;

         newBuffer = Str_SafeAsprintf(NULL, "%s %s-%s %s %s %s %s",
                                      argv[0], argv[1], argv[4], argv[2],
                                      argv[3], argv[4], argv[5]);

        Str_Strcpy(buffer, newBuffer, requiredSize);

        Posix_Free(newBuffer);

        goto fixedUp;
  }

   if (sscanf(argv[2], "%u", &memberValues->lamportNumber) != 1) {
      goto corrupt;
   }

   if ((strcmp(argv[3], LOCK_SHARED) != 0) &&
       (strcmp(argv[3], LOCK_EXCLUSIVE) != 0)) {
      goto corrupt;
   }

   memberValues->machineID = argv[0];
   memberValues->executionID = argv[1];
   memberValues->lockType = argv[3];
   memberValues->memberName = Unicode_Duplicate(fileName);

   Posix_Free(path);

   return 0;

corrupt:
   Warning(LGPFX" %s removing problematic lock file '%s'\n", __FUNCTION__,
           path);

   if (argc) {
      uint32 i;

      Log(LGPFX" %s '%s' contents are:\n", __FUNCTION__, fileName);

      for (i = 0; i < argc; i++) {
         Log(LGPFX" %s %s argv[%u]: '%s'\n", __FUNCTION__, fileName,
             i, argv[i]);
      }
   }

   /* Remove the lock file and behave like it has disappeared */
   err = FileDeletionRobust(path, FALSE);

   if (err == 0) {
      err = ENOENT;
   }

bail:
   Posix_Free(path);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockValidName --
 *
 *      Validate the format of the file name.
 *
 * Results:
 *      TRUE    Yes
 *      FALSE   No
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
FileLockValidName(const char *fileName)  // IN:
{
   uint32 i;

   ASSERT(fileName != NULL);

   /* The fileName must start with the ASCII character, 'M', 'D' or 'E' */
   if (Unicode_FindSubstrInRange("MDE", 0, -1, fileName, 0,
                                 1) == UNICODE_INDEX_NOT_FOUND) {
      return FALSE;
   }

   /* The fileName must contain 5 ASCII digits after the initial character */
   for (i = 0; i < 5; i++) {
      if (Unicode_FindSubstrInRange("0123456789", 0, -1, fileName, i + 1,
                                    1) == UNICODE_INDEX_NOT_FOUND) {
         return FALSE;
      }
   }

   /* The fileName must terminate with the appropriate suffix string */
   return Unicode_EndsWith(fileName, FILELOCK_SUFFIX);
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockActivateList
 *
 *      Insure a lock list entry exists for the lock directory.
 *
 * Results:
 *     0        success
 *     > 0      error (errno)
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockActivateList(const char *dirName,   // IN:
                     LockValues *myValues)  // IN:
{
   ActiveLock   *ptr;

   ASSERT(dirName != NULL);
   ASSERT(*dirName == 'D');

   /* Search the list for a matching entry */
   for (ptr = myValues->lockList; ptr != NULL; ptr = ptr->next) {
      if (Unicode_Compare(ptr->dirName, dirName) == 0) {
         break;
      }
   }

   /* No entry? Attempt to add one. */
   if (ptr == NULL) {
      ptr = Util_SafeMalloc(sizeof *ptr);

      ptr->next = myValues->lockList;
      myValues->lockList = ptr;

      ptr->age = 0;
      ptr->dirName = Unicode_Duplicate(dirName);
   }

   /* Mark the entry (exists) */
   ptr->marked = TRUE;

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockLocationChecksum --
 *
 *      Compute the location checksum of the argument path.
 *
 * Results:
 *      The location checksum as dynamically allocated string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
FileLockLocationChecksum(const char *path)  // IN:
{
   int c;
   uint32 hash = 5381;

#if defined(_WIN32)
   char *p;
   char *value = Unicode_Duplicate(path);

   /* Don't get fooled by mixed case; "normalize" */
   Str_ToLower(value);
   p = value;
#else
   char *p = (char *) path;
#endif

   /* DBJ2 hash... good enough? */
   while ((c = *p++)) {
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
   }

#if defined(_WIN32)
   Posix_Free(value);
#endif

   return Str_SafeAsprintf(NULL, "%u", hash);
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockScanDirectory --
 *
 *      Call the specified function for each member file found in the
 *      specified directory.
 *
 * Results:
 *      0       success
 *      > 0     failure
 *
 * Side effects:
 *     Anything that this not a valid locking file is deleted.
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockScanDirectory(const char *lockDir,      // IN:
                      int (*func)(              // IN:
                             const char *lockDir,
                             const char *fileName,
                             LockValues *memberValues,
                             LockValues *myValues
                           ),
                      LockValues *myValues,    // IN:
                      Bool cleanUp)            // IN:
{
   uint32 i;
   int err;
   int numEntries;

   char **fileList = NULL;
   char *myExecutionID = NULL;
   char *locationChecksum = NULL;

   ASSERT(lockDir != NULL);

   numEntries = FileListDirectoryRobust(lockDir, &fileList);

   if (numEntries == -1) {
      Log(LGPFX" %s: Could not read the directory '%s': %d\n",
          __FUNCTION__, lockDir, Err_Errno());

      return EDOM;  // out of my domain
   }

   /* Pass 1: Validate entries and handle any 'D' entries */
   for (i = 0, err = 0; i < numEntries; i++) {
      /* Remove any non-locking files */
      if (!FileLockValidName(fileList[i])) {
         Log(LGPFX" %s discarding %s from %s'; invalid file name.\n",
             __FUNCTION__, fileList[i], lockDir);

         err = FileLockRemoveLockingFile(lockDir, fileList[i]);
         if (err != 0) {
            goto bail;
         }

        Posix_Free(fileList[i]);
        fileList[i] = NULL;

        continue;
      }

      /*
       * Any lockers appear to be entering?
       *
       * This should be rather rare. If a locker dies while entering
       * this will cleaned-up.
       */

      if (*fileList[i] == 'D') {
         if (cleanUp) {
            err = FileLockActivateList(fileList[i], myValues);
            if (err != 0) {
               goto bail;
            }
        }

        Posix_Free(fileList[i]);
        fileList[i] = NULL;
      }
   }

   if (myValues->lockList != NULL) {
      goto bail;
   }

   myExecutionID = FileLockGetExecutionID();
   locationChecksum = FileLockLocationChecksum(lockDir);

   /* Pass 2: Handle the 'M' entries */
   for (i = 0, err = 0; i < numEntries; i++) {
      LockValues *ptr;
      Bool       myLockFile;
      LockValues memberValues;

      if ((fileList[i] == NULL) || (*fileList[i] == 'E')) {
         continue;
      }

      myLockFile = (Unicode_Compare(fileList[i],
                          myValues->memberName) == 0) ? TRUE : FALSE;

      if (myLockFile) {
         /* It's me! No need to read or validate anything. */
         ptr = myValues;
      } else {
         char buffer[FILELOCK_DATA_SIZE];
         /* It's not me! Attempt to extract the member values. */
         err = FileLockMemberValues(lockDir, fileList[i], buffer,
                                    FILELOCK_DATA_SIZE, &memberValues);

         if (err != 0) {
            if (err == ENOENT) {
               err = 0;
               /* Not there anymore; locker unlocked or timed out */
               continue;
            }

            break;
         }

         /* Remove any stale locking files */
         if (FileLockMachineIDMatch(myValues->machineID,
                                    memberValues.machineID)) {
            char *dispose = NULL;

            if (FileLockValidExecutionID(memberValues.executionID)) {
               /* If it's mine it better still be where I put it! */
               if ((strcmp(myExecutionID, memberValues.executionID) == 0) &&
                   ((memberValues.locationChecksum != NULL) &&
                    (strcmp(memberValues.locationChecksum,
                            locationChecksum) != 0))) {
                  dispose = Unicode_Duplicate("lock file has been moved.");
               }
            } else {
               dispose = Str_SafeAsprintf(NULL, "invalid executionID %s.",
                                          memberValues.executionID);
            }

            if (dispose) {
               Log(LGPFX" %s discarding %s from %s': %s\n",
                   __FUNCTION__, fileList[i], lockDir, dispose);

               Posix_Free(dispose);
               Posix_Free(memberValues.memberName);

               err = FileLockRemoveLockingFile(lockDir, fileList[i]);
               if (err != 0) {
                  break;
               }

               continue;
            }
         }

         ptr = &memberValues;
      }

      /* Locking file looks good; see what happens */
      err = (*func)(lockDir, fileList[i], ptr, myValues);

      if (ptr == &memberValues) {
         Posix_Free(memberValues.memberName);
      }

      if (err != 0) {
         break;
      }
   }

bail:

   Util_FreeStringList(fileList, numEntries);

   Posix_Free(locationChecksum);
   Posix_Free(myExecutionID);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockScanner --
 *
 *      Call the specified function for each member file found in the
 *      specified directory. If a rescan is necessary check the list
 *      of outstanding locks and handle removing stale locks.
 *
 * Results:
 *     0        success
 *     > 0      failure
 *
 * Side effects:
 *     None
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockScanner(const char *lockDir,     // IN:
                int (*func)(             // IN:
                       const char *lockDir,
                       const char *fileName,
                       LockValues *memberValues,
                       LockValues *myValues
                    ),
                LockValues *myValues,    // IN:
                Bool cleanUp)            // IN:
{
   int        err;
   ActiveLock *ptr;

   ASSERT(lockDir != NULL);

   myValues->lockList = NULL;

   while (TRUE) {
      ActiveLock *prev;

      err = FileLockScanDirectory(lockDir, func, myValues, cleanUp);
      if ((err > 0) || ((err == 0) && (myValues->lockList == NULL))) {
         break;
      }

      prev = NULL;
      ptr = myValues->lockList;

      /*
       * Some 'D' entries have persisted. Age them and remove those that
       * have not progressed. Remove those that have disappeared.
       */

      while (ptr != NULL) {
         Bool remove;

         if (ptr->marked) {
            if (ptr->age > FILELOCK_PROGRESS_DEARTH) {
               char *temp;
               char *path;
               UnicodeIndex index;

               ASSERT(*ptr->dirName == 'D');

               Log(LGPFX" %s discarding %s data from '%s'.\n",
                   __FUNCTION__, ptr->dirName, lockDir);

               path = Unicode_Join(lockDir, DIRSEPS, ptr->dirName, NULL);

               index = Unicode_FindLast(path, "D");
               ASSERT(index != UNICODE_INDEX_NOT_FOUND);

               temp = Unicode_Replace(path, index, 1, "M");
               FileDeletionRobust(temp, FALSE);
               Posix_Free(temp);

               temp = Unicode_Replace(path, index, 1, "E");
               FileDeletionRobust(temp, FALSE);
               Posix_Free(temp);

               FileRemoveDirectoryRobust(path);

               Posix_Free(path);

               remove = TRUE;
            } else {
               ptr->marked = FALSE;
               ptr->age += FILELOCK_PROGRESS_SAMPLE;

               remove = FALSE;
            }
         } else {
            remove = TRUE;
         }

         if (remove) {
            ActiveLock *temp = ptr;

            ptr = ptr->next;
            Posix_Free(temp->dirName);
            Posix_Free(temp);

            if (prev == NULL) {
               myValues->lockList = ptr;
            } else {
               prev->next = ptr;
            }
         } else {
            prev = ptr;
            ptr = ptr->next;
         }
      }

      FileSleeper(FILELOCK_PROGRESS_SAMPLE,
                  FILELOCK_PROGRESS_SAMPLE); // relax
   }

   /* Clean up anything still on the list; they are no longer important */
   while (myValues->lockList != NULL) {
      ptr = myValues->lockList;
      myValues->lockList = ptr->next;

      Posix_Free(ptr->dirName);

      Posix_Free(ptr);
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileUnlockIntrinsic --
 *
 *      Release a lock on a file.
 *
 * Results:
 *      0       unlocked
 *      > 0     errno
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
FileUnlockIntrinsic(FileLockToken *tokenPtr)  // IN:
{
   int err = 0;

   ASSERT(tokenPtr && (tokenPtr->signature == FILELOCK_TOKEN_SIGNATURE));

   LOG(1, "Requesting unlock on %s\n", tokenPtr->pathName);

   if (tokenPtr->portable) {
      /*
       * If the lockFilePath (a pointer) is the fixed-address token representing
       * an implicit read lock, there is no lock file and the token can simply
       * be discarded.
       */

      if (tokenPtr->u.portable.lockFilePath != &implicitReadToken) {
         char *lockDir;

         /* The lock directory path */
         lockDir = Unicode_Append(tokenPtr->pathName, FILELOCK_SUFFIX);

         /*
          * TODO: under vmx86_debug validate the contents of the lock file as
          *       matching the machineID and executionID.
          */

         err = FileDeletionRobust(tokenPtr->u.portable.lockFilePath, FALSE);

         FileRemoveDirectoryRobust(lockDir); // just in case we can clean up

         if (err && vmx86_debug) {
            Log(LGPFX" %s failed for '%s': %s\n", __FUNCTION__,
                tokenPtr->u.portable.lockFilePath, Err_Errno2String(err));
         }
         Posix_Free(lockDir);
         Posix_Free(tokenPtr->u.portable.lockFilePath);
      }

      tokenPtr->u.portable.lockFilePath = NULL;  // Just in case...
   } else {
      ASSERT(FileIO_IsValid(&tokenPtr->u.mandatory.lockFd));

      if (!FileIO_IsSuccess(FileIO_CloseAndUnlink(&tokenPtr->u.mandatory.lockFd))) {
         /*
          * Should succeed, but there is an unavoidable race:
          * close() must precede unlink(), but another thread could touch
          * the file between close() and unlink(). We only worry about other
          * FileLock-like manipulations; the advisory lock file should not
          * experience any name collisions. Treat races as success.
          * Specific errors:
          *    EBUSY: other locked file
          *    ENOENT: other locked + unlocked (w/ implicit unlink) file
          */
         if (Err_Errno() == EBUSY || Err_Errno() == ENOENT) {
            LOG(0, "Tolerating %s on unlink of advisory lock at %s\n",
                Err_Errno() == EBUSY ? "EBUSY" : "ENOENT", tokenPtr->pathName);
         } else {
            err = Err_Errno();
            if (vmx86_debug) {
               Log(LGPFX" %s failed for advisory lock '%s': %s\n", __FUNCTION__,
                   tokenPtr->pathName, Err_Errno2String(err));
            }
         }
      }
   }

   Posix_Free(tokenPtr->pathName);
   tokenPtr->signature = 0;        // Just in case...
   tokenPtr->pathName = NULL;      // Just in case...
   Posix_Free(tokenPtr);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockWaitForPossession --
 *
 *      Wait until the caller has a higher priority towards taking
 *      possession of a lock than the specified file.
 *
 * Results:
 *     0        success
 *     > 0      error (errno)
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockWaitForPossession(const char *lockDir,       // IN:
                          const char *fileName,      // IN:
                          LockValues *memberValues,  // IN:
                          LockValues *myValues)      // IN:
{
   int err = 0;

   ASSERT(lockDir != NULL);
   ASSERT(fileName != NULL);

   /* "Win" or wait? */
   if (((memberValues->lamportNumber < myValues->lamportNumber) ||
       ((memberValues->lamportNumber == myValues->lamportNumber) &&
          (Unicode_Compare(memberValues->memberName,
                           myValues->memberName) < 0))) &&
        ((strcmp(memberValues->lockType, LOCK_EXCLUSIVE) == 0) ||
         (strcmp(myValues->lockType, LOCK_EXCLUSIVE) == 0))) {
      Bool thisMachine = FileLockMachineIDMatch(myValues->machineID,
                                                memberValues->machineID);
      char *path = Unicode_Join(lockDir, DIRSEPS, fileName, NULL);

      while ((err = FileLockSleeper(myValues)) == 0) {
         /* still there? */
         err = FileAttributesRobust(path, NULL);
         if (err != 0) {
            if (err == ENOENT) {
               /* Not there anymore; locker unlocked or timed out */
               err = 0;
            }

            break;
         }

         /* still valid? */
         if (thisMachine &&
             !FileLockValidExecutionID(memberValues->executionID)) {
            /* Invalid Execution ID; remove the member file */
            Warning(LGPFX" %s discarding file '%s'; invalid executionID.\n",
                    __FUNCTION__, path);

            err = FileLockRemoveLockingFile(lockDir, fileName);
            break;
         }
      }

      /*
       * Log the disposition of each timeout for all non "try lock" locking
       * attempts. This can assist in debugging locking problems.
       */

      if ((myValues->maxWaitTimeMsec != FILELOCK_TRYLOCK_WAIT) &&
          (err == EAGAIN)) {
         if (thisMachine) {
            Log(LGPFX" %s timeout on '%s' due to a local process '%s'\n",
                __FUNCTION__, path, memberValues->executionID);
         } else {
            Log(LGPFX" %s timeout on '%s' due to another machine '%s'\n",
                __FUNCTION__, path, memberValues->machineID);
         }
      }

      Posix_Free(path);
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockNumberScan --
 *
 *      Determine the maxmimum number value within the current locking set.
 *
 * Results:
 *     0        success
 *     > 0      failure (errno)
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockNumberScan(const char *lockDir,       // IN:
                   const char *fileName,      // IN:
                   LockValues *memberValues,  // IN:
                   LockValues *myValues)      // IN/OUT:
{
   ASSERT(lockDir != NULL);
   ASSERT(fileName != NULL);

   if (memberValues->lamportNumber > myValues->lamportNumber) {
      myValues->lamportNumber = memberValues->lamportNumber;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockMakeDirectory --
 *
 *      Create a directory.
 *
 * Results:
 *      0       success
 *      > 0     failure (errno)
 *
 * Side Effects:
 *      File system may be modified.
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockMakeDirectory(const char *pathName)  // IN:
{
   int err;

#if !defined(_WIN32)
   mode_t save;

   save = umask(0);
#endif

   ASSERT(pathName != NULL);

   err = FileCreateDirectoryRobust(pathName, 0777);

#if !defined(_WIN32)
   umask(save);
#endif

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockCreateEntryDirectory --
 *
 *      Create an entry directory in the specified locking directory.
 *
 *      Due to FileLock_Unlock() attempting to remove the locking
 *      directory on an unlock operation (to "clean up" and remove the
 *      locking directory when it is no longer needed), this routine
 *      must carefully handle a number of race conditions to insure the
 *      the locking directory exists and the entry directory is created
 *      within.
 *
 * Results:
 *      0       success
 *      > 0     failure (errno)
 *
 * Side Effects:
 *      On success returns the number identifying the entry directory and
 *      the entry directory path name.
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockCreateEntryDirectory(const char *lockDir,    // IN:
                             char **entryDirectory,  // OUT:
                             char **entryFilePath,   // OUT:
                             char **memberFilePath,  // OUT:
                             char **memberName)      // OUT:
{
   int err;
   VmTimeType startTimeMsec;

   ASSERT(lockDir != NULL);

   *entryDirectory = NULL;
   *entryFilePath = NULL;
   *memberFilePath = NULL;
   *memberName = NULL;

   /* Fun at the races */
   startTimeMsec = Hostinfo_SystemTimerMS();

   while (TRUE) {
      char *temp;
      FileData fileData;
      VmTimeType ageMsec;
      uint32 randomNumber;

      err = FileAttributesRobust(lockDir, &fileData);
      if (err == 0) {
        /* The name exists. Deal with it... */

        if (fileData.fileType == FILE_TYPE_REGULAR) {
           /*
            * It's a file. Assume this is an (active?) old style lock and
            * err on the safe side - don't remove it (and automatically
            * upgrade to a new style lock).
            */

            Log(LGPFX" %s: '%s' exists; an old style lock file?\n",
                __FUNCTION__, lockDir);

            err = EBUSY;
            break;
        }

        if (fileData.fileType != FILE_TYPE_DIRECTORY) {
           /* Not a directory; attempt to remove the debris */
           if (FileDeletionRobust(lockDir, FALSE) != 0) {
              Warning(LGPFX" %s: '%s' exists and is not a directory.\n",
                      __FUNCTION__, lockDir);

              err = ENOTDIR;
              break;
           }

           continue;
        }
      } else {
         if (err == ENOENT) {
            /* Not there anymore; locker unlocked or timed out */
            err = FileLockMakeDirectory(lockDir);

            if ((err != 0) && (err != EEXIST)) {
               Warning(LGPFX" %s creation failure on '%s': %s\n",
                       __FUNCTION__, lockDir, Err_Errno2String(err));

               break;
            }
         } else {
            Warning(LGPFX" %s stat failure on '%s': %s\n",
                    __FUNCTION__, lockDir, Err_Errno2String(err));

            break;
         }
      }

      /* There is a small chance of collision/failure; grab stings now */
      randomNumber = (FileSimpleRandom() >> 8) & 0xFFFF;

      *memberName = Unicode_Format("M%05u%s", randomNumber, FILELOCK_SUFFIX);

      temp = Unicode_Format("D%05u%s", randomNumber, FILELOCK_SUFFIX);
      *entryDirectory = Unicode_Join(lockDir, DIRSEPS, temp, NULL);
      Posix_Free(temp);

      temp = Unicode_Format("E%05u%s", randomNumber, FILELOCK_SUFFIX);
      *entryFilePath = Unicode_Join(lockDir, DIRSEPS, temp, NULL);
      Posix_Free(temp);

      *memberFilePath = Unicode_Join(lockDir, DIRSEPS, *memberName, NULL);

      err = FileLockMakeDirectory(*entryDirectory);

      if (err == 0) {
         /*
          * The entry directory was safely created. See if a member file
          * is in use (the entry directory is removed once the member file
          * is created). If a member file is in use, choose another number,
          * otherwise the use of the this number is OK.
          *
          * Err on the side of caution... don't want to trash perfectly
          * good member files.
          */

         err = FileAttributesRobust(*memberFilePath, NULL);

         if (err != 0) {
            if (err == ENOENT) {
               err = 0;
               break;
            }

            if (vmx86_debug) {
               Log(LGPFX" %s stat failure on '%s': %s\n",
                   __FUNCTION__, *memberFilePath, Err_Errno2String(err));
             }
         }

         err = FileRemoveDirectoryRobust(*entryDirectory);

         if (err != 0) {
            Warning(LGPFX" %s unable to remove '%s': %s\n",
                    __FUNCTION__, *entryDirectory, Err_Errno2String(err));

            break;
         }
      } else {
          if ((err != EEXIST) &&  // Another process/thread created it...
              (err != ENOENT)) {  // lockDir is gone...
             Warning(LGPFX" %s creation failure on '%s': %s\n",
                     __FUNCTION__, *entryDirectory, Err_Errno2String(err));

             break;
          }
      }

      Posix_Free(*entryDirectory);
      Posix_Free(*entryFilePath);
      Posix_Free(*memberFilePath);
      Posix_Free(*memberName);

      *entryDirectory = NULL;
      *entryFilePath = NULL;
      *memberFilePath = NULL;
      *memberName = NULL;

      /*
       * If we've been trying to get the locking started for a unacceptable
       * amount of time, bail. Something is seriously wrong, probably the
       * file system or networking. Nothing we can do about it.
       */

      ageMsec = Hostinfo_SystemTimerMS() - startTimeMsec;

      if (ageMsec > FILELOCK_PROGRESS_DEARTH) {
         Warning(LGPFX" %s lack of progress on '%s'\n", __FUNCTION__, lockDir);

         err = EBUSY;
         break;
      }
   }

   if (err != 0) {
      Posix_Free(*entryDirectory);
      Posix_Free(*entryFilePath);
      Posix_Free(*memberFilePath);
      Posix_Free(*memberName);

      *entryDirectory = NULL;
      *entryFilePath = NULL;
      *memberFilePath = NULL;
      *memberName = NULL;
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockCreateMemberFile --
 *
 *      Create the member file.
 *
 * Results:
 *     0        success
 *     > 0      failure (errno)
 *
 * Side Effects:
 *     None
 *
 *-----------------------------------------------------------------------------
 */

static int
FileLockCreateMemberFile(FileIODescriptor *desc,       // IN:
                         const LockValues *myValues,   // IN:
                         const char *entryFilePath,    // IN:
                         const char *memberFilePath)   // IN:
{
   int cnt;
   int pid;
   size_t len;
   FileIOResult result;
   uint64 processCreationTime;

   int err = 0;
   char buffer[FILELOCK_DATA_SIZE] = { 0 };

   ASSERT(entryFilePath != NULL);
   ASSERT(memberFilePath != NULL);

   /*
    * Populate the buffer with appropriate data
    *
    * Lock file arguments are space separated. There is a minimum of 5
    * arguments - machineID, executionID, Lamport number, lock type
    * and process creation time. The maximum number of arguments is
    * FL_MAX_ARGS.
    *
    * Additional arguments, if present, form a property list - one or more
    * "name=value" pairs.
    *
    * Yes, the process creation time is redundently encoded. This is necessary
    * to maintain backwards compatibility. Should an older code pick up a
    * newer lock file and there is lock contention, the older code will log
    * the name of the process causing the contention - it's also encoded
    * into the executionID.
    */

   cnt = sscanf(myValues->executionID, "%d-%"FMT64"u", &pid,
                &processCreationTime);

   ASSERT(cnt == 2);  // ensure new format executionID

   Str_Sprintf(buffer, sizeof buffer, "%s %s %u %s %"FMT64"u lc=%s",
               myValues->machineID,
               myValues->executionID,
               myValues->lamportNumber,
               myValues->lockType,
               processCreationTime,
               myValues->locationChecksum);

   /* Attempt to write the data */
   result = FileIO_Write(desc, buffer, sizeof buffer, &len);

   if (!FileIO_IsSuccess(result)) {
      err = FileMapErrorToErrno(__FUNCTION__, Err_Errno());

      Warning(LGPFX" %s write of '%s' failed: %s\n", __FUNCTION__,
              entryFilePath, Err_Errno2String(err));

      FileIO_Close(desc);

      return err;
   }

   if (!FileIO_IsSuccess(FileIO_Close(desc))) {
      err = FileMapErrorToErrno(__FUNCTION__, Err_Errno());

      Warning(LGPFX" %s close of '%s' failed: %s\n", __FUNCTION__,
              entryFilePath, Err_Errno2String(err));

      return err;
   }

   if (len != sizeof buffer) {
      Warning(LGPFX" %s write length issue on '%s': %"FMTSZ"d and %"FMTSZ"d\n",
              __FUNCTION__, entryFilePath, len, sizeof buffer);

      return EIO;
   }

   err = File_Rename(entryFilePath, memberFilePath);

   if (err != 0) {
      Warning(LGPFX" %s FileRename of '%s' to '%s' failed: %s\n",
              __FUNCTION__, entryFilePath, memberFilePath,
              Err_Errno2String(err));

      if (vmx86_debug) {
         Log(LGPFX" %s FileLockFileType() of '%s': %s\n",
             __FUNCTION__, entryFilePath,
            Err_Errno2String(FileAttributesRobust(entryFilePath, NULL)));

         Log(LGPFX" %s FileLockFileType() of '%s': %s\n",
             __FUNCTION__, memberFilePath,
            Err_Errno2String(FileAttributesRobust(memberFilePath, NULL)));
      }

      return err;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockIntrinsicMandatory --
 *
 *      Obtain a lock on a file; shared or exclusive access.
 *
 *      This implementation uses the FILEIO_OPEN_LOCK_MANDATORY flag,
 *      which requires kernel support for mandatory locking. Such locks
 *      are automatically broken if the host holding the lock fails.
 *
 *      maxWaitTimeMsec specifies the maximum amount of time, in
 *      milliseconds, to wait for the lock before returning the "not
 *      acquired" status. A value of FILELOCK_TRYLOCK_WAIT is the
 *      equivalent of a "try lock" - the lock will be acquired only if
 *      there is no contention. A value of FILELOCK_INFINITE_WAIT
 *      specifies "waiting forever" to acquire the lock.
 *
 * Results:
 *      NULL    Lock not acquired. Check err.
 *              err     0       Lock Timed Out
 *              err     > 0     errno
 *      !NULL   Lock Acquired. This is the "lockToken" for an unlock.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static FileLockToken *
FileLockIntrinsicMandatory(const char *pathName,   // IN:
                           const char *lockFile,   // IN:
                           LockValues *myValues,   // IN/OUT:
                           int *err)               // OUT:
{
   int access;
   int errnum;
   FileIOResult result;
   FileLockToken *tokenPtr = Util_SafeMalloc(sizeof *tokenPtr);

   tokenPtr->signature = FILELOCK_TOKEN_SIGNATURE;
   tokenPtr->portable = FALSE;
   tokenPtr->pathName = Unicode_Duplicate(pathName);
   FileIO_Invalidate(&tokenPtr->u.mandatory.lockFd);

   access = myValues->exclusivity ? FILEIO_OPEN_ACCESS_WRITE :
                                    FILEIO_OPEN_ACCESS_READ;
   access |= FILEIO_OPEN_EXCLUSIVE_LOCK;

   do {
      result = FileIOCreateRetry(&tokenPtr->u.mandatory.lockFd,
                                 lockFile, access,
                                 FILEIO_OPEN_CREATE, 0600,
                                 0);
      errnum = Err_Errno();
      if (result != FILEIO_LOCK_FAILED) {
         break;
      }
   } while (FileLockSleeper(myValues) == 0);

   if (FileIO_IsSuccess(result)) {
      ASSERT(FileIO_IsValid(&tokenPtr->u.mandatory.lockFd));
      *err = 0;

      return tokenPtr;
   } else {
      if (result == FILEIO_LOCK_FAILED) {
         *err = 0;
      } else {
         *err = FileMapErrorToErrno(__FUNCTION__, errnum);
      }
      Posix_Free(tokenPtr->pathName);
      ASSERT(!FileIO_IsValid(&tokenPtr->u.mandatory.lockFd));
      Posix_Free(tokenPtr);

      return NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockIntrinsicPortable --
 *
 *      Obtain a lock on a file; shared or exclusive access.
 *
 *      This implementation uses a HIGHLY portable directory-namespace +
 *      Lamport bakery scheme that works on all filesystems that provide
 *      atomicity of the directory namespace (That is, all known filesystems).
 *      The various files involved are hidden within a "pathName.lck/"
 *      subdirectory.
 *
 *      The lock can be broken by removing the subdirectory. The lock
 *      is self-cleaning on the same host (e.g. will detect a dead process
 *      and will break the lock), but NOT self-cleaning across hosts. The
 *      lock does not require any sort of time-based leases or heartbeats.
 *
 * Results:
 *      NULL    Lock not acquired. Check err.
 *              err     0       Lock Timed Out
 *              err     > 0     errno
 *      !NULL   Lock Acquired. This is the "lockToken" for an unlock.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static FileLockToken *
FileLockIntrinsicPortable(const char *pathName,   // IN:
                          const char *lockDir,    // IN:
                          LockValues *myValues,   // IN/OUT:
                          int *err)               // OUT:
{
   int access;
   FileIOResult result;
   FileIODescriptor desc;
   FileLockToken *tokenPtr;

   char *entryFilePath = NULL;
   char *memberFilePath = NULL;
   char *entryDirectory = NULL;

   ASSERT(pathName != NULL);
   ASSERT(err != NULL);

   /*
    * Attempt to create the locking and entry directories; obtain the
    * entry and member path names.
    */

   *err = FileLockCreateEntryDirectory(lockDir, &entryDirectory,
                                       &entryFilePath, &memberFilePath,
                                       &myValues->memberName);

   switch (*err) {
   case 0:
      break;

   case EROFS:
      /* FALL THROUGH */
   case EACCES:
      if (!myValues->exclusivity) {
         /*
          * Lock is for read/shared access however the lock directory could
          * not be created. Grant an implicit read lock whenever possible.
          * The address of a private variable will be used for the lock token.
          */

         Warning(LGPFX" %s implicit %s lock succeeded on '%s'.\n",
                 __FUNCTION__, LOCK_SHARED, pathName);

         *err = 0;
         memberFilePath = &implicitReadToken;
      }

      /* FALL THROUGH */
   default:
      goto bail;
   }

   ASSERT(Unicode_LengthInCodeUnits(memberFilePath) -
          Unicode_LengthInCodeUnits(pathName) <= FILELOCK_OVERHEAD);

   /* Attempt to create the entry file */
   access = FILEIO_OPEN_ACCESS_WRITE;

#if defined(_WIN32)
   access |= FILEIO_OPEN_SHARE_DELETE;
#else
   access |= FILEIO_OPEN_ACCESS_NOFOLLOW;
#endif

   FileIO_Invalidate(&desc);

   result = FileIOCreateRetry(&desc, entryFilePath, access,
                              FILEIO_OPEN_CREATE_SAFE, 0644,
                              FILE_MAX_WAIT_TIME_MS);

   if (!FileIO_IsSuccess(result)) {
      *err = FileMapErrorToErrno(__FUNCTION__, Err_Errno());

      /* clean up */
      FileRemoveDirectoryRobust(entryDirectory);
      FileRemoveDirectoryRobust(lockDir);

      goto bail;
   }

   /* What is max(Number[1]... Number[all lockers])? */
   *err = FileLockScanner(lockDir, FileLockNumberScan, myValues, FALSE);

   if (*err != 0) {
      /* clean up */
      FileIO_Close(&desc);
      FileDeletionRobust(entryFilePath, FALSE);
      FileRemoveDirectoryRobust(entryDirectory);
      FileRemoveDirectoryRobust(lockDir);

      goto bail;
   }

   /* Number[i] = 1 + max([Number[1]... Number[all lockers]) */
   myValues->lamportNumber++;

   /* Attempt to create the member file */
   *err = FileLockCreateMemberFile(&desc, myValues, entryFilePath,
                                   memberFilePath);

   /* Remove entry directory; it has done its job */
   if (*err == 0) {
      *err = FileRemoveDirectoryRobust(entryDirectory);
   }

   if (*err != 0) {
      /* clean up */
      FileDeletionRobust(entryFilePath, FALSE);
      FileDeletionRobust(memberFilePath, FALSE);
      FileRemoveDirectoryRobust(lockDir);

      goto bail;
   }

   /* Attempt to acquire the lock */
   *err = FileLockScanner(lockDir, FileLockWaitForPossession, myValues, TRUE);

   switch (*err) {
   case 0:
      break;

   case EAGAIN:
      /* clean up */
      FileDeletionRobust(memberFilePath, FALSE);
      FileRemoveDirectoryRobust(lockDir);

      /* FALL THROUGH */
   default:
      break;
   }

bail:

   Posix_Free(entryDirectory);
   Posix_Free(entryFilePath);

   if (*err == 0) {
      tokenPtr = Util_SafeMalloc(sizeof *tokenPtr);

      tokenPtr->signature = FILELOCK_TOKEN_SIGNATURE;
      tokenPtr->portable = TRUE;
      tokenPtr->pathName = Unicode_Duplicate(pathName);
      tokenPtr->u.portable.lockFilePath = memberFilePath;
   } else {
      Posix_Free(memberFilePath);
      tokenPtr = NULL;

      if (*err == EAGAIN) {
         *err = 0; // lock not acquired
      }
   }

   return tokenPtr;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockIntrinsic --
 *
 *      Obtain a lock on a file; shared or exclusive access.
 *
 *      All FileLock_-based locks are advisory locks (i.e. the
 *      lock is maintained separately from the file so only FileLock_
 *      callers experience locking). Advisory locks have an inherent problem
 *      that they are difficult to break in the event one of the cooperating
 *      entities fails, particularly across distributed filesystems.
 *
 *      This wrapper function will adaptively switch between a scheme
 *      implemented via mandatory locks and a more portable scheme depending
 *      on host OS support.
 *
 *      maxWaitTimeMsec specifies the maximum amount of time, in
 *      milliseconds, to wait for the lock before returning the "not
 *      acquired" status. A value of FILELOCK_TRYLOCK_WAIT is the
 *      equivalent of a "try lock" - the lock will be acquired only if
 *      there is no contention. A value of FILELOCK_INFINITE_WAIT
 *      specifies "waiting forever" to acquire the lock.
 *
 * Results:
 *      NULL    Lock not acquired. Check err.
 *              err     0       Lock Timed Out
 *              err     > 0     errno
 *      !NULL   Lock Acquired. This is the "lockToken" for an unlock.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

FileLockToken *
FileLockIntrinsic(const char *pathName,    // IN:
                  Bool exclusivity,        // IN:
                  uint32 maxWaitTimeMsec,  // IN:
                  int *err)                // OUT:
{
   char *lockBase;
   LockValues myValues = { 0 };
   FileLockToken *tokenPtr;

   /* Construct the locking directory path */
   lockBase = Unicode_Append(pathName, FILELOCK_SUFFIX);

   myValues.lockType = exclusivity ? LOCK_EXCLUSIVE : LOCK_SHARED;
   myValues.exclusivity = exclusivity;
   myValues.startTimeMsec = Hostinfo_SystemTimerMS();
   myValues.maxWaitTimeMsec = maxWaitTimeMsec;

   if (File_SupportsMandatoryLock(pathName)) {
      LOG(1, "Requesting %s lock on %s (mandatory, %u).\n",
          myValues.lockType, pathName, myValues.maxWaitTimeMsec);

      tokenPtr = FileLockIntrinsicMandatory(pathName, lockBase, &myValues, err);
   } else {
      myValues.machineID = (char *) FileLockGetMachineID(); // don't free this!
      myValues.executionID = FileLockGetExecutionID();      // free this!
      myValues.lamportNumber = 0;
      myValues.locationChecksum = FileLockLocationChecksum(lockBase); // free this!
      myValues.memberName = NULL;

      LOG(1, "Requesting %s lock on %s (%s, %s, %u).\n",
          myValues.lockType, pathName, myValues.machineID,
          myValues.executionID, myValues.maxWaitTimeMsec);

      tokenPtr = FileLockIntrinsicPortable(pathName, lockBase, &myValues, err);

      Posix_Free(myValues.memberName);
      Posix_Free(myValues.locationChecksum);
      Posix_Free(myValues.executionID);
   }

   Posix_Free(lockBase);

   return tokenPtr;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockIsLockedMandatory --
 *
 *      Is a file currently locked (at the time of the call)?
 *
 *      The only way to check for a mandatory lock is to try opening
 *      the file (and quickly closing it again). If the lock is held,
 *      attempting to open the file will return FILEIO_LOCK_FAILED.
 *
 * Results:
 *      TRUE    YES
 *      FALSE   NO; if err is not NULL may check *err for an error
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
FileLockIsLockedMandatory(const char *lockFile,  // IN:
                          int *err)              // OUT/OPT:
{
   int access;
   FileIOResult result;
   FileIODescriptor desc;

   FileIO_Invalidate(&desc);

   /*
    * Check for lock by actually locking file, and dropping
    * lock quickly if open was successful.
    */

   access = FILEIO_OPEN_ACCESS_READ | FILEIO_OPEN_ACCESS_WRITE |
            FILEIO_OPEN_EXCLUSIVE_LOCK;

   result = FileIOCreateRetry(&desc, lockFile, access, FILEIO_OPEN, 0644, 0);

   if (FileIO_IsSuccess(result)) {
      Bool success;

      success = FileIO_IsSuccess(FileIO_Close(&desc));

      ASSERT(success);
      return FALSE;
   } else if (result == FILEIO_LOCK_FAILED) {
      return TRUE;   // locked
   } else if (result == FILEIO_FILE_NOT_FOUND) {
      return FALSE;  // no lock file means unlocked
   } else {
      if (err != NULL) {
         *err = FileMapErrorToErrno(__FUNCTION__, Err_Errno());
      }

      return FALSE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockIsLockedPortable --
 *
 *      Is a file currently locked (at the time of the call)?
 *
 *      The "portable" lock is held if the lock directory exists and
 *      there are any "M" entries (representing held locks).
 *
 *      FileLocks implemented via mandatory locking are reported
 *      as held locks (errno == ENOTDIR).
 *
 * Results:
 *      TRUE    YES
 *      FALSE   NO; if err is not NULL may check *err for an error
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
FileLockIsLockedPortable(const char *lockDir,  // IN:
                         int *err)             // OUT/OPT:
{
   uint32 i;
   int numEntries;
   Bool isLocked = FALSE;
   char **fileList = NULL;

   numEntries = FileListDirectoryRobust(lockDir, &fileList);

   if (numEntries == -1) {
      /*
       * If the lock directory doesn't exist, we should not count this
       * as an error.  This is expected if the file isn't locked.
       */

      if (err != NULL) {
         *err = (errno == ENOENT) ? 0 : errno;
      }

      return FALSE;
   }

   for (i = 0; i < numEntries; i++) {
      if (*fileList[i] == 'M') {
         isLocked = TRUE;
         break;
      }
   }

   Util_FreeStringList(fileList, numEntries);

   return isLocked;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockIsLocked --
 *
 *      Is a file currently locked (at the time of the call)?
 *
 * Results:
 *      TRUE    YES
 *      FALSE   NO; if err is not NULL may check *err for an error
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
FileLockIsLocked(const char *pathName,  // IN:
                 int *err)              // OUT/OPT:
{
   Bool isLocked;
   char *lockBase;

   ASSERT(pathName != NULL);

   lockBase = Unicode_Append(pathName, FILELOCK_SUFFIX);

   if (File_SupportsMandatoryLock(pathName)) {
      isLocked = FileLockIsLockedMandatory(lockBase, err);
   } else {
      isLocked = FileLockIsLockedPortable(lockBase, err);
   }

   Posix_Free(lockBase);

   return isLocked;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLock_TokenPathName --
 *
 *      Return the path name associated with a lock (token). The path name
 *      is returned as a dynamically allocated string the caller is
 *      responsible for.
 *
 * Results:
 *      As above
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char *
FileLock_TokenPathName(const FileLockToken *lockToken)  // IN:
{
   ASSERT(lockToken && (lockToken->signature == FILELOCK_TOKEN_SIGNATURE));

   return Unicode_Duplicate(lockToken->pathName);
}
