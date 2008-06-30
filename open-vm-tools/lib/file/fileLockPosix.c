/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 *      Interface to host-specific locking function for Posix hosts.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* Needed before sys/vfs.h with glibc 2.0 --hpreg */
#if !defined(__FreeBSD__)
#if defined(__APPLE__)
#include <sys/param.h> 
#include <sys/mount.h> 
#include <sys/times.h>
#include <sys/sysctl.h>
#else
#include <sys/vfs.h>
#endif
#endif
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>

#include "vmware.h"
#include "posix.h"
#include "file.h"
#include "fileIO.h"
#include "fileLock.h"
#include "fileInt.h"
#include "util.h"
#include "str.h"
#include "vm_version.h"
#include "localconfig.h"
#include "hostinfo.h"
#include "su.h"

#include "unicodeOperations.h"

#if defined(VMX86_SERVER)
#include "hostType.h"
/*
 * UserWorlds have to handle file locking slightly differently than COS
 * applications, as they are in a different pid space.  The problem is that
 * if a UserWorld tries to write its UserWorld pid to a lock file, a COS
 * program will think the lock file is stale, as that pid won't be valid on
 * the COS. Luckily, every UW is attached to a COS proxy, so, for the
 * purposes of this file, the UW can use its COS proxy's pid as its pid when
 * locking files. This way, if a COS app looks up the proxy pid, it will be
 * valid and the COS app will wait for the lock.
 */
#include "uwvmk.h"
#endif

#define LOGLEVEL_MODULE main
#include "loglevel_user.h"

#define DEVICE_LOCK_DIR "/var/lock"

/*
 * Parameters used by the file library.
 */
typedef struct FileLockOptions {
   int lockerPid;
   Bool userWorld;
} FileLockOptions;

static FileLockOptions fileLockOptions = { 0, FALSE };

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
 * FileLock_Init --
 *
 *      Initialize the file locking library.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Allows the user to enable locking on random file systems.
 *
 *----------------------------------------------------------------------
 */

void
FileLock_Init(int lockerPid,            // IN
              Bool userWorld)           // IN
{
   fileLockOptions.lockerPid = lockerPid;
   fileLockOptions.userWorld = userWorld;
}

#if !defined(__FreeBSD__) && !defined(sun)
/*
 *----------------------------------------------------------------------
 *
 * IsLinkingAvailable --
 *
 *      Check if linking is supported in the filesystem where we create
 *      the lock file.
 *
 * Results:
 *      TRUE is we're sure it's supported.  FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
IsLinkingAvailable(const char *fileName)     // IN:
{
   struct statfs buf;
   int           status;

   ASSERT(fileName);

#if defined(VMX86_SERVER)
   // Never supported on VMvisor
   if (HostType_OSIsPureVMK()) {
      return FALSE;
   }
#endif

   status = statfs(fileName, &buf);

   if (status == -1) {
      Log(LGPFX" Bad statfs using %s (%s).\n", fileName, strerror(errno));

      return FALSE;
   }

#if defined(__APPLE__)
   if ((Str_Strcasecmp(buf.f_fstypename, "hfs") == 0) ||
       (Str_Strcasecmp(buf.f_fstypename, "nfs") == 0) ||
       (Str_Strcasecmp(buf.f_fstypename, "ufs") == 0)) {
      return TRUE;
   }

   if ((Str_Strcasecmp(buf.f_fstypename, "smbfs") != 0) &&
       (Str_Strcasecmp(buf.f_fstypename, "afpfs") != 0)) {
      Log(LGPFX" Unknown filesystem '%s'. Using non-linking file locking.\n",
          buf.f_fstypename);
   }
#else
   /* consult "table" of known filesystem types */
   switch (buf.f_type) {
   case AFFS_SUPER_MAGIC:
   case EXT_SUPER_MAGIC:
   case EXT2_OLD_SUPER_MAGIC:
   case EXT2_SUPER_MAGIC:
   // EXT3_SUPER_MAGIC is EXT2_SUPER_MAGIC
   case HFSPLUS_SUPER_MAGIC:
   case NFS_SUPER_MAGIC:
   case XENIX_SUPER_MAGIC:
   case SYSV4_SUPER_MAGIC:
   case SYSV2_SUPER_MAGIC:
   case COH_SUPER_MAGIC:
   case UFS_SUPER_MAGIC:
   case REISERFS_SUPER_MAGIC:
   case XFS_SUPER_MAGIC:
   case TMPFS_SUPER_MAGIC:
   case JFS_SUPER_MAGIC:
        return TRUE;                        // these are known to work
   case VMFS_SUPER_MAGIC:
   case SMB_SUPER_MAGIC:
   case MSDOS_SUPER_MAGIC:
        return FALSE;
   }

   /*
    * Nothing is known about this filesystem. Play it safe and use
    * non-link based locking.
    */
   Warning(LGPFX" Unknown filesystem 0x%x. Using non-linking locking.\n",
           (unsigned int) buf.f_type);
#endif

   return FALSE;
}


/*
 *---------------------------------------------------------------------------
 *
 * FileLockGetPid --
 *
 *      Returns the pid of the main thread if we're running in a
 *      multithreaded process, otherwise return the result of getpid().
 *
 * Results:
 *      a pid.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static int
FileLockGetPid(void)
{
#if defined(VMX86_SERVER)
   /*
    * For a UserWorld, we want to get this cartel's proxy's cos pid.
    */
   if (fileLockOptions.userWorld) {
      int pid = 0;

      VMKernel_GetLockPid(&pid);

      return pid;
   }
#endif

   return fileLockOptions.lockerPid > 0 ?
                         fileLockOptions.lockerPid : (int) getpid();
}


/*
 *----------------------------------------------------------------------
 *
 * RemoveStaleLockFile --
 *
 *        Remove a stale lock file.
 *
 * Results:
 *        TRUE on success.
 *
 * Side effects:
 *        Unlink file.
 *
 *----------------------------------------------------------------------
 */

static Bool
RemoveStaleLockFile(const char *lockFileName)      // IN:
{
   Bool su;
   int  ret;
   int  saveErrno;

   ASSERT(lockFileName);

   /* stale lock */
   Log(LGPFX" Found a previous instance of lock file '%s'. "
       "It will be removed automatically.\n", lockFileName);

   su = IsSuperUser();
   SuperUser(TRUE);
   ret = unlink(lockFileName);
   saveErrno = errno;
   SuperUser(su);

   if (ret < 0) {
      Warning(LGPFX" Failed to remove stale lock file %s (%s).\n",
              lockFileName, strerror(saveErrno));

      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * GetLockFileValues --
 *
 *      Get host name and PID of locking process.
 *
 * Results:
 *      1 on success, 0 if file doesn't exist, -1 for all other errors.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
GetLockFileValues(const char *lockFileName, // IN:
                  int *pid,                 // OUT:
                  char *hostID)             // OUT:
{
   char *p;
   Bool su;
   int  saveErrno;
   FILE *lockFile;
   char line[1000];
   Bool deleteLockFile;

   int status;

   ASSERT(lockFileName);
   ASSERT(pid);
   ASSERT(hostID);

   su = IsSuperUser();
   SuperUser(TRUE);
   lockFile = Posix_Fopen(lockFileName, "r");
   saveErrno = errno;
   SuperUser(su);

   if (lockFile == NULL) {
      Warning(LGPFX" Failed to open existing lock file %s (%s).\n",
              lockFileName, strerror(saveErrno));

      return (saveErrno == ENOENT) ? 0 : -1;
   }

   p = fgets(line, sizeof line, lockFile);
   saveErrno = errno;

   fclose(lockFile);

   if (p == NULL) {
      Warning(LGPFX" Failed to read line from lock file %s (%s).\n",
              lockFileName, strerror(saveErrno));

      deleteLockFile = TRUE;
   } else {
      switch (sscanf(line, "%d %999s", pid, hostID)) {
      case 2:
         // Everything is OK
         deleteLockFile = FALSE;
         break;

      case 1:
      default:
         Warning(LGPFX" Badly formatted lock file %s.\n", lockFileName);
         deleteLockFile = TRUE;
      }
   }

   status = 1;
   if (deleteLockFile) {
      status = 0;  // we're going to delete the file
      if (!RemoveStaleLockFile(lockFileName)) {
         status = -1;
      }
   }

   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * IsValidProcess --
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
IsValidProcess(int pid)       // IN:
{
   Bool su;
   int err;
   Bool ret;

#if defined(VMX86_SERVER)
   if (fileLockOptions.userWorld) {
      return (VMKernel_IsLockPidAlive(pid) == 0) ? TRUE : FALSE;
   }
#endif

   su = IsSuperUser();
   SuperUser(TRUE);

   err = (kill(pid, 0) == -1) ? errno : 0;

   SuperUser(su);

   switch (err) {
   case 0:
   case EPERM:
      ret = TRUE;
      break;
   case ESRCH:
      ret = FALSE;
      break;
   default:
      Log(LGPFX" %s Unexpected errno (%d), assuming valid.\n",
          __FUNCTION__, err);
      ret = TRUE;
      break;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * CreateLockFile --
 *
 *      Create a new lock file, either via a O_EXCL creat() call or
 *      through the linking method.
 *
 * Results:
 *      1 if we created our lock file successfully.
 *      0 if we should retry the creation.
 *     -1 if the process failed.
 *
 * Side effects:
 *      Change the host file system.
 *
 *----------------------------------------------------------------------
 */

static int
CreateLockFile(const char *lockFileName, // IN:
               const char *lockFileLink, // IN:
               const char *uniqueID)     // IN:
{
   int  err;
   int  lockFD;
   int  status = 1;
   int  saveErrno;
   Bool useLinking = IsLinkingAvailable(lockFileName);
   Bool su = IsSuperUser();

   if (useLinking) {
      SuperUser(TRUE);
      lockFD = creat(lockFileLink, 0444);
      saveErrno = errno;
      SuperUser(su);

      if (lockFD == -1) {
         Log(LGPFX" Failed to create new lock file %s (%s).\n",
             lockFileLink, strerror(saveErrno));

         return (saveErrno == EEXIST) ? 0 : -1;
      }
   } else {
      /*
       * XXX
       * Note that this option is racy, at least for SMB and FAT32 file
       * systems. It appears, however, that by using a temporary lock
       * file before getting the real, persistent lock file, the race
       * can be eliminated. -- johnh
       */

      SuperUser(TRUE);
      lockFD = Posix_Open(lockFileName, O_CREAT | O_EXCL | O_WRONLY,
                                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      saveErrno = errno;
      SuperUser(su);

      if (lockFD == -1) {
         Log(LGPFX" Failed to create new lock file %s (%s).\n",
             lockFileName, strerror(saveErrno));

         return (saveErrno == EEXIST) ? 0 : -1;
      }
   }

   err = write(lockFD, uniqueID, strlen(uniqueID));
   saveErrno = errno;

   close(lockFD);

   if (err != strlen(uniqueID)) {
      Warning(LGPFX" Failed to write to new lock file %s (%s).\n",
              lockFileName, strerror(saveErrno));
      status = -1;
      goto exit;
   }

   SuperUser(TRUE);

   if (useLinking && (link(lockFileLink, lockFileName) < 0)) {
      status = (errno == EEXIST) ? 0 : -1;
   }

   SuperUser(su);

exit:
   if (useLinking) {
      SuperUser(TRUE);
      err = unlink(lockFileLink);
      SuperUser(su);

      if (err < 0) {
         Warning(LGPFX" Failed to remove temporary lock file %s (%s).\n",
                 lockFileLink, strerror(errno));
      }

   }
   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLock_LockDevice --
 *
 *      Lock with a file.  Detect and remove stale locks
 *      when possible.
 *
 * Results:
 *      1 if got the lock, 0 if not, -1 for errors.
 *
 * Side effects:
 *      Change the host file system.
 *
 * Note:
 *      This locking method remains due to "minicom" and similar
 *      programs that use this locking method for access serialization
 *      of serial ports.
 *
 *----------------------------------------------------------------------
 */

int
FileLock_LockDevice(const char *deviceName)   // IN:
{
   const char *hostID;
   char       uniqueID[1000];
   char       *lockFileName;
   char       *lockFileLink;

   int  status = -1;

   ASSERT(deviceName);

   lockFileName = Str_SafeAsprintf(NULL, "%s/LCK..%s", DEVICE_LOCK_DIR,
                                   deviceName);

   lockFileLink = Str_SafeAsprintf(NULL, "%s/LTMP..%s.t%05d", DEVICE_LOCK_DIR,
                                   deviceName, FileLockGetPid());

   LOG(1, ("Requesting lock %s (temp = %s).\n", lockFileName,
       lockFileLink));

   hostID = FileLockGetMachineID();
   Str_Sprintf(uniqueID, sizeof uniqueID, "%d %s\n",
               FileLockGetPid(), hostID);

   while ((status = CreateLockFile(lockFileName, lockFileLink,
                                   uniqueID)) == 0) {
      int  pid;
      char fileID[1000];

      /*
       *  The lock file already exists. See if it is a stale lock.
       *
       *  We retry the link if the file is gone now (0 return).
       */

      switch (GetLockFileValues(lockFileName, &pid, fileID)) {
      case 1:
         break;
      case 0:
         continue;
      case -1:
         status = -1;
         goto exit;
      default:
         NOT_REACHED();
      }

      if (strcmp(hostID, fileID) != 0) {
         // Lock was acquired by a different host.
         status = 0;
         goto exit;
      }

      if (IsValidProcess(pid)) {
         status = 0;
         goto exit;
      }

      // stale lock
     if (!RemoveStaleLockFile(lockFileName)) {
         status = -1;
         goto exit;
      }
      /* TRY AGAIN */
   }

exit:
   free(lockFileName);
   free(lockFileLink);
   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLock_UnlockDevice --
 *
 *      Unlock a lock obtained by FileLock_LockDevice.
 *
 * Results:
 *      True if successful, FALSE otherwise.
 *
 * Side effects:
 *      Change the host file system.
 *
 *----------------------------------------------------------------------
 */

Bool
FileLock_UnlockDevice(const char *deviceName)  // IN:
{
   Bool su;
   int  ret;
   int  saveErrno;
   char *path;

   ASSERT(deviceName);

   path = Str_SafeAsprintf(NULL, "%s/LCK..%s", DEVICE_LOCK_DIR, deviceName);

   LOG(1, ("Releasing lock %s.\n", path));

   su = IsSuperUser();
   SuperUser(TRUE);
   ret = unlink(path);
   saveErrno = errno;
   SuperUser(su);

   if (ret < 0) {
      Log(LGPFX" Cannot remove lock file %s (%s).\n",
          path, strerror(saveErrno));
      free(path);

      return FALSE;
   }

   free(path);

   return TRUE;
}

#if defined(linux)
/*
 *----------------------------------------------------------------------
 *
 * ReadSlashProc --
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
ReadSlashProc(const char *procPath, // IN:
              char *buffer,         // OUT:
              size_t bufferSize)    // IN:
{
   int fd;
   int err;
   char *p;
   size_t len;

   ASSERT(procPath);
   ASSERT(buffer);
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
 *----------------------------------------------------------------------
 *
 * ProcessCreationTime --
 *
 *      Return the specified process's creation time.
 *
 * Results:
 *      The process's creation time is returned. If an error occurs the
 *      reported creation time will be 0.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static uint64
ProcessCreationTime(pid_t pid)
{
   int    err;
   char   path[64];
   char   buffer[1024];
   uint64 creationTime;

   Str_Sprintf(path, sizeof path, "/proc/%d/stat", pid);

   err = ReadSlashProc(path, buffer, sizeof buffer);

   if (err == 0) {
      uint32 i;
      char   *p;
      char   *last;

      /*
       * You are in a maze of twisty little fields, (virtually) all alike...
       *
       * The process creation time, in 64-bit jiffies is "out there".
       *
       * A "man 5 proc" will provide illumination concerning all of the
       * fields found on this line of text. We code for the worst case
       * and insure that file names containing spaces or parens are
       * properly handled.
       */

      p = strrchr(buffer, ')');
      ASSERT(p != NULL);
      p = strtok_r(++p, " ", &last);
      ASSERT(p != NULL);

      for (i = 0; i < 19; i++) {
         p = strtok_r(NULL, " ", &last);
         ASSERT(p != NULL);
      }

      if (sscanf(p, "%"FMT64"u", &creationTime) != 1) {
         creationTime = 0;
      }
   } else {
      creationTime = 0;
   }

   return creationTime;
}
#elif defined(__APPLE__)
static uint64
ProcessCreationTime(pid_t pid)
{
    int                err;
    size_t             size;
    struct kinfo_proc  info;
    int                mib[4];
  
    // Request information about the specified process
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = pid;

    memset(&info, 0, sizeof info);
    size = sizeof(info);
    err = sysctl(mib, ARRAYSIZE(mib), &info, &size, NULL, 0);

    // Log any failures
    if (err == -1) {
       Warning(LGPFX" %s sysctl for pid %d failed: %s\n", __FUNCTION__,
               pid, strerror(errno));

       return 0;
    }

    // Return the process creation time
    return (info.kp_proc.p_starttime.tv_sec * CONST64U(1000000)) +
            info.kp_proc.p_starttime.tv_usec;
}
#else
static uint64
ProcessCreationTime(pid_t pid)
{
   return 0;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * FileLockValidOwner --
 *
 *      Validate the lock file owner.
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
FileLockValidOwner(const char *executionID, // IN:
                   const char *payload)     // IN:
{
   int pid;

   // Validate the PID.
   if (sscanf(executionID, "%d", &pid) != 1) {
      Warning(LGPFX" %s pid conversion error on %s. Assuming valid.\n",
              __FUNCTION__, executionID);

      return TRUE;
   }

   if (!IsValidProcess(pid)) {
      return FALSE;
   }

   // If there is a payload perform additional validation.
   if ((payload != NULL) && (strncmp(payload, "pc=", 3) == 0)) {
      uint64 fileCreationTime;
      uint64 processCreationTime;

      /*
       * The payload is the process creation time of the process that
       * created the lock file.
       */

      if (sscanf(payload + 3, "%"FMT64"u", &fileCreationTime) != 1) {
         Warning(LGPFX" %s payload conversion error on %s. Assuming valid.\n",
                 __FUNCTION__, payload);

         return TRUE;
      }

      // Non-matching process creation times -> pid is not the creator
      processCreationTime = ProcessCreationTime(pid);

      if ((fileCreationTime != 0) &&
          (processCreationTime != 0) &&
          (fileCreationTime != processCreationTime)) {
         return FALSE;
      }
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 *  FileLockOpenFile --
 *
 *      Open the specified file
 *
 * Results:
 *      0       success
 *      > 0     failure (errno)
 *
 * Side effects:
 *      May change the host file system.
 *
 *----------------------------------------------------------------------
 */

int
FileLockOpenFile(ConstUnicode pathName,        // IN:
                 int flags,                    // IN:
                 FILELOCK_FILE_HANDLE *handle) // OUT:
{
   ASSERT(pathName);

   *handle = PosixFileOpener(pathName, flags, 0644);

   return *handle == -1 ? errno : 0;
}


/*
 *----------------------------------------------------------------------
 *
 *  FileLockCloseFile --
 *
 *      Close the specified file
 *
 * Results:
 *      0       success
 *      > 0     failure (errno)
 *
 * Side effects:
 *      May change the host file system.
 *
 *----------------------------------------------------------------------
 */

int
FileLockCloseFile(FILELOCK_FILE_HANDLE handle) // IN:
{
   return close(handle) == -1 ? errno : 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockReadFile --
 *
 *      Read a file.
 *
 * Results:
 *      0       success
 *      > 0     failure (errno)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
FileLockReadFile(FILELOCK_FILE_HANDLE handle,  // IN:
                 void *buf,                    // IN:
                 uint32 requestedBytes,        // IN:
                 uint32 *resultantBytes)       // OUT:
{
   int err;
   ssize_t result;

   result = read(handle, buf, requestedBytes);

   if (result == -1) {
      *resultantBytes = 0;
      err = errno;
   } else {
      *resultantBytes = result;
      err = 0;
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockWriteFile --
 *
 *      Write a file.
 *
 * Results:
 *      0       success
 *      > 0     failure (errno)
 *
 * Side effects:
 *      May change the host file system.
 *
 *-----------------------------------------------------------------------------
 */

int
FileLockWriteFile(FILELOCK_FILE_HANDLE handle,  // IN:
                  void *buf,                    // IN:
                  uint32 requestedBytes,        // IN:
                  uint32 *resultantBytes)       // OUT:
{
   int err;
   ssize_t result;

   result = write(handle, buf, requestedBytes);

   if (result == -1) {
      *resultantBytes = 0;
      err = errno;
   } else {
      *resultantBytes = result;
      err = 0;
   }

   return err;
}


/*
 *---------------------------------------------------------------------------
 *
 * FileLockGetExecutionID --
 *
 *      Returns the executionID of the caller.
 *
 * Results:
 *      The executionID of the caller.
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
   return Str_SafeAsprintf(NULL, "%d", FileLockGetPid());
}


/*
 *----------------------------------------------------------------------
 *
 * FileLock_Lock --
 *
 *      Obtain a lock on a file; shared or exclusive access. Also specify
 *      how long to wait on lock acquisition - msecMaxWaitTime
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
 *              err     !0      errno
 *      !NULL   Lock Acquired. This is the "lockToken" for an unlock.
 *
 * Side effects:
 *      Changes the host file system.
 *
 *----------------------------------------------------------------------
 */

void *
FileLock_Lock(ConstUnicode filePath,        // IN:
              const Bool readOnly,          // IN:
              const uint32 msecMaxWaitTime, // IN:
              int *err)                     // OUT:
{
   Unicode fullPath;
   void *lockToken;
   char creationTimeString[32];

   ASSERT(filePath);
   ASSERT(err);

   fullPath = File_FullPath(filePath);
   if (fullPath == NULL) {
      *err = EINVAL;
      return NULL;
   }

   Str_Sprintf(creationTimeString, sizeof creationTimeString, "%"FMT64"u",
               ProcessCreationTime(FileLockGetPid()));

   lockToken = FileLockIntrinsic(fullPath, !readOnly, msecMaxWaitTime,
                                 creationTimeString, err);

   Unicode_Free(fullPath);

   return lockToken;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLock_Unlock --
 *
 *      Release the lock held on the specified file.
 *
 * Results:
 *      0       unlocked
 *      >0      errno
 *
 * Side effects:
 *      Changes the host file system.
 *
 *----------------------------------------------------------------------
 */

int
FileLock_Unlock(ConstUnicode filePath,  // IN:
                const void *lockToken)  // IN:
{
   int err;
   Unicode fullPath;

   ASSERT(filePath);
   ASSERT(lockToken);

   fullPath = File_FullPath(filePath);
   if (fullPath == NULL) {
      return EINVAL;
   }

   err = FileUnlockIntrinsic(fullPath, lockToken);

   Unicode_Free(fullPath);

   return err;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLock_DeleteFileVMX --
 *
 *      The VMX file delete primitive.
 *
 * Results:
 *      0       unlocked
 *      >0      errno
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
FileLock_DeleteFileVMX(ConstUnicode filePath)  // IN:
{
   int err;
   Unicode fullPath;

   ASSERT(filePath);

   fullPath = File_FullPath(filePath);
   if (fullPath == NULL) {
      return EINVAL;
   }

   err = FileLockHackVMX(fullPath);

   Unicode_Free(fullPath);

   return err;
}

#endif /* !__FreeBSD__ && !sun */
