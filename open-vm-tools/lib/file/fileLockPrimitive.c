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
 * fileLockPrimitive.c --
 *
 *      Portable file locking via Lamport's Bakery algorithm.
 *
 * This implementation does rely upon a remove directory operation to fail
 * if the directory contains any files.
 */

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
#include "file.h"
#include "fileLock.h"
#include "fileInt.h"
#include "random.h"
#include "vm_atomic.h"

#include "unicodeOperations.h"

#define LOGLEVEL_MODULE main
#include "loglevel_user.h"

#define LOCK_SHARED     "S"
#define LOCK_EXCLUSIVE  "X"
#define FILELOCK_PROGRESS_DEARTH 8000 // Dearth of progress time in msec
#define FILELOCK_PROGRESS_SAMPLE 200  // Progress sampling time in msec

static char implicitReadToken;

#define PARSE_TABLE_UINT   0
#define PARSE_TABLE_STRING 1

typedef struct parse_table
{
   int type;
   char *name;
   void *valuePtr;
} ParseTable;


/*
 *-----------------------------------------------------------------------------
 *
 * Sleeper --
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
Sleeper(LockValues *myValues, // IN/OUT:
        uint32 *loopCount)    // IN/OUT:
{
   uint32 msecSleepTime;

   if ((myValues->msecMaxWaitTime == FILELOCK_TRYLOCK_WAIT) ||
       ((myValues->msecMaxWaitTime != FILELOCK_INFINITE_WAIT) &&
        (myValues->waitTime > myValues->msecMaxWaitTime))) {
      return EAGAIN;
   }

   if (*loopCount <= 20) {
      /* most locks are "short" */
      msecSleepTime = 100;
      *loopCount += 1;
   } else if (*loopCount < 40) {
      /* lock has been around a while, linear back-off */
      msecSleepTime = 100 * (*loopCount - 19);
      *loopCount += 1;
   } else {
      /* WOW! long time... Set a maximum */
      msecSleepTime = 2000;
   }

   myValues->waitTime += msecSleepTime;

   while (msecSleepTime) {
      uint32 sleepTime = (msecSleepTime > 900) ? 900 : msecSleepTime;

      usleep(1000 * sleepTime);

      msecSleepTime -= sleepTime;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RemoveLockingFile --
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
RemoveLockingFile(ConstUnicode lockDir,   // IN:
                  ConstUnicode fileName)  // IN:
{
   int err;
   Unicode path;

   ASSERT(lockDir);
   ASSERT(fileName);

   path = Unicode_Join(lockDir, DIRSEPS, fileName, NULL);

   err = FileDeletionRobust(path, FALSE);

   if (err != 0) {
      if (err == ENOENT) {
         /* Not there anymore; locker unlocked or timed out */
         err = 0;
      } else {
         Warning(LGPFX" %s of '%s' failed: %s\n", __FUNCTION__,
                 UTF8(path), strerror(err));
      }
   }

   Unicode_Free(path);

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
FileLockMemberValues(ConstUnicode lockDir,     // IN:
                     ConstUnicode fileName,    // IN:
                     char *buffer,             // OUT:
                     uint32 requiredSize,      // IN:
                     LockValues *memberValues) // OUT:
{
   uint32 argc = 0;
   FILELOCK_FILE_HANDLE handle;
   uint32 len;
   char *argv[FL_MAX_ARGS];
   int err;
   Unicode path;
   FileData fileData;

   ParseTable table = { PARSE_TABLE_STRING,
                        "lc",
                        (void *) &memberValues->locationChecksum
                      };
 
   ASSERT(lockDir);
   ASSERT(fileName);

   path = Unicode_Join(lockDir, DIRSEPS, fileName, NULL);

   err = FileLockOpenFile(path, O_RDONLY, &handle);

   if (err != 0) {
      /*
       * A member file may "disappear" if is deleted due to an unlock
       * immediately after a directory scan but before the scan is processed.
       * Since this is a "normal" thing ENOENT will be suppressed.
       */

      if (err != ENOENT) {
         Warning(LGPFX" %s open failure on '%s': %s\n", __FUNCTION__,
                 UTF8(path), strerror(err));
      }

      goto bail;
   }

   /* Attempt to obtain the lock file attributes now that it is opened */
   err = FileAttributesRobust(path, &fileData);

   if (err != 0) {
      Warning(LGPFX" %s file size failure on '%s': %s\n", __FUNCTION__,
              UTF8(path), strerror(err));

      FileLockCloseFile(handle);

      goto bail;
   }

   /* Complain if the lock file is not the proper size */
   if (fileData.fileSize != requiredSize) {
      Warning(LGPFX" %s file '%s': size %"FMT64"u, required size %u\n",
              __FUNCTION__, UTF8(path), fileData.fileSize, requiredSize);

      FileLockCloseFile(handle);

      goto corrupt;
   }

   /* Attempt to read the lock file data and validate how much was read. */
   err = FileLockReadFile(handle, buffer, requiredSize, &len);

   FileLockCloseFile(handle);

   if (err != 0) {
      Warning(LGPFX" %s read failure on '%s': %s\n",
              __FUNCTION__, UTF8(path), strerror(err));

      goto bail;
   }

   if (len != requiredSize) {
      Warning(LGPFX" %s read length issue on '%s': %u and %u\n",
              __FUNCTION__, UTF8(path), len, requiredSize);

      err = EIO;
      goto bail;
   }

   /* Extract and validate the lock file data. */
   for (argc = 0; argc < FL_MAX_ARGS; argc++) {
      argv[argc] = strtok((argc == 0) ? buffer : NULL, " ");

      if (argv[argc] == NULL) {
         break;
      }
   }

   if ((argc < 4) || ((argc == FL_MAX_ARGS) && (strtok(NULL, " ") != NULL))) {
      goto corrupt;
   }

   /*
    * Lock file arguments are space separated. There is a minimum of 4
    * arguments - machineID, executionID, Lamport number and lock type.
    * The maximum number of arguments is FL_MAX_ARGS.
    *
    * The fifth argument, if present, is the payload or "[" if there is no
    * payload and additional arguments are present. The additional arguments
    * form  a properly list - one or more "name=value" pairs.
    *
    * Here is picture of valid forms:
    *
    * 0 1 2 3 4 5 6   Comment
    *-------------------------
    * A B C D         contents, no payload, no list entries
    * A B C D [       contents, no payload, no list entries
    * A B C D P       contents, a payload,  no list entries
    * A B C D [ x     contents, no payload, one list entry
    * A B C D P x     contents, a payload,  one list entry
    * A B C D [ x y   contents, no payload, two list entries
    * A B C D P x y   contents, a payload,  two list entries
    */

   memberValues->locationChecksum = NULL;

   if (argc == 4) {
      memberValues->payload = NULL;
   } else {
      if (strcmp(argv[4], "[") == 0) {
         memberValues->payload = NULL;
      } else {
         memberValues->payload = argv[4];
      }

      if (FileLockParseArgs(argv, argc - 5, &table, 1)) {
         goto corrupt;
      }
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

   Unicode_Free(path);

   return 0;

corrupt:
   Warning(LGPFX" %s removing problematic lock file '%s'\n", __FUNCTION__,
           UTF8(path));

   if (argc) {
      Log(LGPFX" %s '%s' contents are:\n", __FUNCTION__, UTF8(fileName));

      for (len = 0; len < argc; len++) {
         Log(LGPFX" %s %s argv[%d]: '%s'\n", __FUNCTION__, UTF8(fileName),
             len, argv[len]);
      }
   }

   /* Remove the lock file and behave like it has disappeared */
   err = FileDeletionRobust(path, FALSE);

   if (err == 0) {
      err = ENOENT;
   }

bail:
   Unicode_Free(path);

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
FileLockValidName(ConstUnicode fileName) // IN:
{
   uint32 i;

   ASSERT(fileName);

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
 * ActivateLockList
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
ActivateLockList(ConstUnicode dirName,  // IN:
                 LockValues *myValues)  // IN:
{
   ActiveLock   *ptr;

   ASSERT(dirName);

   ASSERT(Unicode_StartsWith(dirName, "D"));

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
FileLockLocationChecksum(ConstUnicode path)  // IN:
{
   int c;
   uint32 hash = 5381;

#if defined(_WIN32)
   char *p;
   Unicode value = Unicode_Duplicate(path);

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
   Unicode_Free(value);
#endif

   return Str_SafeAsprintf(NULL, "%u", hash);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ScanDirectory --
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
ScanDirectory(ConstUnicode lockDir,     // IN:
              int (*func)(              // IN:
                     ConstUnicode lockDir,
                     ConstUnicode fileName,
                     LockValues *memberValues,
                     LockValues *myValues
                   ),
              LockValues *myValues,    // IN:
              Bool cleanUp)            // IN:
{
   uint32 i;
   int err;
   int numEntries;

   Unicode *fileList = NULL;
   char *myExecutionID = NULL;
   char *locationChecksum = NULL;

   ASSERT(lockDir);

   numEntries = FileListDirectoryRobust(lockDir, &fileList);

   if (numEntries == -1) {
      Log(LGPFX" %s: Could not read the directory '%s': %d\n",
          __FUNCTION__, UTF8(lockDir), Err_Errno());

      return EDOM;  // out of my domain
   }

   /* Pass 1: Validate entries and handle any 'D' entries */
   for (i = 0, err = 0; i < numEntries; i++) {
      /* Remove any non-locking files */
      if (!FileLockValidName(fileList[i])) {
         Log(LGPFX" %s discarding %s from %s'; invalid file name.\n",
             __FUNCTION__, UTF8(fileList[i]), UTF8(lockDir));

         err = RemoveLockingFile(lockDir, fileList[i]);
         if (err != 0) {
            goto bail;
         }

        Unicode_Free(fileList[i]);
        fileList[i] = NULL;

        continue;
      }

      /*
       * Any lockers appear to be entering?
       *
       * This should be rather rare. If a locker dies while entering
       * this will cleaned-up.
       */

      if (Unicode_StartsWith(fileList[i], "D")) {
         if (cleanUp) {
            err = ActivateLockList(fileList[i], myValues);
            if (err != 0) {
               goto bail;
            }
        }

        Unicode_Free(fileList[i]);
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
      char       buffer[FILELOCK_DATA_SIZE];

      if ((fileList[i] == NULL) ||
          (Unicode_StartsWith(fileList[i], "E"))) {
         continue;
      }

      myLockFile = (Unicode_Compare(fileList[i],
                          myValues->memberName) == 0) ? TRUE : FALSE;

      if (myLockFile) {
         /* It's me! No need to read or validate anything. */
         ptr = myValues;
      } else {
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

            if (FileLockValidOwner(memberValues.executionID,
                                   memberValues.payload)) {
               /* If it's mine it better still be where I put it! */
               if ((strcmp(myExecutionID, memberValues.executionID) == 0) &&
                   ((memberValues.locationChecksum != NULL) &&
                    (strcmp(memberValues.locationChecksum,
                            locationChecksum) != 0))) {
                  dispose = "lock file has been moved.";
               }
            } else {
               dispose = "invalid executionID.";
            }

            if (dispose) {
               Log(LGPFX" %s discarding %s from %s': %s\n",
                   __FUNCTION__, UTF8(fileList[i]), UTF8(lockDir), dispose);

               Unicode_Free(memberValues.memberName);

               err = RemoveLockingFile(lockDir, fileList[i]);
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
         Unicode_Free(memberValues.memberName);
      }

      if (err != 0) {
         break;
      }
   }

bail:

   for (i = 0; i < numEntries; i++) {
      Unicode_Free(fileList[i]);
   }

   free(fileList);
   free(locationChecksum);
   free(myExecutionID);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scanner --
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
Scanner(ConstUnicode lockDir,    // IN:
        int (*func)(             // IN:
               ConstUnicode lockDir,
               ConstUnicode fileName,
               LockValues *memberValues,
               LockValues *myValues
            ),
        LockValues *myValues,    // IN:
        Bool cleanUp)            // IN:
{
   int        err;
   ActiveLock *ptr;

   ASSERT(lockDir);

   myValues->lockList = NULL;

   while (TRUE) {
      ActiveLock *prev;

      err = ScanDirectory(lockDir, func, myValues, cleanUp);
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
               Unicode temp;
               Unicode path;
               UnicodeIndex index;

               ASSERT(Unicode_StartsWith(ptr->dirName, "D"));

               Log(LGPFX" %s discarding %s data from '%s'.\n",
                   __FUNCTION__, UTF8(ptr->dirName), UTF8(lockDir));

               path = Unicode_Join(lockDir, DIRSEPS, ptr->dirName, NULL);

               index = Unicode_FindLast(path, "D");
               ASSERT(index != UNICODE_INDEX_NOT_FOUND);

               temp = Unicode_Replace(path, index, 1, "M");
               FileDeletionRobust(temp, FALSE);
               Unicode_Free(temp);

               temp = Unicode_Replace(path, index, 1, "E");
               FileDeletionRobust(temp, FALSE);
               Unicode_Free(temp);

               FileRemoveDirectoryRobust(path);

               Unicode_Free(path);

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
            if (prev == NULL) {
               myValues->lockList = ptr->next;
            } else {
               prev->next = ptr->next;
            }
         }

         prev = ptr;
         ptr = ptr->next;
      }

      usleep(FILELOCK_PROGRESS_SAMPLE * 1000); // relax
   }

   // Clean up anything still on the list; they are no longer important
   while (myValues->lockList != NULL) {
      ptr = myValues->lockList;
      myValues->lockList = ptr->next;

      Unicode_Free(ptr->dirName);

      free(ptr);
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
FileUnlockIntrinsic(ConstUnicode pathName,  // IN:
                    const void *lockToken)  // IN:
{
   int err;

   ASSERT(pathName);
   ASSERT(lockToken);

   LOG(1, ("Requesting unlock on %s\n", UTF8(pathName)));

   if (lockToken == &implicitReadToken) {
      /*
       * The lock token is the fixed-address implicit read lock token.
       * Since no lock file was created no further action is required.
       */

      err = 0;
   } else {
      Unicode lockDir;

      /* The lock directory path */
      lockDir = Unicode_Append(pathName, FILELOCK_SUFFIX);

      /*
       * The lock token is the (unicode) path of the lock file.
       *
       * TODO: under vmx86_debug validate the contents of the lock file as
       *       matching the machineID and executionID.
       */

      err = FileDeletionRobust((Unicode) lockToken, FALSE);

      if (err && vmx86_debug) {
         Log(LGPFX" %s failed for '%s': %s\n",
             __FUNCTION__, (char *) lockToken, strerror(err));
      }

      /*
       * The lockToken (a unicode path) was allocated in FileLockIntrinsic
       * and returned to the caller hidden behind a "void *" pointer.
       */

      Unicode_Free((Unicode) lockToken);

      FileRemoveDirectoryRobust(lockDir); // just in case we can clean up

      Unicode_Free(lockDir);
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * WaitForPossession --
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
WaitForPossession(ConstUnicode lockDir,     // IN:
                  ConstUnicode fileName,    // IN:
                  LockValues *memberValues, // IN:
                  LockValues *myValues)     // IN:
{
   int err = 0;

   ASSERT(lockDir);
   ASSERT(fileName);

   /* "Win" or wait? */
   if (((memberValues->lamportNumber < myValues->lamportNumber) ||
       ((memberValues->lamportNumber == myValues->lamportNumber) &&
          (Unicode_Compare(memberValues->memberName,
                           myValues->memberName) < 0))) &&
        ((strcmp(memberValues->lockType, LOCK_EXCLUSIVE) == 0) ||
         (strcmp(myValues->lockType, LOCK_EXCLUSIVE) == 0))) {
      Unicode path;
      uint32 loopCount;
      Bool   thisMachine; 

      thisMachine = FileLockMachineIDMatch(myValues->machineID,
                                           memberValues->machineID);

      loopCount = 0;

      path = Unicode_Join(lockDir, DIRSEPS, fileName, NULL);

      while ((err = Sleeper(myValues, &loopCount)) == 0) {
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
         if (thisMachine && !FileLockValidOwner(memberValues->executionID,
                                                memberValues->payload)) {
            /* Invalid Execution ID; remove the member file */
            Warning(LGPFX" %s discarding file '%s'; invalid executionID.\n",
                    __FUNCTION__, UTF8(path));

            err = RemoveLockingFile(lockDir, fileName);
            break;
         }
      }

      /*
       * Log the disposition of each timeout for all non "try lock" locking
       * attempts. This can assist in debugging locking problems.
       */

      if ((myValues->msecMaxWaitTime != FILELOCK_TRYLOCK_WAIT) &&
          (err == EAGAIN)) {
         if (thisMachine) {
            Log(LGPFX" %s timeout on '%s' due to a local process (%s)\n",
                    __FUNCTION__, UTF8(path), memberValues->executionID);
         } else {
            Log(LGPFX" %s timeout on '%s' due to another machine (%s)\n",
                    __FUNCTION__, UTF8(path), memberValues->machineID);
         }
      }

      Unicode_Free(path);
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * NumberScan --
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
NumberScan(ConstUnicode lockDir,      // IN:
           ConstUnicode fileName,     // IN:
           LockValues *memberValues,  // IN:
           LockValues *myValues)      // IN/OUT:
{
   ASSERT(lockDir);
   ASSERT(fileName);

   if (memberValues->lamportNumber > myValues->lamportNumber) {
      myValues->lamportNumber = memberValues->lamportNumber;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SimpleRandomNumber --
 *
 *      Return a random number in the range of 0 and 2^16-1.
 *
 * Results:
 *      Random number is returned.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static uint32
SimpleRandomNumber(const char *machineID,   // IN:
                   const char *executionID) // IN:
{
   static Atomic_Ptr atomic; /* Implicitly initialized to NULL. --mbellon */
   char *context;

   context = Atomic_ReadPtr(&atomic);

   if (context == NULL) {
      void *p;
      uint32 value = 0;

      /*
       * Use the machineID and executionID to hopefully start each machine
       * and process/thread at a different place in the answer stream.
       */

      while (*machineID) {
         value += *machineID++;
      }

      while (*executionID) {
         value += *executionID++;
      }

      p = Random_QuickSeed(value);

      if (Atomic_ReadIfEqualWritePtr(&atomic, NULL, p)) {
         free(p);
      }

      context = Atomic_ReadPtr(&atomic);
      ASSERT(context);
   }

   return (Random_Quick(context) >> 8) & 0xFFFF;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MakeDirectory --
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
MakeDirectory(ConstUnicode pathName)  // IN:
{
   int err;

#if !defined(_WIN32)
   mode_t save;

   ASSERT(pathName);

   save = umask(0);
#else
   ASSERT(pathName);
#endif

   err = FileCreateDirectoryRobust(pathName);

#if !defined(_WIN32)
   umask(save);
#endif

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CreateEntryDirectory --
 *
 *      Create an entry directory in the specified locking directory.
 *
 *      Due to FileLock_UnlockFile() attempting to remove the locking
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
CreateEntryDirectory(const char *machineID,    // IN:
                     const char *executionID,  // IN:
                     ConstUnicode lockDir,     // IN:
                     Unicode *entryDirectory,  // OUT:
                     Unicode *entryFilePath,   // OUT:
                     Unicode *memberFilePath,  // OUT:
                     Unicode *memberName)      // OUT:
{
   int err = 0;
   uint32 randomNumber = 0;

   ASSERT(lockDir);

   *entryDirectory = NULL;
   *entryFilePath = NULL;
   *memberFilePath = NULL;
   *memberName = NULL;

   /* Fun at the races */

   while (TRUE) {
      Unicode temp;
      FileData fileData;

      err = FileAttributesRobust(lockDir, &fileData);
      if (err == 0) {
        /* The name exists. Deal with it... */

        if (fileData.fileType == FILE_TYPE_REGULAR) {
           /*
            * It's a file. Assume this is an (active?) old style lock
            * and err on the safe side - don't remove it (and
            * automatically upgrade to a new style lock).
            */

            Log(LGPFX" %s: '%s' exists; an old style lock file?\n",
                      __FUNCTION__, UTF8(lockDir));

            err = EAGAIN;
            break;
        }

        if (fileData.fileType != FILE_TYPE_DIRECTORY) {
           /* Not a directory; attempt to remove the debris */
           if (FileDeletionRobust(lockDir, FALSE) != 0) {
              Warning(LGPFX" %s: '%s' exists and is not a directory.\n",
                      __FUNCTION__, UTF8(lockDir));

              err = ENOTDIR;
              break;
           }

           continue;
        }
      } else {
         if (err == ENOENT) {
            /* Not there anymore; locker unlocked or timed out */
            err = MakeDirectory(lockDir);

            if ((err != 0) && (err != EEXIST)) {
               Warning(LGPFX" %s creation failure on '%s': %s\n",
                       __FUNCTION__, UTF8(lockDir), strerror(err));

               break;
            }
         } else {
            Warning(LGPFX" %s stat failure on '%s': %s\n",
                    __FUNCTION__, UTF8(lockDir), strerror(err));

            break;
         }
      }

      /* There is a small chance of collision/failure; grab stings now */
      randomNumber = SimpleRandomNumber(machineID, executionID);

      *memberName = Unicode_Format("M%05u%s", randomNumber, FILELOCK_SUFFIX);

      temp = Unicode_Format("D%05u%s", randomNumber, FILELOCK_SUFFIX);
      *entryDirectory = Unicode_Join(lockDir, DIRSEPS, temp, NULL);
      Unicode_Free(temp);

      temp = Unicode_Format("E%05u%s", randomNumber, FILELOCK_SUFFIX);
      *entryFilePath = Unicode_Join(lockDir, DIRSEPS, temp, NULL);
      Unicode_Free(temp);

      *memberFilePath = Unicode_Join(lockDir, DIRSEPS, *memberName, NULL);

      err = MakeDirectory(*entryDirectory);

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
                   __FUNCTION__, UTF8(*memberFilePath), strerror(err));
             }
         }

         FileRemoveDirectoryRobust(*entryDirectory);
      } else {
          if ((err != EEXIST) &&  // Another process/thread created it...
              (err != ENOENT)) {  // lockDir is gone...
             Warning(LGPFX" %s creation failure on '%s': %s\n",
                     __FUNCTION__, UTF8(*entryDirectory), strerror(err));

             break;
          }
      }

      Unicode_Free(*entryDirectory);
      Unicode_Free(*entryFilePath);
      Unicode_Free(*memberFilePath);
      Unicode_Free(*memberName);

      *entryDirectory = NULL;
      *entryFilePath = NULL;
      *memberFilePath = NULL;
      *memberName = NULL;
   }

   if (err != 0) {
      Unicode_Free(*entryDirectory);
      Unicode_Free(*entryFilePath);
      Unicode_Free(*memberFilePath);
      Unicode_Free(*memberName);

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
 * CreateMemberFile --
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
CreateMemberFile(FILELOCK_FILE_HANDLE entryHandle,  // IN:
                 const LockValues *myValues,        // IN:
                 ConstUnicode entryFilePath,        // IN:
                 ConstUnicode memberFilePath)       // IN:
{
   int err;
   uint32 len;
   char buffer[FILELOCK_DATA_SIZE] = { 0 };

   ASSERT(entryFilePath);
   ASSERT(memberFilePath);

   /*
    * Populate the buffer with appropriate data
    *
    * Lock file arguments are space separated. There is a minimum of 4
    * arguments - machineID, executionID, Lamport number and lock type.
    * The maximum number of argument is FL_MAX_ARGS.
    *
    * The fifth argument, if present, is the payload or "[" if there is no
    * payload and additional arguments are present. The additional arguments
    * form  a properly list - one or more "name=value" pairs.
    */

   Str_Sprintf(buffer, sizeof buffer, "%s %s %u %s %s lc=%s",
               myValues->machineID,
               myValues->executionID,
               myValues->lamportNumber,
               myValues->lockType,
               myValues->payload == NULL ? "[" : myValues->payload,
               myValues->locationChecksum);

   /* Attempt to write the data */
   err = FileLockWriteFile(entryHandle, buffer, sizeof buffer, &len);

   if (err != 0) {
      Warning(LGPFX" %s write of '%s' failed: %s\n", __FUNCTION__,
              UTF8(entryFilePath), strerror(err));

      FileLockCloseFile(entryHandle);

      return err;
   }

   err = FileLockCloseFile(entryHandle);

   if (err != 0) {
      Warning(LGPFX" %s close of '%s' failed: %s\n", __FUNCTION__,
              UTF8(entryFilePath), strerror(err));

      return err;
   }

   if (len != sizeof buffer) {
      Warning(LGPFX" %s write length issue on '%s': %u and %"FMTSZ"d\n",
              __FUNCTION__, UTF8(entryFilePath), len, sizeof buffer);

      return EIO;
   }

   err = FileRename(entryFilePath, memberFilePath);

   if (err != 0) {
      Warning(LGPFX" %s FileRename of '%s' to '%s' failed: %s\n",
              __FUNCTION__, UTF8(entryFilePath), UTF8(memberFilePath),
              strerror(err));

      if (vmx86_debug) {
         Log(LGPFX" %s FileLockFileType() of '%s': %s\n",
             __FUNCTION__, UTF8(entryFilePath),
            strerror(FileAttributesRobust(entryFilePath, NULL)));

         Log(LGPFX" %s FileLockFileType() of '%s': %s\n",
             __FUNCTION__, UTF8(memberFilePath),
            strerror(FileAttributesRobust(memberFilePath, NULL)));
      }

      return err;
   }

   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * FileLockIntrinsic --
 *
 *      Obtain a lock on a file; shared or exclusive access.
 *
 *      msecMaxWaitTime specifies the maximum amount of time, in
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

void *
FileLockIntrinsic(ConstUnicode pathName,   // IN:
                  Bool exclusivity,        // IN:
                  uint32 msecMaxWaitTime,  // IN:
                  const char *payload,     // IN:
                  int *err)                // OUT:
{
   FILELOCK_FILE_HANDLE handle;
   LockValues myValues;

   Unicode lockDir = NULL;
   Unicode entryFilePath = NULL;
   Unicode memberFilePath = NULL;
   Unicode entryDirectory = NULL;

   ASSERT(pathName);
   ASSERT(err);

   /* Construct the locking directory path */
   lockDir = Unicode_Append(pathName, FILELOCK_SUFFIX);

   /* establish our values */

   myValues.machineID = (char *) FileLockGetMachineID(); // don't free this!
   myValues.executionID = FileLockGetExecutionID();      // free this!
   myValues.payload = (char *) payload;
   myValues.lockType = exclusivity ? LOCK_EXCLUSIVE : LOCK_SHARED;
   myValues.lamportNumber = 0;
   myValues.locationChecksum = FileLockLocationChecksum(lockDir); // free this!
   myValues.waitTime = 0;
   myValues.msecMaxWaitTime = msecMaxWaitTime;
   myValues.memberName = NULL;

   LOG(1, ("Requesting %s lock on %s (%s, %s, %u).\n",
       myValues.lockType, UTF8(pathName), myValues.machineID,
       myValues.executionID, myValues.msecMaxWaitTime));

   /*
    * Attempt to create the locking and entry directories; obtain the
    * entry and member path names.
    */

   *err = CreateEntryDirectory(myValues.machineID, myValues.executionID,
                               lockDir,
                               &entryDirectory, &entryFilePath,
                               &memberFilePath, &myValues.memberName);

   switch (*err) {
   case 0:
      break;

   case EROFS:
      /* FALL THROUGH */
   case EACCES:
      if (!exclusivity) {
         /*
          * Lock is for read/shared access however the lock directory could
          * not be created. Grant an implicit read lock whenever possible.
          * The address of a private variable will be used for the lock token.
          */

         Warning(LGPFX" %s implicit %s lock succeeded on '%s'.\n",
                 __FUNCTION__, LOCK_SHARED, UTF8(pathName));

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
   *err = FileLockOpenFile(entryFilePath, O_CREAT | O_WRONLY, &handle);

   if (*err != 0) {
      /* clean up */
      FileRemoveDirectoryRobust(entryDirectory);
      FileRemoveDirectoryRobust(lockDir);

      goto bail;
   }

   /* what is max(Number[1]... Number[all lockers])? */
   *err = Scanner(lockDir, NumberScan, &myValues, FALSE);

   if (*err != 0) {
      /* clean up */
      FileLockCloseFile(handle);
      FileDeletionRobust(entryFilePath, FALSE);
      FileRemoveDirectoryRobust(entryDirectory);
      FileRemoveDirectoryRobust(lockDir);

      goto bail;
   }

   /* Number[i] = 1 + max([Number[1]... Number[all lockers]) */
   myValues.lamportNumber++;

   /* Attempt to create the member file */
   *err = CreateMemberFile(handle, &myValues, entryFilePath, memberFilePath);

   /* Remove entry directory; it has done its job */
   FileRemoveDirectoryRobust(entryDirectory);

   if (*err != 0) {
      /* clean up */
      FileDeletionRobust(entryFilePath, FALSE);
      FileDeletionRobust(memberFilePath, FALSE);
      FileRemoveDirectoryRobust(lockDir);

      goto bail;
   }

   /* Attempt to acquire the lock */
   *err = Scanner(lockDir, WaitForPossession, &myValues, TRUE);

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

   Unicode_Free(lockDir);
   Unicode_Free(entryDirectory);
   Unicode_Free(entryFilePath);
   Unicode_Free(myValues.memberName);
   free(myValues.locationChecksum);
   free(myValues.executionID);

   if (*err != 0) {
      Unicode_Free(memberFilePath);
      memberFilePath = NULL;

      if (*err == EAGAIN) {
         *err = 0; // lock not acquired
      }
   }

   return (void *) memberFilePath;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ScannerVMX --
 *
 *      VMX hack scanner
 *
 * Results:
 *      0       success
 *      > 0     error (errno)
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
ScannerVMX(ConstUnicode lockDir,     // IN:
           ConstUnicode fileName,    // IN:
           LockValues *memberValues, // IN:
           LockValues *myValues)     // IN/OUT:
{
   ASSERT(lockDir);
   ASSERT(fileName);

   myValues->lamportNumber++;

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLockHackVMX --
 *
 *      The VMX file delete primitive.
 *
 * Results:
 *      0       unlocked
 *      > 0     errno
 *
 * Side effects:
 *      Changes the host file system.
 *
 * Note:
 *      THIS IS A HORRIBLE HACK AND NEEDS TO BE REMOVED ASAP!!!
 *
 *----------------------------------------------------------------------
 */

int
FileLockHackVMX(ConstUnicode pathName)  // IN:
{
   int err;
   LockValues myValues;

   Unicode lockDir = NULL;
   Unicode entryFilePath = NULL;
   Unicode memberFilePath = NULL;
   Unicode entryDirectory = NULL;

   ASSERT(pathName);

   /* first the locking directory path name */
   lockDir = Unicode_Append(pathName, FILELOCK_SUFFIX);

   /* establish our values */
   myValues.machineID = (char *) FileLockGetMachineID(); // don't free this!
   myValues.executionID = FileLockGetExecutionID();      // free this!
   myValues.locationChecksum = FileLockLocationChecksum(lockDir); // free this!
   myValues.lamportNumber = 0;
   myValues.memberName = NULL;

   LOG(1, ("%s on %s (%s, %s).\n", __FUNCTION__, UTF8(pathName),
       myValues.machineID, myValues.executionID));

   err = CreateEntryDirectory(myValues.machineID, myValues.executionID,
                              lockDir,
                              &entryDirectory, &entryFilePath,
                              &memberFilePath, &myValues.memberName);

   if (err != 0) {
      goto bail;
   }

   /* Scan the lock directory */
   err = Scanner(lockDir, ScannerVMX, &myValues, FALSE);

   if (err == 0) {
      /* if no members are valid, clean up */
      if (myValues.lamportNumber == 1) {
         FileDeletionRobust(pathName, FALSE);
      }
   } else {
      if (vmx86_debug) {
         Warning(LGPFX" %s clean-up failure for '%s': %s\n",
                 __FUNCTION__, UTF8(pathName), strerror(err));
      }
   }

   /* clean up */
   FileRemoveDirectoryRobust(entryDirectory);
   FileRemoveDirectoryRobust(lockDir);

bail:

   Unicode_Free(lockDir);
   Unicode_Free(entryDirectory);
   Unicode_Free(entryFilePath);
   Unicode_Free(memberFilePath);
   Unicode_Free(myValues.memberName);
   free(myValues.locationChecksum);
   free(myValues.executionID);

   return err;
}
