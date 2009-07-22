/*********************************************************
 * Copyright (C) 1998-2008 VMware, Inc. All rights reserved.
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
 * hgfsServerLinux.c --
 *
 *      This file contains the linux implementation of the server half
 *      of the Host/Guest File System (hgfs), a.k.a. "Shared Folder".
 *
 *      The hgfs server carries out filesystem requests that it receives
 *      over the backdoor from a driver in the other world.
 */

#define _GNU_SOURCE // for O_NOFOLLOW

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>  // for utimes(2)
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>

#if defined(__FreeBSD__)
#   include <sys/param.h>
#else
#   include <wchar.h>
#   include <wctype.h>
#endif

#include "vmware.h"
#include "hgfsServerPolicy.h" // for security policy
#include "hgfsServerInt.h"
#include "hgfsEscape.h"
#include "str.h"
#include "cpNameLite.h"
#include "hgfsUtil.h"  // for cross-platform time conversion
#include "posix.h"
#include "file.h"
#include "util.h"
#include "syncMutex.h"
#include "su.h"
#include "codeset.h"
#include "unicodeOperations.h"

#if defined(linux) && !defined(SYS_getdents64)
/* For DT_UNKNOWN */
#   include <dirent.h>
#endif

#ifndef VMX86_TOOLS
#   include "config.h"
#endif

#define LOGLEVEL_MODULE hgfs
#include "loglevel_user.h"

#if defined(__APPLE__)
#include <CoreServices/CoreServices.h> // for the alias manager
#include <CoreFoundation/CoreFoundation.h> // for CFString and CFURL
#include <sys/attr.h>       // for getattrlist
#include <sys/vnode.h>      // for VERG / VDIR
#endif

/*
 * ALLPERMS (mode 07777) and ACCESSPERMS (mode 0777) are not defined in the
 * Solaris version of <sys/stat.h>.
 */
#ifdef sun
#   define ACCESSPERMS (S_IRWXU|S_IRWXG|S_IRWXO)
#   define ALLPERMS (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)
#endif

#ifdef HGFS_OPLOCKS
#   include <signal.h>
#   include "sig.h"
#endif

#if defined(linux) && !defined(GLIBC_VERSION_21)
/*
 * Implements the system call (it is not wrapped by glibc 2.1.1)
 */
static INLINE int
_llseek(unsigned int fd,
        unsigned long offset_high,
        unsigned long offset_low,
        loff_t *result,
        unsigned int whence)
{
   return syscall(SYS__llseek, fd, offset_high, offset_low, result, whence);
}
#endif

/*
 * On Linux, we must wrap getdents64, as glibc does not wrap it for us. We use getdents64
 * (rather than getdents) because with the latter, we'll get 64-bit offsets and inode
 * numbers. Note that getdents64 isn't supported everywhere, in particular, kernels older
 * than 2.4.1 do not implement it. On those older guests, we try using getdents(), and
 * translating the results to our DirectoryEntry structure...
 *
 * On FreeBSD and Mac platforms, getdents is implemented as getdirentries, and takes an
 * additional parameter which returns the position of the block read, which we don't care
 * about.
 */
#if defined(linux)
static INLINE int
getdents_linux(unsigned int fd,
               DirectoryEntry *dirp,
               unsigned int count)
{
#   if defined(SYS_getdents64)
   return syscall(SYS_getdents64, fd, dirp, count);
#   else
   /*
    * Fall back to regular getdents on older Linux systems that don't have
    * getdents64. Because glibc does translation between the kernel's "struct dirent" and
    * the libc "struct dirent", this structure matches the one in linux/dirent.h, rather
    * than us using the libc 'struct dirent' directly
    */
   struct linux_dirent {
      long d_ino;
      long d_off; /* __kernel_off_t expands to long on RHL 6.2 */
      unsigned short d_reclen;
      char d_name[NAME_MAX];
   } *dirp_temp;
   int retval;

   dirp_temp = alloca((sizeof *dirp_temp) * count);

   retval = syscall(SYS_getdents, fd, dirp_temp, count);

   if (retval > 0) {
      int i;

      /*
       * Translate from the Linux 'struct dirent' to the hgfs DirectoryEntry, since
       * they're not always the same layout.
       */
      for (i = 0; i < count; i++) {
         dirp[i].d_ino = dirp_temp[i].d_ino;
         dirp[i].d_off = dirp_temp[i].d_off;
         dirp[i].d_reclen = dirp_temp[i].d_reclen;
         dirp[i].d_type = DT_UNKNOWN;
         memcpy(dirp[i].d_name, dirp_temp[i].d_name,
                ((sizeof dirp->d_name) < (sizeof dirp_temp->d_name))
                ? (sizeof dirp->d_name)
                : (sizeof dirp_temp->d_name));
      }
   }

   return retval;
#   endif
}
#      define getdents getdents_linux
#elif defined(__FreeBSD__) || defined(__APPLE__)
#define getdents(fd, dirp, count)                                             \
({                                                                            \
   long basep;                                                                \
   getdirentries(fd, dirp, count, &basep);                                    \
})
#endif

/*
 * O_DIRECTORY is only relevant on Linux. For other platforms, we'll hope that
 * the kernel is smart enough to deny getdents(2) (or getdirentries(2)) on
 * files which aren't directories.
 *
 * Likewise, O_NOFOLLOW doesn't exist on Solaris 9. Oh well.
 */
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif


#if defined(sun) || defined(linux) || \
    (defined(__FreeBSD_version) && __FreeBSD_version < 490000)
/*
 * Implements futimes(), which was introduced in glibc 2.3.3. FreeBSD 3.2
 * doesn't have it, but 4.9 does. Unfortunately, these early FreeBSD versions
 * don't have /proc/self, so futimes() will simply fail. For now the only
 * caller to futimes() is HgfsServerSetattr which doesn't get invoked at all
 * in the HGFS server which runs in the Tools, so it's OK.
 */
#define PROC_SELF_FD "/proc/self/fd/"
#define STRLEN_OF_MAXINT_AS_STRING 10
int
futimes(int fd, const struct timeval times[2])
{
#ifndef sun
   /*
    * Hack to allow the timevals in futimes() to be const even when utimes()
    * expects non-const timevals. This is the case on glibc up to 2.3 or
    * thereabouts.
    */
   struct timeval mytimes[2];

   /* Maximum size of "/proc/self/fd/<fd>" as a string. Accounts for nul. */
   char nameBuffer[sizeof PROC_SELF_FD + STRLEN_OF_MAXINT_AS_STRING];

   mytimes[0] = times[0];
   mytimes[1] = times[1];
   if (snprintf(nameBuffer, sizeof nameBuffer, PROC_SELF_FD "%d", fd) < 0) {
      return -1;
   }
   return Posix_Utimes(nameBuffer, mytimes);
#else /* !sun */
   return futimesat(fd, NULL, times);
#endif /* !sun */
}
#undef PROC_SELF_FD
#undef STRLEN_OF_MAXINT_AS_STRING
#endif

#if defined(__APPLE__)
struct FInfoAttrBuf {
   uint32 length;
   fsobj_type_t objType;
   char finderInfo[32];
};
#endif
/*
 * Server open flags, indexed by HgfsOpenFlags. Stolen from
 * lib/fileIOPosix.c
 *
 * Using O_NOFOLLOW will allow us to forgo a (racy) symlink check just
 * before opening the file.
 *
 * Using O_NONBLOCK will prevent us from blocking the HGFS server if
 * we open a FIFO.
 */
static const int HgfsServerOpenFlags[] = {
   O_NONBLOCK | O_NOFOLLOW,
   O_NONBLOCK | O_NOFOLLOW | O_TRUNC,
   O_NONBLOCK | O_NOFOLLOW | O_CREAT,
   O_NONBLOCK | O_NOFOLLOW | O_CREAT | O_EXCL,
   O_NONBLOCK | O_NOFOLLOW | O_CREAT | O_TRUNC,
};


/*
 * Server open mode, indexed by HgfsOpenMode.
 */
static const int HgfsServerOpenMode[] = {
   O_RDONLY,
   O_WRONLY,
   O_RDWR,
};

/* Local functions. */
static HgfsInternalStatus HgfsConvertFromNameStatus(HgfsNameStatus status);

static HgfsInternalStatus HgfsGetattrResolveAlias(char const *fileName,
                                                  char **targetName);

static HgfsInternalStatus HgfsGetattrFromName(char *fileName,
                                              HgfsShareOptions configOptions,
                                              char *shareName,
                                              HgfsFileAttrInfo *attr,
                                              char **targetName);

static HgfsInternalStatus HgfsGetattrFromFd(int fd,
                                            HgfsSessionInfo *session,
                                            HgfsFileAttrInfo *attr);

static void HgfsStatToFileAttr(struct stat *stats,
                               uint64 *creationTime,
                               HgfsFileAttrInfo *attr);
static int HgfsStat(const char* fileName,
                    Bool followLink,
                    struct stat *stats,
                    uint64 *creationTime);
static int HgfsFStat(int fd,
                     struct stat *stats,
                     uint64 *creationTime);

static int HgfsConvertComponentCase(char *currentComponent,
                                    const char *dirPath,
                                    const char **convertedComponent,
                                    size_t *convertedComponentSize);

static int HgfsConstructConvertedPath(char **path,
                                      size_t *pathSize,
                                      char *convertedPath,
                                      size_t convertedPathSize);

static int HgfsCaseInsensitiveLookup(const char *sharePath,
                                     size_t sharePathLength,
                                     char *fileName,
                                     size_t fileNameLength,
                                     char **convertedFileName,
                                     size_t *convertedFileNameLength);

static Bool HgfsSetattrMode(struct stat *statBuf,
                            HgfsFileAttrInfo *attr,
                            mode_t *newPermissions);

static Bool HgfsSetattrOwnership(HgfsFileAttrInfo *attr,
                                 uid_t *newUid,
                                 gid_t *newGid);

static HgfsInternalStatus HgfsSetattrTimes(struct stat *statBuf,
                                           HgfsFileAttrInfo *attr,
                                           HgfsAttrHint hints,
                                           struct timeval *accessTime,
                                           struct timeval *modTime,
                                           Bool *timesChanged);

static HgfsInternalStatus HgfsSetattrFromFd(HgfsHandle file,
                                            HgfsSessionInfo *session,
                                            HgfsFileAttrInfo *attr,
                                            HgfsAttrHint hints);

static HgfsInternalStatus HgfsSetattrFromName(char *cpName,
                                              size_t cpNameSize,
                                              HgfsFileAttrInfo *attr,
                                              HgfsAttrHint hints,
                                              uint32 caseFlags,
                                              HgfsSessionInfo *session);

static Bool HgfsIsShareRoot(char const *cpName, size_t cpNameSize);
static HgfsInternalStatus HgfsGetHiddenXAttr(char const *fileName, Bool *attribute);
static HgfsInternalStatus HgfsSetHiddenXAttr(char const *fileName, Bool value);
static HgfsInternalStatus HgfsEffectivePermissions(char *fileName,
                                                   Bool readOnlyShare,
                                                   uint32 *permissions);
#if defined(__APPLE__)
static void HgfsConvertStat64ToStat(const struct stat64 *stats64,
                                    struct stat *stats,
                                    uint64 *creationTime);
#else
static uint64 HgfsGetCreationTime(const struct stat *stats);
#endif

#ifdef HGFS_OPLOCKS
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSigOplockBreak --
 *
 *      Handle a pending oplock break. Called from the VMX poll loop context.
 *      All we really do is set up the state for an oplock break and call
 *      HgfsServerOplockBreak which will do the rest of the work.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerSigOplockBreak(int sigNum,       // IN: Signal number
                         siginfo_t *info,  // IN: Additional info about signal
                         ucontext_t *u,    // IN: Interrupted context (regs etc)
                         void *clientData) // IN: Ignored
{
   ServerLockData *lockData;
   int newLease, fd;
   HgfsServerLock newServerLock;

   ASSERT(sigNum == SIGIO);
   ASSERT(info);
   ASSERT(clientData == NULL);

   fd = info->si_fd;
   LOG(4, ("%s: Received SIGIO for fd %d\n", __FUNCTION__, fd));

   /*
    * We've got all we need from the signal handler, let it continue handling
    * signals of this type.
    */

   Sig_Continue(sigNum);

   /*
    * According to locks.c in kernel source, doing F_GETLEASE when a lease
    * break is pending will return the new lease we should use. It'll be
    * F_RDLCK if we can downgrade, or F_UNLCK if we should break altogether.
    */

   newLease = fcntl(fd, F_GETLEASE);
   if (newLease == F_RDLCK) {
      newServerLock = HGFS_LOCK_SHARED;
   } else if (newLease == F_UNLCK) {
      newServerLock = HGFS_LOCK_NONE;
   } else if (newLease == -1) {
      int error = errno;
      Log("%s: Could not get old lease for fd %d: %s\n", __FUNCTION__,
          fd, strerror(error));
      goto error;
   } else {
      Log("%s: Unexpected reply to get lease for fd %d: %d\n",
          __FUNCTION__, fd, newLease);
      goto error;
   }

   /*
    * Setup a ServerLockData struct so that we can make use of
    * HgfsServerOplockBreak which does the heavy lifting of discovering which
    * HGFS handle we're interested in breaking, sending the break, receiving
    * the acknowledgement, and firing the platform-specific acknowledgement
    * function (where we'll downgrade the lease).
    */

   lockData = malloc(sizeof *lockData);
   if (lockData) {
      lockData->fileDesc = fd;
      lockData->serverLock = newServerLock;
      lockData->event = 0; // not needed

      /*
       * Relinquish control of this data. It'll get freed later, when the RPC
       * command completes.
       */
      HgfsServerOplockBreak(lockData);
      return;
   } else {
      Log("%s: Could not allocate memory for lease break on behalf of fd %d\n",
          __FUNCTION__, fd);
   }

  error:
   /* Clean up as best we can. */
   fcntl(fd, F_SETLEASE, F_UNLCK);
   HgfsUpdateNodeServerLock(fd, HGFS_LOCK_NONE);
}
#endif /* HGFS_OPLOCKS */


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsConvertFromNameStatus --
 *
 *    This function converts between a status code used in processing a cross
 *    platform filename, and a platform-specific status code.
 *
 *    Because the two status codes never go down the wire, there is no danger
 *    of backwards compatibility here, and we should ASSERT if we encounter
 *    an status code that we're not familiar with.
 *
 * Results:
 *    Converted status code.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsConvertFromNameStatus(HgfsNameStatus status) // IN
{
   switch(status) {
   case HGFS_NAME_STATUS_COMPLETE:
      return 0;
   case HGFS_NAME_STATUS_FAILURE:
   case HGFS_NAME_STATUS_INCOMPLETE_BASE:
   case HGFS_NAME_STATUS_INCOMPLETE_ROOT:
   case HGFS_NAME_STATUS_INCOMPLETE_DRIVE:
   case HGFS_NAME_STATUS_INCOMPLETE_UNC:
   case HGFS_NAME_STATUS_INCOMPLETE_UNC_MACH:
      return EINVAL;
   case HGFS_NAME_STATUS_DOES_NOT_EXIST:
      return ENOENT;
   case HGFS_NAME_STATUS_ACCESS_DENIED:
      return EACCES;
   case HGFS_NAME_STATUS_SYMBOLIC_LINK:
      return ELOOP;
   case HGFS_NAME_STATUS_OUT_OF_MEMORY:
      return ENOMEM;
   case HGFS_NAME_STATUS_TOO_LONG:
      return ENAMETOOLONG;
   case HGFS_NAME_STATUS_NOT_A_DIRECTORY:
      return ENOTDIR;
   default:
      NOT_IMPLEMENTED();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetDefaultDirAttrs --
 *
 *    Get default directory attributes. Permissions are Read and
 *    Execute permission only.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerGetDefaultDirAttrs(HgfsFileAttrInfo *attr) // OUT
{
   struct timeval tv;
   uint64 hgfsTime;

   ASSERT(attr);

   attr->type = HGFS_FILE_TYPE_DIRECTORY;
   attr->size = 4192;

   /*
    * Linux and friends are OK with receiving timestamps of 0, but just
    * for consistency with the Windows server, we'll pass back the
    * host's time in a virtual directory's timestamps.
    */
   if (gettimeofday(&tv, NULL) != 0) {
      hgfsTime = 0;
   } else {
      hgfsTime = HgfsConvertToNtTime(tv.tv_sec, tv.tv_usec * 1000);
   }
   attr->creationTime = hgfsTime;
   attr->accessTime = hgfsTime;
   attr->writeTime = hgfsTime;
   attr->attrChangeTime = hgfsTime;
   attr->specialPerms = 0;
   attr->ownerPerms = HGFS_PERM_READ | HGFS_PERM_EXEC;
   attr->groupPerms = HGFS_PERM_READ | HGFS_PERM_EXEC;
   attr->otherPerms = HGFS_PERM_READ | HGFS_PERM_EXEC;

   attr->mask = HGFS_ATTR_VALID_TYPE |
      HGFS_ATTR_VALID_SIZE |
      HGFS_ATTR_VALID_CREATE_TIME |
      HGFS_ATTR_VALID_ACCESS_TIME |
      HGFS_ATTR_VALID_WRITE_TIME |
      HGFS_ATTR_VALID_CHANGE_TIME |
      HGFS_ATTR_VALID_SPECIAL_PERMS |
      HGFS_ATTR_VALID_OWNER_PERMS |
      HGFS_ATTR_VALID_GROUP_PERMS |
      HGFS_ATTR_VALID_OTHER_PERMS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetOpenFlags --
 *
 *      Retrieve system open flags from HgfsOpenFlags.
 *
 *      Does the correct bounds checking on the HgfsOpenFlags before
 *      indexing into the array of flags to use. See bug 54429.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsServerGetOpenFlags(HgfsOpenFlags flagsIn, // IN
                       int *flagsOut)         // OUT
{
   unsigned int arraySize;

   ASSERT(flagsOut);

   arraySize = ARRAYSIZE(HgfsServerOpenFlags);

   if (flagsIn < 0 || flagsIn >= arraySize) {
      Log("%s: Invalid HgfsOpenFlags %d\n", __FUNCTION__, flagsIn);

      return FALSE;
   }

   *flagsOut = HgfsServerOpenFlags[flagsIn];

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPlatformInit --
 *
 *      Set up any state needed to start Linux HGFS server.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerPlatformInit(void)
{
#ifdef HGFS_OPLOCKS
   /* Register a signal handler to catch oplock break signals. */
   Sig_Callback(SIGIO, SIG_SAFE, HgfsServerSigOplockBreak, NULL);
#endif
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPlatformDestroy --
 *
 *      Tear down any state used for Linux HGFS server.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsServerPlatformDestroy(void)
{
#ifdef HGFS_OPLOCKS
   /* Tear down oplock state, so we no longer catch signals. */
   Sig_Callback(SIGIO, SIG_NOHANDLER, NULL, NULL);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetOpenMode --
 *
 *      Retrieve system open mode from HgfsOpenMode.
 *
 *      Does the correct bounds checking on the HgfsOpenMode before
 *      indexing into the array of modes to use. See bug 54429.
 *
 *      This is just the POSIX implementation; the Windows implementation is
 *      more complicated, hence the need for the HgfsFileOpenInfo as an
 *      argument.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerGetOpenMode(HgfsFileOpenInfo *openInfo, // IN:  Open info to examine
                      uint32 *modeOut)            // OUT: Local mode
{
   ASSERT(modeOut);

   /*
    * If we didn't get the mode in the open request, we'll return a mode of 0.
    * This has the effect of failing the call to open(2) later, which is
    * exactly what we want.
    */
   if ((openInfo->mask & HGFS_OPEN_VALID_MODE) == 0) {
      *modeOut = 0;
      return TRUE;
   }

   if (!HGFS_OPEN_MODE_IS_VALID_MODE(openInfo->mode)) {
      Log("%s: Invalid HgfsOpenMode %d\n", __FUNCTION__, openInfo->mode);

      return FALSE;
   }

   *modeOut = HgfsServerOpenMode[HGFS_OPEN_MODE_ACCMODE(openInfo->mode)];

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCloseFile --
 *
 *    Closes the file descriptor.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsCloseFile(fileDesc fileDesc) // IN: File descriptor
{
   if (close(fileDesc) != 0) {
      int error = errno;

      LOG(4, ("%s: Could not close fd %d: %s\n", __FUNCTION__, fileDesc, 
              strerror(error)));

      return error;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCheckFileNode --
 *
 *    Check if a file node is still valid (i.e. if the file name stored in the
 *    file node still refers to the same file)
 *
 * Results:
 *    Zero if the file node is valid
 *    Non-zero if the file node is stale
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsCheckFileNode(char const *localName,      // IN
                  HgfsLocalId const *localId) // IN
{
   struct stat nodeStat;

   ASSERT(localName);
   ASSERT(localId);

   /*
    * A file is uniquely identified by a (device; inode) pair. Check that the
    * file name still refers to the same pair
    */

   if (Posix_Stat(localName, &nodeStat) < 0) {
      int error = errno;

      LOG(4, ("%s: couldn't stat local file \"%s\": %s\n", __FUNCTION__,
              localName, strerror(error)));

      return error;
   }

   if (nodeStat.st_dev != localId->volumeId ||
       nodeStat.st_ino != localId->fileId) {
      LOG(4, ("%s: local Id mismatch\n", __FUNCTION__));

      return ENOENT;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetFd --
 *
 *    Returns the file descriptor associated with the node. If the node is
 *    cached then it just returns the cached file descriptor (checking for
 *    correct write flags). Otherwise, it opens a new file, caches the node
 *    and returns the file desriptor.
 *
 * Results:
 *    Zero on success. fd contains the opened file descriptor.
 *    Non-zero on error.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsGetFd(HgfsHandle hgfsHandle,    // IN:  HGFS file handle
          HgfsSessionInfo *session, // IN:  Session info
          Bool append,              // IN:  Open with append flag
          fileDesc *fd)             // OUT: Opened file descriptor
{
   int newFd = -1, openFlags = 0;
   HgfsFileNode node;
   HgfsInternalStatus status = 0;

   ASSERT(fd);
   ASSERT(session);

   /*
    * Use node copy convenience function to get the node information.
    * Note that we shouldn't keep this node around for too long because
    * the information can become stale. However, it's ok to get all the
    * fields in one step, instead of getting them all separate.
    *
    * XXX: It would be better if we didn't do this node copy on the fast
    * path. Unfortuntely, even the fast path may need to look at the node's
    * append flag.
    */

   node.utf8Name = NULL;
   if (!HgfsGetNodeCopy(hgfsHandle, session, TRUE, &node)) {
      /* XXX: Technically, this can also fail if we're out of memory. */
      LOG(4, ("%s: Invalid hgfs handle.\n", __FUNCTION__));
      status = EBADF;
      goto exit;
   }

   /* If the node is found in the cache */
   if (HgfsIsCached(hgfsHandle, session)) {
      /*
       * If the append flag is set check to see if the file was opened
       * in append mode. If not, close the file and reopen it in append
       * mode.
       */

      if (append && !(node.flags & HGFS_FILE_NODE_APPEND_FL)) {
         status = HgfsCloseFile(node.fileDesc);
         if (status != 0) {
            LOG(4, ("%s: Couldn't close file \"%s\" for reopening\n",
                    __FUNCTION__, node.utf8Name));
            goto exit;
         }

         /*
          * Update the node in the cache with the new value of the append
          * flag.
          */

         if (!HgfsUpdateNodeAppendFlag(hgfsHandle, session, TRUE)) {
            LOG(4, ("%s: Could not update the node in the cache\n",
                    __FUNCTION__));
            status = EBADF;
            goto exit;
         }
      } else {
         newFd = node.fileDesc;
         goto exit;
      }
   }

   /*
    * If we got here then the file was either not in the cache or needs
    * reopening. This means we need to open a file. But first, verify
    * that the file we intend to open isn't stale.
    */

   status = HgfsCheckFileNode(node.utf8Name, &node.localId);
   if (status != 0) {
      goto exit;
   }

   /*
    * We're not interested in creating a new file. So let's just get the
    * flags for a simple open request. This really should always work.
    */

   HgfsServerGetOpenFlags(0, &openFlags);

   /*
    * We don't need to specify open permissions here because we're only
    * reopening an existing file, not creating a new one.
    *
    * XXX: We should use O_LARGEFILE, see lib/file/fileIOPosix.c --hpreg
    */

   newFd = Posix_Open(node.utf8Name,
		node.mode | openFlags | (append ? O_APPEND : 0));

   if (newFd < 0) {
      int error = errno;

      LOG(4, ("%s: Couldn't open file \"%s\": %s\n", __FUNCTION__,
              node.utf8Name, strerror(errno)));
      status = error;
      goto exit;
   }

   /*
    * Update the original node with the new value of the file desc.
    * This call might fail if the node is not used anymore.
    */

   if (!HgfsUpdateNodeFileDesc(hgfsHandle, session, newFd)) {
      LOG(4, ("%s: Could not update the node -- node is not used.\n",
              __FUNCTION__));
      status = EBADF;
      goto exit;
   }

   /* Add the node to the cache. */
   if (!HgfsAddToCache(hgfsHandle, session)) {
      LOG(4, ("%s: Could not add node to the cache\n", __FUNCTION__));
      status = EBADF;
      goto exit;
   }

  exit:
   if (status == 0) {
      *fd = newFd;
   }
   free(node.utf8Name);

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsValidateOpen --
 *
 *    Verify that the file with the given local name exists in the
 *    local filesystem by trying to open the file with the requested
 *    mode and permissions. If the open succeeds we stat the file
 *    and fill in the volumeId and fileId with the file's local
 *    filesystem device and inode number, respectively.
 *
 * Results:
 *    Zero on success
 *    Non-zero on failure.
 *
 * Side effects:
 *    File with name "localName" may be created or truncated.
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsValidateOpen(HgfsFileOpenInfo *openInfo, // IN: Open info struct
                 int followSymlinks,         // IN: followSymlinks config option
                 HgfsSessionInfo *session,   // IN: session info
                 HgfsLocalId *localId,       // OUT: Local unique file ID
                 int *fileDesc)              // OUT: Handle to the file
{
   struct stat fileStat;
   int fd;
   int error;
   int openMode = 0, openFlags = 0;
   mode_t openPerms;
   HgfsServerLock serverLock;
   HgfsInternalStatus status = 0;
   Bool needToSetAttribute = FALSE;

   ASSERT(openInfo);
   ASSERT(localId);
   ASSERT(fileDesc);
   ASSERT(session);

   /*
    * Get correct system flags and mode from the HgfsOpenFlags and
    * HgfsOpenMode. This is related to bug 54429.
    */
   if (!HgfsServerGetOpenFlags(openInfo->mask & HGFS_OPEN_VALID_FLAGS ?
                                 openInfo->flags : 0,
                               &openFlags) ||
       !HgfsServerGetOpenMode(openInfo, &openMode)) {
      status = EPROTO;
      goto exit;
   }

   /*
    * Create mode_t for use in open(). If owner permissions are missing, use
    * read/write for the owner permissions. If group or other permissions
    * are missing, use the owner permissions.
    *
    * This sort of makes sense. If the Windows driver wants to make a file
    * read-only, it probably intended for the file to be 555. Since creating
    * a file requires a valid mode, it's highly unlikely that we'll ever
    * be creating a file without owner permissions.
    */
   openPerms = ~ALLPERMS;
   openPerms |= openInfo->mask & HGFS_OPEN_VALID_SPECIAL_PERMS ?
                  openInfo->specialPerms << 9 : 0;
   openPerms |= openInfo->mask & HGFS_OPEN_VALID_OWNER_PERMS ?
                  openInfo->ownerPerms << 6 : S_IWUSR | S_IRUSR;
   openPerms |= openInfo->mask & HGFS_OPEN_VALID_GROUP_PERMS ?
                  openInfo->groupPerms << 3 : (openPerms & S_IRWXU) >> 3;
   openPerms |= openInfo->mask & HGFS_OPEN_VALID_OTHER_PERMS ?
                  openInfo->otherPerms : (openPerms & S_IRWXU) >> 6;

   /*
    * By default we don't follow symlinks, O_NOFOLLOW is always set.
    * Unset it if followSymlinks config option is specified.
    */
   if (followSymlinks) {
      openFlags &= ~O_NOFOLLOW;
   }

   /*
    * Determine if hidden attribute needs to be updated.
    * It needs to be updated if a new file is created or an existing file is truncated.
    * Since Posix_Open does not tell us if a new file has been created when O_CREAT is
    * specified we need to find out if the file exists before an open that may create
    * it.
    */
   if (openInfo->mask & HGFS_OPEN_VALID_FILE_ATTR) {
      if ((openFlags & O_TRUNC) ||
          ((openFlags & O_CREAT) && (openFlags & O_EXCL))) {
         needToSetAttribute = TRUE;
      } else if (openFlags & O_CREAT) {
         int err = access(openInfo->utf8Name, F_OK);
         needToSetAttribute = (err != 0) && (errno == ENOENT);
      }
   }

   /*
    * Try to open the file with the requested mode, flags and permissions.
    */
   fd = Posix_Open(openInfo->utf8Name,
             openMode | openFlags,
             openPerms);
   if (fd < 0) {
      error = errno;
      LOG(4, ("%s: couldn't open file \"%s\": %s\n", __FUNCTION__,
              openInfo->utf8Name, strerror(error)));
      status = error;
      goto exit;
   }

   /* Set the rest of the Windows specific attributes if necessary. */
   if (needToSetAttribute) {
      HgfsSetHiddenXAttr(openInfo->utf8Name, openInfo->attr & HGFS_ATTR_HIDDEN);
   }

   /* Stat file to get its volume and file info */
   if (fstat(fd, &fileStat) < 0) {
      error = errno;
      LOG(4, ("%s: couldn't stat local file \"%s\": %s\n", __FUNCTION__,
              openInfo->utf8Name, strerror(error)));
      close(fd);
      status = error;
      goto exit;
   }

   /* Try to acquire an oplock. */
   if (openInfo->mask & HGFS_OPEN_VALID_SERVER_LOCK) {
      serverLock = openInfo->desiredLock;
      if (!HgfsAcquireServerLock(fd, session, &serverLock)) {
         openInfo->acquiredLock = HGFS_LOCK_NONE;
      } else {
         openInfo->acquiredLock = serverLock;
      }
   } else {
      openInfo->acquiredLock = HGFS_LOCK_NONE;
   }

   *fileDesc = fd;

   /* Set volume and file ids from stat results */
   localId->volumeId = fileStat.st_dev;
   localId->fileId = fileStat.st_ino;

  exit:
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAcquireServerLock --
 *
 *    Acquire a lease for the open file. Typically we try and get the exact
 *    lease desired, but if the client asked for HGFS_LOCK_OPPORTUNISTIC, we'll
 *    take the "best" lease we can get.
 *
 * Results:
 *    TRUE on success. serverLock contains the type of the lock acquired.
 *    FALSE on failure. serverLock is HGFS_LOCK_NONE.
 *
 *    XXX: This function has the potential to return per-platform error codes,
 *    but since it is opportunistic by nature, it isn't necessary to do so.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsAcquireServerLock(fileDesc fileDesc,            // IN: OS handle
                      HgfsSessionInfo *session,     // IN: session info
                      HgfsServerLock *serverLock)   // IN/OUT: Oplock asked for/granted
{
#ifdef HGFS_OPLOCKS
   HgfsServerLock desiredLock;
   int leaseType, error;

   ASSERT(serverLock);
   ASSERT(session);

   desiredLock = *serverLock;

   if (desiredLock == HGFS_LOCK_NONE) {
      return TRUE;
   }

   if (!HgfsIsServerLockAllowed(session)) {
      return FALSE;
   }

   /*
    * First tell the kernel which signal to send us. SIGIO is already the
    * default, but if we skip this step, we won't get the siginfo_t when
    * a lease break occurs.
    *
    * XXX: Do I need to do fcntl(fileDesc, F_SETOWN, getpid())?
    */

   if (fcntl(fileDesc, F_SETSIG, SIGIO)) {
      error = errno;
      Log("%s: Could not set SIGIO as the desired lease break signal for "
          "fd %d: %s\n", __FUNCTION__, fileDesc, strerror(error));

      return FALSE;
   }

   /*
    * If the client just wanted the best lock possible, start off with a write
    * lease and move down to a read lease if that was unavailable.
    */

   if ((desiredLock == HGFS_LOCK_OPPORTUNISTIC) ||
       (desiredLock == HGFS_LOCK_EXCLUSIVE)) {
      leaseType = F_WRLCK;
   } else if (desiredLock  == HGFS_LOCK_SHARED) {
      leaseType = F_RDLCK;
   } else {
      LOG(4, ("%s: Unknown server lock\n", __FUNCTION__));

      return FALSE;
   }
   if (fcntl(fileDesc, F_SETLEASE, leaseType)) {
      /*
       * If our client was opportunistic and we failed to get his lease because
       * someone else is already writing or reading to the file, try again with
       * a read lease.
       */

      if (desiredLock == HGFS_LOCK_OPPORTUNISTIC &&
          (errno == EAGAIN || errno == EACCES)) {
         leaseType = F_RDLCK;
         if (fcntl(fileDesc, F_SETLEASE, leaseType)) {
            error = errno;
            LOG(4, ("%s: Could not get any opportunistic lease for fd %d: %s\n",
                    __FUNCTION__, fileDesc, strerror(error)));

            return FALSE;
         }
      } else {
         error = errno;
         LOG(4, ("%s: Could not get %s lease for fd %d: %s\n",
                 __FUNCTION__, leaseType == F_WRLCK ? "write" : "read",
                 fileDesc, strerror(errno)));

         return FALSE;
      }
   }

   /* Got a lease of some kind. */
   LOG(4, ("%s: Got %s lease for fd %d\n", __FUNCTION__,
           leaseType == F_WRLCK ? "write" : "read", fileDesc));

   *serverLock = leaseType == F_WRLCK ? HGFS_LOCK_EXCLUSIVE : HGFS_LOCK_SHARED;
   return TRUE;
#else
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetattrResolveAlias --
 *
 *    Mac OS defines a special file type known as an alias which behaves like a
 *    symlink when viewed through the Finder, but is actually a regular file
 *    otherwise. Unlike symlinks, aliases cannot be broken; if the target file
 *    is deleted, so is the alias.
 *
 *    If the given filename is (or contains) an alias, this function will
 *    resolve it completely and set targetName to something non-NULL.
 *
 * Results:
 *    Zero on success. targetName is allocated if the file was an alias, and
 *    NULL otherwise.
 *    Non-zero on failure. targetName is unmodified.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsGetattrResolveAlias(char const *fileName,       // IN:  Input filename
                        char **targetName)          // OUT: Target filename
{
#ifndef __APPLE__
   *targetName = NULL;
   return 0;
#else
   char *myTargetName = NULL;
   HgfsInternalStatus status = HGFS_INTERNAL_STATUS_ERROR;
   CFURLRef resolvedRef = NULL;
   CFStringRef resolvedString;
   FSRef fileRef;
   Boolean targetIsFolder;
   Boolean wasAliased;
   OSStatus osStatus;

   ASSERT_ON_COMPILE(sizeof osStatus == sizeof (int32));

   /*
    * Create and resolve an FSRef of the desired path. We pass FALSE to
    * resolveAliasChains because aliases to aliases should behave as
    * symlinks to symlinks. If the file is an alias, wasAliased will be set to
    * TRUE and fileRef will reference the target file.
    */

   osStatus = FSPathMakeRef(fileName, &fileRef, NULL);
   if (osStatus != noErr) {
      LOG(4, ("%s: could not create file reference: error %d\n",
              __FUNCTION__, (int32)osStatus));
      goto exit;
   }

   /*
    * If alias points to an unmounted volume, the volume needs to be explicitly
    * mounted on the host. Mount flag kResolveAliasFileNoUI serves the purpose.
    *
    * XXX: This function returns fnfErr (file not found) if it encounters a
    * broken alias. Perhaps we should make that look like a dangling symlink
    * instead of returning an error?
    *
    * XXX: It also returns errors if it encounters a file with a .alias suffix
    * that isn't a real alias. That's OK for now because our caller
    * (HgfsGetattrFromName) will assume that an error means the file is a
    * regular file.
    */

   osStatus = FSResolveAliasFileWithMountFlags(&fileRef, FALSE, &targetIsFolder,
                                               &wasAliased,
                                               kResolveAliasFileNoUI);
   if (osStatus != noErr) {
      LOG(4, ("%s: could not resolve reference: error %d\n",
              __FUNCTION__, (int32)osStatus));
      goto exit;
   }

   if (wasAliased) {
      CFIndex maxPath;

      /*
       * This is somewhat convoluted. We create a CFURL from the FSRef because
       * we want to call CFURLGetFileSystemRepresentation() to get a UTF-8
       * string representing the target of the alias. But to call
       * CFStringGetMaximumSizeOfFileSystemRepresentation(), we need a
       * CFString, so we make one from the CFURL. Once we've got the max number
       * of bytes for a filename on the filesystem, we allocate some memory
       * and convert the CFURL to a basic UTF-8 string using a call to
       * CFURLGetFileSystemRepresentation().
       */

      resolvedRef = CFURLCreateFromFSRef(NULL, &fileRef);
      if (resolvedRef == NULL) {
         LOG(4, ("%s: could not create resolved URL reference from "
                 "resolved filesystem reference\n", __FUNCTION__));
         goto exit;
      }
      resolvedString = CFURLGetString(resolvedRef);
      if (resolvedString == NULL) {
         LOG(4, ("%s: could not create resolved string reference from "
                 "resolved URL reference\n", __FUNCTION__));
         goto exit;
      }
      maxPath = CFStringGetMaximumSizeOfFileSystemRepresentation(resolvedString);
      myTargetName = malloc(maxPath);
      if (myTargetName == NULL) {
         LOG(4, ("%s: could not allocate %"FMTSZ"d bytes of memory for "
                 "target name storage\n", __FUNCTION__, maxPath));
         goto exit;
      }
      if (!CFURLGetFileSystemRepresentation(resolvedRef, FALSE, myTargetName,
                                            maxPath)) {
         LOG(4, ("%s: could not convert and copy resolved URL reference "
                 "into allocated buffer\n", __FUNCTION__));
         goto exit;
      }

      *targetName = myTargetName;
      LOG(4, ("%s: file was an alias\n", __FUNCTION__));
   } else {
      *targetName = NULL;
      LOG(4, ("%s: file was not an alias\n", __FUNCTION__));
   }
   status = 0;

  exit:
   if (status != 0) {
      free(myTargetName);
   }

   if (resolvedRef != NULL) {
      CFRelease(resolvedRef);
   }

   return status;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetHiddenAttr --
 *
 *    For Mac hosts and Linux hosts, if a guest is Windows we force the "dot",
 *    files to be treated as hidden too in the Windows client by always setting
 *    the hidden attribute flag.
 *    Currently, this flag cannot be removed by Windows clients.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsGetHiddenAttr(char const *fileName,         // IN:  Input filename
                  HgfsFileAttrInfo *attr)       // OUT: Struct to copy into
{
   char *baseName;

   ASSERT(fileName);
   ASSERT(attr);

   baseName = strrchr(fileName, DIRSEPC);

   if ((baseName != NULL) &&
       (baseName[1] == '.') &&
       (strcmp(&baseName[1], ".") != 0) &&
       (strcmp(&baseName[1], "..") != 0)) {
      attr->mask |= HGFS_ATTR_VALID_FLAGS;
      attr->flags |= HGFS_ATTR_HIDDEN;
      /*
       * The request sets the forced flag so the client knows it is simulated
       * and is not a real attribute, which can only happen on a Windows server.
       * This allows the client to enforce some checks correctly if the flag
       * is real or not.
       * This replicates SMB behavior see bug 292189.
       */
      attr->flags |= HGFS_ATTR_HIDDEN_FORCED;
   } else {
      Bool isHidden = FALSE;
      /*
       * Do not propagate any error returned from HgfsGetHiddenXAttr.
       * Consider that the file is not hidden if can't get hidden attribute for
       * whatever reason; most likely it fails because hidden attribute is not supported
       * by the OS or file system.
       */
      HgfsGetHiddenXAttr(fileName, &isHidden);
      if (isHidden) {
         attr->mask |= HGFS_ATTR_VALID_FLAGS;
         attr->flags |= HGFS_ATTR_HIDDEN;
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsConvertComponentCase --
 *
 *    Do a case insensitive search of a directory for the specified entry. If
 *    a matching entry is found, return it in the convertedComponent argument.
 *
 * Results:
 *    On Success:
 *    Returns 0 and the converted component name in the argument convertedComponent.
 *    The length for the convertedComponent is returned in convertedComponentSize.
 *
 *    On Failure:
 *    Non-zero errno return, with convertedComponent and convertedComponentSize
 *    set to NULL and 0 respectively.
 *
 * Side effects:
 *    On success, allocated memory is returned in convertedComponent and needs
 *    to be freed.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsConvertComponentCase(char *currentComponent,           // IN
                         const char *dirPath,              // IN
                         const char **convertedComponent,  // OUT
                         size_t *convertedComponentSize)   // OUT
{
   struct dirent *dirent;
   DIR *dir = NULL;
   char *dentryName;
   size_t dentryNameLen;
   char *myConvertedComponent = NULL;
   size_t myConvertedComponentSize;
   int ret;

   ASSERT(currentComponent);
   ASSERT(dirPath);
   ASSERT(convertedComponent);
   ASSERT(convertedComponentSize);

   /* Open the specified directory. */
   dir = opendir(dirPath);
   if (!dir) {
      ret = errno;
      goto exit;
   }

   /*
    * Unicode_CompareIgnoreCase crashes with invalid unicode strings,
    * validate it before passing it to Unicode_* functions.
    */
   if (!Unicode_IsBufferValid(currentComponent, -1, STRING_ENCODING_UTF8)) {
      /* Invalid unicode string, return failure. */
      ret = EINVAL;
      goto exit;
   }

   /*
    * Read all of the directory entries. For each one, convert the name
    * to lower case and then compare it to the lower case component.
    */
   while ((dirent = readdir(dir))) {
      Unicode dentryNameU;
      int cmpResult;

      dentryName = dirent->d_name;
      dentryNameLen = strlen(dentryName);

      /*
       * Unicode_CompareIgnoreCase crashes with invalid unicode strings,
       * validate and convert it appropriately before passing it to Unicode_*
       * functions.
       */

      if (!Unicode_IsBufferValid(dentryName, dentryNameLen,
                                 STRING_ENCODING_DEFAULT)) {
         /* Invalid unicode string, skip the entry. */
         continue;
      }

      dentryNameU = Unicode_Alloc(dentryName, STRING_ENCODING_DEFAULT);

      cmpResult = Unicode_CompareIgnoreCase(currentComponent, dentryNameU);
      Unicode_Free(dentryNameU);

      if (cmpResult == 0) {
         /*
          * The current directory entry is a case insensitive match to
          * the specified component. Malloc and copy the current directory entry.
          */

         myConvertedComponentSize = dentryNameLen + 1;
         myConvertedComponent = malloc(myConvertedComponentSize);
         if (myConvertedComponent == NULL) {
            ret = errno;
            LOG(4, ("%s: failed to malloc myConvertedComponent.\n",
                    __FUNCTION__));
            goto exit;
         }
         Str_Strcpy(myConvertedComponent, dentryName, myConvertedComponentSize);

         /* Success. Cleanup and exit. */
         ret = 0;
         *convertedComponentSize = myConvertedComponentSize;
         *convertedComponent = myConvertedComponent;
         goto exit;
      }
   }

   /* We didn't find a match. Failure. */
   ret = ENOENT;

exit:
   if (dir) {
      closedir(dir);
   }
   if (ret) {
      *convertedComponent = NULL;
      *convertedComponentSize = 0;
   }
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsConstructConvertedPath --
 *
 *    Expand the passed string and append the converted path.
 *
 * Results:
 *    Returns 0 if successful, errno on failure. Note that this
 *    function cannot return ENOENT.
 *
 * Side effects:
 *    Reallocs the path.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsConstructConvertedPath(char **path,                 // IN/OUT
                           size_t *pathSize,            // IN/OUT
                           char *convertedPath,         // IN
                           size_t convertedPathSize)    // IN
{
   char *p;
   size_t convertedPathLen = convertedPathSize - 1;

   ASSERT(path);
   ASSERT(*path);
   ASSERT(convertedPath);
   ASSERT(pathSize);

   p = realloc(*path, *pathSize + convertedPathLen + sizeof (DIRSEPC));
   if (!p) {
      int error = errno;
      LOG(4, ("%s: failed to realloc.\n", __FUNCTION__));

      return error;
   }

   *path = p;
   *pathSize += convertedPathLen + sizeof (DIRSEPC);

   /* Copy out the converted component to curDir, and free it. */
   Str_Strncat(p, *pathSize, DIRSEPS, sizeof (DIRSEPS));
   Str_Strncat(p, *pathSize, convertedPath, convertedPathLen);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCaseInsensitiveLookup --
 *
 *    Do a case insensitive lookup for fileName. Each component past sharePath is
 *    looked-up case-insensitively. Expensive!
 *
 *    NOTE:
 *    shareName is always expected to be a prefix of fileName.
 *
 * Results:
 *    Returns 0 if successful and resolved path for fileName is returned in
 *    convertedFileName with its length in convertedFileNameLength.
 *    Otherwise returns non-zero errno with convertedFileName and
 *    convertedFileNameLength set to NULL and 0 respectively.
 *
 * Side effects:
 *    On success, allocated memory is returned in convertedFileName and needs
 *    to be freed.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsCaseInsensitiveLookup(const char *sharePath,           // IN
                          size_t sharePathLength,          // IN
                          char *fileName,                  // IN
                          size_t fileNameLength,           // IN
                          char **convertedFileName,        // OUT
                          size_t *convertedFileNameLength) // OUT
{
   char *currentComponent;
   char *curDir;
   char *nextComponent;
   int error = ENOENT;
   size_t curDirSize;
   char *convertedComponent = NULL;
   size_t convertedComponentSize = 0;

   ASSERT(sharePath);
   ASSERT(fileName);
   ASSERT(convertedFileName);
   ASSERT(fileNameLength >= sharePathLength);

   currentComponent = fileName + sharePathLength;
   /* Check there is something beyond the share name. */
   if (*currentComponent == '\0') {
      /*
       * The fileName is the same as sharePath. Nothing else to do.
       * Dup the string and return.
       */
      *convertedFileName = strdup(fileName);
      if (!*convertedFileName) {
         error = errno;
         *convertedFileName = NULL;
         *convertedFileNameLength = 0;
         LOG(4, ("%s: strdup on fileName failed.\n", __FUNCTION__));
      } else {
         *convertedFileNameLength = strlen(fileName);
      }

      return 0;
   }

   /* Skip a component separator if not in the share path. */
   if (*currentComponent == DIRSEPC) {
      currentComponent += 1;
   }

   curDirSize = sharePathLength + 1;
   curDir = malloc(curDirSize);
   if (!curDir) {
      error = errno;
      LOG(4, ("%s: failed to allocate for curDir\n", __FUNCTION__));
      goto exit;
   }
   Str_Strcpy(curDir, sharePath, curDirSize);

   while (TRUE) {
      /* Get the next component. */
      nextComponent = strchr(currentComponent, DIRSEPC);
      if (nextComponent != NULL) {
         *nextComponent = '\0';
      }

      /*
       * Try to match the current component against the one in curDir.
       * HgfsConvertComponentCase may return ENOENT. In that case return
       * the path case-converted uptil now (curDir) and append to it the
       * rest of the unconverted path.
       */
      error = HgfsConvertComponentCase(currentComponent, curDir,
                                       (const char **)&convertedComponent,
                                       &convertedComponentSize);
      /* Restore the path separator if we removed it earlier. */
      if (nextComponent != NULL) {
         *nextComponent = DIRSEPC;
      }

      if (error) {
         if (error == ENOENT) {
	    int ret;
            /*
             * Copy out the components starting from currentComponent. We do this
             * after replacing DIRSEPC, so all the components following
             * currentComponent gets copied.
             */
            ret = HgfsConstructConvertedPath(&curDir, &curDirSize, currentComponent,
                                             strlen(currentComponent) + 1);
            if (ret) {
               error = ret;
            }
         }

         if (error != ENOENT) {
            free(curDir);
         }
         break;
      }

      /* Expand curDir and copy out the converted component. */
      error = HgfsConstructConvertedPath(&curDir, &curDirSize, convertedComponent,
                                         convertedComponentSize);
      if (error) {
         free(curDir);
         free(convertedComponent);
         break;
      }

      /* Free the converted component. */
      free(convertedComponent);

      /* If there is no component after the current one then we are done. */
      if (nextComponent == NULL) {
         /* Set success. */
         error = 0;
         break;
      }

      /*
       * Set the current component pointer to point at the start of the next
       * component.
       */
      currentComponent = nextComponent + 1;
   }

   /* If the conversion was successful, return the result. */
   if (error == 0 || error == ENOENT) {
      *convertedFileName = curDir;
      *convertedFileNameLength = curDirSize;
   }

exit:
   if (error && error != ENOENT) {
      *convertedFileName = NULL;
      *convertedFileNameLength = 0;
   }
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerConvertCase --
 *
 *    Converts the fileName to appropriate case depending upon flags.
 *
 * Results:
 *    Returns HGFS_NAME_STATUS_COMPLETE if successful and converted
 *    path for fileName is returned in convertedFileName and it length in
 *    convertedFileNameLength.
 *
 *    Otherwise returns non-zero integer without affecting fileName with
 *    convertedFileName and convertedFileNameLength set to NULL and 0
 *    respectively.
 *
 * Side effects:
 *    On success, allocated memory is returned in convertedFileName and needs
 *    to be freed.
 *
 *-----------------------------------------------------------------------------
 */

HgfsNameStatus
HgfsServerConvertCase(const char *sharePath,              // IN
                      size_t sharePathLength,             // IN
                      char *fileName,                     // IN
                      size_t fileNameLength,              // IN
                      uint32 caseFlags,                   // IN
                      char **convertedFileName,           // OUT
                      size_t *convertedFileNameLength)    // OUT
{
   int error = 0;
   HgfsNameStatus nameStatus = HGFS_NAME_STATUS_COMPLETE;

   ASSERT(sharePath);
   ASSERT(fileName);
   ASSERT(convertedFileName);
   ASSERT(convertedFileNameLength);

   *convertedFileName = NULL;
   *convertedFileNameLength = 0;

   /*
    * Case-insensitive lookup is expensive, do it only if the flag is set
    * and file is inaccessible using the case passed to us. We use access(2)
    * call to check if the passed case of the file name is correct.
    */

   if (caseFlags == HGFS_FILE_NAME_CASE_INSENSITIVE &&
       access(fileName, F_OK) == -1) {
      LOG(4, ("%s: Case insensitive lookup, fileName: %s, flags: %u.\n",
              __FUNCTION__, fileName, caseFlags));

      error = HgfsCaseInsensitiveLookup(sharePath, sharePathLength,
                                        fileName, fileNameLength,
                                        convertedFileName,
                                        convertedFileNameLength);

      /*
       * Success or non-ENOENT error code. HgfsCaseInsensitiveLookup can
       * return ENOENT, and its ok to continue if it is ENOENT.
       */

      switch (error) {
         case ENOENT:
            nameStatus = HGFS_NAME_STATUS_COMPLETE;
            break;
         case ENOTDIR:
            nameStatus = HGFS_NAME_STATUS_NOT_A_DIRECTORY;
            break;
         default:
            nameStatus = HGFS_NAME_STATUS_FAILURE;
            break;
      }

      return nameStatus;
   }

   *convertedFileName = strdup(fileName);
   if (!*convertedFileName) {
      nameStatus = HGFS_NAME_STATUS_OUT_OF_MEMORY;
      LOG(4, ("%s: strdup on fileName failed.\n", __FUNCTION__));
   } else {
      *convertedFileNameLength = fileNameLength;
   }

   return nameStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerCaseConversionRequired --
 *
 *   Determines if the case conversion is required for this platform.
 *
 * Results:
 *   TRUE on Linux / Apple.
 *
 * Side effects:
 *   None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerCaseConversionRequired()
{
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEffectivePermissions --
 *
 *    Get permissions that are in efffect for the current user.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsEffectivePermissions(char *fileName,          // IN: Input filename
                         Bool readOnlyShare,      // IN: Share name
                         uint32 *permissions)     // OUT: Effective permissions
{
   *permissions = 0;
   if (access(fileName, R_OK) == 0) {
      *permissions |= HGFS_PERM_READ;
   }
   if (access(fileName, X_OK) == 0) {
      *permissions |= HGFS_PERM_EXEC;
   }
   if (!readOnlyShare && (access(fileName, W_OK) == 0)) {
      *permissions |= HGFS_PERM_WRITE;
   }
   return 0;
}


#if defined(__APPLE__)
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsConvertStat64ToStat --
 *
 *    Helper function that converts data in stat64 format into stat format.
 *    It returns creationTime in HGFS platform independent format.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsConvertStat64ToStat(const struct stat64 *stats64,  // IN: data in stat64 format
                        struct stat *stats,            // OUT: data in stat format
                        uint64 *creationTime)          // OUT: creation time
{
   stats->st_dev = stats64->st_dev;
   stats->st_ino = stats64->st_ino;
   stats->st_mode = stats64->st_mode;
   stats->st_nlink = stats64->st_nlink;
   stats->st_uid = stats64->st_uid;
   stats->st_gid = stats64->st_gid;
   stats->st_rdev = stats64->st_rdev;
   stats->st_atimespec = stats64->st_atimespec;
   stats->st_mtimespec = stats64->st_mtimespec;
   stats->st_ctimespec = stats64->st_ctimespec;
   stats->st_size = stats64->st_size;
   stats->st_blocks = stats64->st_blocks;
   stats->st_blksize = stats64->st_blksize;
   stats->st_flags = stats64->st_flags;
   stats->st_gen = stats64->st_gen;
   *creationTime =  HgfsConvertTimeSpecToNtTime(&stats64->st_birthtimespec);
}
#else


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetCreationTime --
 *
 *    Calculates actual or emulated file creation time from stat structure.
 *    Definition of stat structure are different on diferent platforms.
 *    This function hides away all these differences and produces 64 bit value
 *    which should be reported to the client.
 *
 * Results:
 *    Value that should be used as a file creation time stamp.
 *    The resulting timestamp is in platform independent HGFS format.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static uint64
HgfsGetCreationTime(const struct stat *stats)
{
   uint64 creationTime;
   /*
    * Linux and FreeBSD before v5 doesn't know about creation time; we just use the time
    * of last data modification for the creation time.
    * FreeBSD 5+ supprots file creation time.
    *
    * Using mtime when creation time is unavailable to be consistent with SAMBA.
    */

#ifdef __FreeBSD__
   /*
    * FreeBSD: All supported versions have timestamps with nanosecond resolution.
    *          FreeBSD 5+ has also file creation time.
    */
#   if __IS_FREEBSD_VER__(500043)
   creationTime   = HgfsConvertTimeSpecToNtTime(&stats->st_birthtimespec);
#   else
   creationTime   = HgfsConvertTimeSpecToNtTime(&stats->st_mtimespec);
#   endif
#elif defined(linux)
   /*
    * Linux: Glibc 2.3+ has st_Xtim.  Glibc 2.1/2.2 has st_Xtime/__unusedX on
    *        same place (see below).  We do not support Glibc 2.0 or older.
    */
#   if (__GLIBC__ == 2) && (__GLIBC_MINOR__ < 3)
   /*
    * stat structure is same between glibc 2.3 and older glibcs, just
    * these __unused fields are always zero. If we'll use __unused*
    * instead of zeroes, we get automatically nanosecond timestamps
    * when running on host which provides them.
    */
   creationTime   = HgfsConvertToNtTime(stats->st_mtime, stats->__unused2);
#   else
   creationTime   = HgfsConvertTimeSpecToNtTime(&stats->st_mtim);
#   endif
#else
   /*
    * Solaris: No nanosecond timestamps, no file create timestamp.
    */
   creationTime   = HgfsConvertToNtTime(stats->st_mtime, 0);
#endif
   return creationTime;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsStat --
 *
 *    Wrapper function that invokes stat64 on Mac OS and stat on Linux (where stat64 is
 *    unavailable).
 *    Returns filled stat structure and a file creation time. File creation time is
 *    the birthday time for Mac OS and last write time for Linux (which does not support
 *    file creation time).
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsStat(const char* fileName,   // IN: file name
         Bool followLink,        // IN: If true then follow symlink
         struct stat *stats,     // OUT: file attributes
         uint64 *creationTime)   // OUT: file creation time
{
   int error;
#if defined(__APPLE__)
   struct stat64 stats64;
   if (followLink) {
      error = stat64(fileName, &stats64);
   } else {
      error = lstat64(fileName, &stats64);
   }
   if (error == 0) {
      HgfsConvertStat64ToStat(&stats64, stats, creationTime);
   }
#else
   if (followLink) {
      error = Posix_Stat(fileName, stats);
   } else {
      error = Posix_Lstat(fileName, stats);
   }
   *creationTime = HgfsGetCreationTime(stats);
#endif
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsFStat --
 *
 *    Wrapper function that invokes stat64 on Mac OS and stat on Linux (where stat64 is
 *    unavailable).
 *    Returns filled stat structure and a file creation time. File creation time is
 *    the birthday time for Mac OS and last write time for Linux (which does not support
 *    file creation time).
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsFStat(int fd,                 // IN: file descriptor
          struct stat *stats,     // OUT: file attributes
          uint64 *creationTime)   // OUT: file creation time
{
   int error = 0;
#if defined(__APPLE__)
   struct stat64 stats64;
   error = fstat64(fd, &stats64);
   if (error == 0) {
      HgfsConvertStat64ToStat(&stats64, stats, creationTime);
   }
#else
   if (fstat(fd, stats) < 0) {
      error = errno;
   }
   *creationTime = HgfsGetCreationTime(stats);
#endif
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetattrFromName --
 *
 *    Performs a stat operation on the given filename, and, if it is a symlink,
 *    allocates the target filename on behalf of the caller and performs a
 *    readlink to get it. If not a symlink, the targetName argument is
 *    untouched. Does necessary translation between Unix file stats and the
 *    HgfsFileAttrInfo formats.
 *    NOTE: The function is different from HgfsGetAttrFromId: this function returns
 *    effectve permissions while HgfsGetAttrFromId does not.
 *    The reason for this asymmetry is that effective permissions are needed
 *    to get a new handle. If the file is already opened then
 *    getting effective permissions does not have any value. However getting
 *    effective permissions would hurt perfomance and should be avoided.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsGetattrFromName(char *fileName,                    // IN/OUT:  Input filename
                    HgfsShareOptions configOptions,    // IN: Share config options
                    char *shareName,                   // IN: Share name
                    HgfsFileAttrInfo *attr,            // OUT: Struct to copy into
                    char **targetName)                 // OUT: Symlink target filename
{
   HgfsInternalStatus status = 0;
   struct stat stats;
   int error;
   char *myTargetName = NULL;
   uint64 creationTime;

   ASSERT(fileName);
   ASSERT(attr);

   LOG(4, ("%s: getting attrs for \"%s\"\n", __FUNCTION__, fileName));

   error = HgfsStat(fileName,
                    HgfsServerPolicy_IsShareOptionSet(configOptions,
                                                      HGFS_SHARE_FOLLOW_SYMLINKS),
                    &stats,
                    &creationTime);
   if (error) {
      status = errno;
      LOG(4, ("%s: error stating file: %s\n", __FUNCTION__, strerror(status)));
      goto exit;
   }

   /*
    * Deal with the file type returned from lstat(2). We currently support
    * regular files, directories, and symlinks. On Mac OS, we'll additionally
    * treat finder aliases as symlinks.
    */

   if (S_ISDIR(stats.st_mode)) {
      attr->type = HGFS_FILE_TYPE_DIRECTORY;
      LOG(4, ("%s: is a directory\n", __FUNCTION__));
   } else if (S_ISLNK(stats.st_mode)) {
      attr->type = HGFS_FILE_TYPE_SYMLINK;
      LOG(4, ("%s: is a symlink\n", __FUNCTION__));

      /*
       * In the case of a symlink, we should populate targetName if the
       * caller asked. Use st_size and readlink() to do so.
       */

      if (targetName != NULL) {

         myTargetName = Posix_ReadLink(fileName);
         if (myTargetName == NULL) {
            error = errno;
            LOG(4, ("%s: readlink returned wrong size\n", __FUNCTION__));

            /*
             * Because of an unavoidable race between the lstat(2) and the
             * readlink(2), the symlink target may have lengthened and we may
             * not have read the entire link. If that happens, just return
             * "out of memory".
             */

            status = error ? error : ENOMEM;
            goto exit;
         }
      }
   } else {
      /*
       * Now is a good time to check if the file was an alias. If so, we'll
       * treat it as a symlink.
       *
       * XXX: If HgfsGetattrResolveAlias fails, we'll treat the file as a
       * regular file. This isn't completely correct (the function may have
       * failed because we're out of memory), but it's better than having to
       * call LScopyItemInfoForRef for each file, which may negatively affect
       * performance. See:
       *
       * http://lists.apple.com/archives/carbon-development/2001/Nov/msg00007.html
       */

      LOG(4, ("%s: NOT a directory or symlink\n", __FUNCTION__));
      if (HgfsGetattrResolveAlias(fileName, &myTargetName)) {
         LOG(4, ("%s: could not resolve file aliases\n", __FUNCTION__));
      }
      attr->type = HGFS_FILE_TYPE_REGULAR;
      if (myTargetName != NULL) {
         /*
          * At this point the alias target has been successfully resolved. If
          * the alias target is inside the same shared folder then convert it
          * to relative path. Converting to a relative path produces a symlink
          * that points to the target file in the guest OS. If the target lies
          * outside the shared folder then treat it the same way as if alias
          * has not been resolved.
          */

         HgfsNameStatus nameStatus;
         size_t sharePathLen;
         const char* sharePath;
         nameStatus = HgfsServerPolicy_GetSharePath(shareName,
                                                    strlen(shareName),
                                                    HGFS_OPEN_MODE_READ_ONLY,
                                                    &sharePathLen,
                                                    &sharePath);
         if (nameStatus == HGFS_NAME_STATUS_COMPLETE &&
             sharePathLen < strlen(myTargetName) &&
             Str_Strncmp(sharePath, myTargetName, sharePathLen) == 0) {
            char *relativeName;
            relativeName = HgfsBuildRelativePath(fileName, myTargetName);
            free(myTargetName);
            myTargetName = relativeName;
            if (myTargetName != NULL) {
               /*
                * Let's mangle the permissions and size of the file so that
                * it more closely resembles a symlink. The size should be
                * the length of the target name (not including the
                * nul-terminator), and the permissions should be 777.
                */

               stats.st_size = strlen(myTargetName);
               stats.st_mode |= ACCESSPERMS;
               attr->type = HGFS_FILE_TYPE_SYMLINK;
            } else {
               LOG(4, ("%s: out of memory\n", __FUNCTION__));
            }
         } else {
             LOG(4, ("%s: alias target is outside shared folder\n",
                     __FUNCTION__));
         }
      }
   }

   if (myTargetName != NULL && targetName != NULL) {
#if defined(__APPLE__)
      /*
       * HGFS clients will expect filenames in unicode normal form C
       * (precomposed) so Mac hosts must convert from normal form D
       * (decomposed).
       */

      if (!CodeSet_Utf8FormDToUtf8FormC(myTargetName, strlen(myTargetName),
                                        targetName, NULL)) {
         LOG(4, ("%s: Unable to normalize form C \"%s\"\n",
                 __FUNCTION__, myTargetName));
         status = HgfsConvertFromNameStatus(HGFS_NAME_STATUS_FAILURE);
         goto exit;
      }
#else
      *targetName = myTargetName;
      myTargetName = NULL;
#endif
   }

   HgfsStatToFileAttr(&stats, &creationTime, attr);

   /*
    * In the case we have a Windows client, force the hidden flag.
    * This will be ignored by Linux, Solaris clients.
    */
   HgfsGetHiddenAttr(fileName, attr);

   /* Get effective permissions if we can */
   if (!(S_ISLNK(stats.st_mode))) {
      HgfsOpenMode shareMode;
      uint32 permissions;
      HgfsNameStatus nameStatus;
      nameStatus = HgfsServerPolicy_GetShareMode(shareName, strlen(shareName),
                                                 &shareMode);
      if (nameStatus == HGFS_NAME_STATUS_COMPLETE &&
          HgfsEffectivePermissions(fileName,
                                   shareMode == HGFS_OPEN_MODE_READ_ONLY,
                                   &permissions) == 0) {
         attr->mask |= HGFS_ATTR_VALID_EFFECTIVE_PERMS;
         attr->effectivePerms = permissions;
      }
   }

exit:
   free(myTargetName);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetattrFromFd --
 *
 *    Performs a stat operation on the given file desc.
 *    Does necessary translation between Unix file stats and the
 *    HgfsFileAttrInfo formats.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsGetattrFromFd(int fd,                     // IN:  file descriptor
                  HgfsSessionInfo *session,   // IN:  session info
                  HgfsFileAttrInfo *attr)     // OUT: FileAttrInfo to copy into
{
   HgfsInternalStatus status = 0;
   struct stat stats;
   int error;
   HgfsOpenMode shareMode;
   HgfsHandle handle = HGFS_INVALID_HANDLE;
   char *fileName = NULL;
   size_t fileNameLen;
   uint64 creationTime;

   ASSERT(attr);
   ASSERT(session);

   LOG(4, ("%s: getting attrs for %u\n", __FUNCTION__, fd));

   error = HgfsFStat(fd, &stats, &creationTime);
   if (error) {
      LOG(4, ("%s: error stating file: %s\n", __FUNCTION__, strerror(error)));
      status = error;
      goto exit;
   }

   /*
    * For now, everything that isn't a directory or symlink is a regular
    * file.
    */

   if (S_ISDIR(stats.st_mode)) {
      attr->type = HGFS_FILE_TYPE_DIRECTORY;
      LOG(4, ("%s: is a directory\n", __FUNCTION__));
   } else if (S_ISLNK(stats.st_mode)) {
      attr->type = HGFS_FILE_TYPE_SYMLINK;
      LOG(4, ("%s: is a symlink\n", __FUNCTION__));

   } else {
      attr->type = HGFS_FILE_TYPE_REGULAR;
      LOG(4, ("%s: NOT a directory or symlink\n", __FUNCTION__));
   }

   HgfsStatToFileAttr(&stats, &creationTime, attr);

   /*
    * XXX - Correct share mode checking should be fully implemented.
    *
    * For now, we must ensure that the client only sees read only
    * attributes when the share is read only. This allows the client
    * to make decisions to fail write/delete operations.
    * It is required by clients who use file handles that
    * are cached, for setting attributes, renaming and deletion.
    */

   if (!HgfsFileDesc2Handle(fd, session, &handle)) {
      LOG(4, ("%s: could not get HGFS handle for fd %u\n", __FUNCTION__, fd));
      status = EBADF;
      goto exit;
   }

   if (!HgfsHandle2ShareMode(handle, session, &shareMode)) {
      LOG(4, ("%s: could not get share mode fd %u\n", __FUNCTION__, fd));
      status = EBADF;
      goto exit;
   }

   if (!HgfsHandle2FileName(handle, session, &fileName, &fileNameLen)) {
      LOG(4, ("%s: could not map cached target file handle %u\n",
              __FUNCTION__, handle));
      status = EBADF;
      goto exit;
   }

   /*
    * In the case we have a Windows client, force the hidden flag.
    * This will be ignored by Linux, Solaris clients.
    */

   HgfsGetHiddenAttr(fileName, attr);

   if (shareMode == HGFS_OPEN_MODE_READ_ONLY) {
      /*
       * Share does not allow write, so tell the client
       * everything is read only.
       */

      if (attr->mask & HGFS_ATTR_VALID_OWNER_PERMS) {
         attr->ownerPerms &= ~HGFS_PERM_WRITE;
      }
      if (attr->mask & HGFS_ATTR_VALID_GROUP_PERMS) {
         attr->groupPerms &= ~HGFS_PERM_WRITE;
      }
      if (attr->mask & HGFS_ATTR_VALID_OTHER_PERMS) {
         attr->otherPerms &= ~HGFS_PERM_WRITE;
      }
   }

exit:
   free(fileName);

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsStatToFileAttr --
 *
 *    Does necessary translation between Unix file stats and the
 *    HgfsFileAttrInfo formats.
 *    It expects creationTime to be in platform-independent HGFS format and
 *    stats in a platform-specific stat format. 
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsStatToFileAttr(struct stat *stats,       // IN: stat information
                   uint64 *creationTime,     // IN: file creation time
                   HgfsFileAttrInfo *attr)   // OUT: FileAttrInfo to copy into
{
   attr->size           = stats->st_size;
   attr->creationTime   = *creationTime;

#ifdef __FreeBSD__
   /*
    * FreeBSD: All supported versions have timestamps with nanosecond resolution.
    *          FreeBSD 5+ has also file creation time.
    */
   attr->accessTime     = HgfsConvertTimeSpecToNtTime(&stats->st_atimespec);
   attr->writeTime      = HgfsConvertTimeSpecToNtTime(&stats->st_mtimespec);
   attr->attrChangeTime = HgfsConvertTimeSpecToNtTime(&stats->st_ctimespec);
#elif defined(linux)
   /*
    * Linux: Glibc 2.3+ has st_Xtim.  Glibc 2.1/2.2 has st_Xtime/__unusedX on
    *        same place (see below).  We do not support Glibc 2.0 or older.
    */
#   if (__GLIBC__ == 2) && (__GLIBC_MINOR__ < 3)
   /*
    * stat structure is same between glibc 2.3 and older glibcs, just
    * these __unused fields are always zero. If we'll use __unused*
    * instead of zeroes, we get automatically nanosecond timestamps
    * when running on host which provides them.
    */
   attr->accessTime     = HgfsConvertToNtTime(stats->st_atime, stats->__unused1);
   attr->writeTime      = HgfsConvertToNtTime(stats->st_mtime, stats->__unused2);
   attr->attrChangeTime = HgfsConvertToNtTime(stats->st_ctime, stats->__unused3);
#   else
   attr->accessTime     = HgfsConvertTimeSpecToNtTime(&stats->st_atim);
   attr->writeTime      = HgfsConvertTimeSpecToNtTime(&stats->st_mtim);
   attr->attrChangeTime = HgfsConvertTimeSpecToNtTime(&stats->st_ctim);
#   endif
#else
   /*
    * Solaris, Mac OS: No nanosecond timestamps.
    */
   attr->accessTime     = HgfsConvertToNtTime(stats->st_atime, 0);
   attr->writeTime      = HgfsConvertToNtTime(stats->st_mtime, 0);
   attr->attrChangeTime = HgfsConvertToNtTime(stats->st_ctime, 0);
#endif

   attr->specialPerms   = (stats->st_mode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9;
   attr->ownerPerms     = (stats->st_mode & S_IRWXU) >> 6;
   attr->groupPerms     = (stats->st_mode & S_IRWXG) >> 3;
   attr->otherPerms     = stats->st_mode & S_IRWXO;
   LOG(4, ("%s: done, permissions %o%o%o%o, size %"FMT64"u\n", __FUNCTION__,
           attr->specialPerms, attr->ownerPerms, attr->groupPerms,
           attr->otherPerms, attr->size));
#ifdef __FreeBSD__
#   if !defined(VM_X86_64) && __FreeBSD_version >= 500043
#      define FMTTIMET ""
#   else
#      define FMTTIMET "l"
#   endif
#else
#   define FMTTIMET "l"
#endif
   LOG(4, ("access: %"FMTTIMET"d/%"FMT64"u \nwrite: %"FMTTIMET"d/%"FMT64"u \n"
           "attr: %"FMTTIMET"d/%"FMT64"u\n",
           stats->st_atime, attr->accessTime, stats->st_mtime, attr->writeTime,
           stats->st_ctime, attr->attrChangeTime));
#undef FMTTIMET

   attr->userId = stats->st_uid;
   attr->groupId = stats->st_gid;
   attr->hostFileId = stats->st_ino;
   attr->volumeId = stats->st_dev;
   attr->mask = HGFS_ATTR_VALID_TYPE |
      HGFS_ATTR_VALID_SIZE |
      HGFS_ATTR_VALID_CREATE_TIME |
      HGFS_ATTR_VALID_ACCESS_TIME |
      HGFS_ATTR_VALID_WRITE_TIME |
      HGFS_ATTR_VALID_CHANGE_TIME |
      HGFS_ATTR_VALID_SPECIAL_PERMS |
      HGFS_ATTR_VALID_OWNER_PERMS |
      HGFS_ATTR_VALID_GROUP_PERMS |
      HGFS_ATTR_VALID_OTHER_PERMS |
      HGFS_ATTR_VALID_USERID |
      HGFS_ATTR_VALID_GROUPID |
      HGFS_ATTR_VALID_FILEID |
      HGFS_ATTR_VALID_VOLID;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSetattrMode --
 *
 *    Set the permissions based on stat and attributes.
 *
 * Results:
 *    TRUE if permissions have changed.
 *    FALSE otherwise.
 *
 *    Note that newPermissions is always set.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsSetattrMode(struct stat *statBuf,       // IN: stat info
                HgfsFileAttrInfo *attr,     // IN: attrs to set
                mode_t *newPermissions)     // OUT: new perms
{
   Bool permsChanged = FALSE;

   ASSERT(statBuf);
   ASSERT(attr);
   ASSERT(newPermissions);

   *newPermissions = ~ALLPERMS;
   if (attr->mask & HGFS_ATTR_VALID_SPECIAL_PERMS) {
      *newPermissions |= attr->specialPerms << 9;
      permsChanged = TRUE;
   } else {
      *newPermissions |= statBuf->st_mode & (S_ISUID | S_ISGID | S_ISVTX);
   }
   if (attr->mask & HGFS_ATTR_VALID_OWNER_PERMS) {
      *newPermissions |= attr->ownerPerms << 6;
      permsChanged = TRUE;
   } else {
      *newPermissions |= statBuf->st_mode & S_IRWXU;
   }
   if (attr->mask & HGFS_ATTR_VALID_GROUP_PERMS) {
      *newPermissions |= attr->groupPerms << 3;
      permsChanged = TRUE;
   } else {
      *newPermissions |= statBuf->st_mode & S_IRWXG;
   }
   if (attr->mask & HGFS_ATTR_VALID_OTHER_PERMS) {
      *newPermissions |= attr->otherPerms;
      permsChanged = TRUE;
   } else {
      *newPermissions |= statBuf->st_mode & S_IRWXO;
   }
   return permsChanged;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSetattrOwnership --
 *
 *    Set the user and group ID based the attributes.
 *
 * Results:
 *    TRUE if ownership has changed.
 *    FALSE otherwise.
 *
 *    Note that newUid/newGid are always set.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsSetattrOwnership(HgfsFileAttrInfo *attr,     // IN: attrs to set
                     uid_t *newUid,              // OUT: new user ID
                     gid_t *newGid)              // OUT: new group ID
{
   Bool idChanged = FALSE;

   ASSERT(attr);
   ASSERT(newUid);
   ASSERT(newGid);

   *newUid = *newGid = -1;

   if (attr->mask & HGFS_ATTR_VALID_USERID) {
      *newUid = attr->userId;
      idChanged = TRUE;
   }

   if (attr->mask & HGFS_ATTR_VALID_GROUPID) {
      *newGid = attr->groupId;
      idChanged = TRUE;
   }

   return idChanged;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSetattrTimes --
 *
 *    Set the time stamps based on stat and attributes.
 *
 * Results:
 *    Zero on success. accessTime/modTime contain new times.
 *    Non-zero on failure.
 *
 *    Note that timesChanged is always set.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsSetattrTimes(struct stat *statBuf,       // IN: stat info
                 HgfsFileAttrInfo *attr,     // IN: attrs to set
                 HgfsAttrHint hints,         // IN: attr hints
                 struct timeval *accessTime, // OUT: access time
                 struct timeval *modTime,    // OUT: modification time
                 Bool *timesChanged)         // OUT: times changed
{
   HgfsInternalStatus status = 0;
   int error;

   ASSERT(statBuf);
   ASSERT(attr);
   ASSERT(accessTime);
   ASSERT(modTime);
   ASSERT(timesChanged);

   *timesChanged = FALSE;

   if (attr->mask & (HGFS_ATTR_VALID_ACCESS_TIME |
                    HGFS_ATTR_VALID_WRITE_TIME)) {

      /*
       * utime(2) only lets you update both atime and mtime at once, so
       * if either one needs updating, first we get the current times
       * and call utime with some combination of the current and new
       * times. This is a bit racy because someone else could update
       * one of them in between, but this seems to be how "touch" does
       * things, so we'll go with it. [bac]
       */

      if ((attr->mask & (HGFS_ATTR_VALID_ACCESS_TIME |
                        HGFS_ATTR_VALID_WRITE_TIME))
          != (HGFS_ATTR_VALID_ACCESS_TIME | HGFS_ATTR_VALID_WRITE_TIME)) {

         /*
          * XXX Set also usec from nsec stat fields.
          */

         accessTime->tv_sec = statBuf->st_atime;
         accessTime->tv_usec = 0;
         modTime->tv_sec = statBuf->st_mtime;
         modTime->tv_usec = 0;
      }

      /*
       * If times need updating, we either use the guest-provided time or the
       * host time.  HGFS_ATTR_HINT_SET_x_TIME_ will be set if we should use
       * the guest time, and alwaysUseHostTime will be TRUE if the config
       * option to always use host time is set.
       */

      if (attr->mask & HGFS_ATTR_VALID_ACCESS_TIME) {
         if (!alwaysUseHostTime && (hints & HGFS_ATTR_HINT_SET_ACCESS_TIME)) {
            /* Use the guest-provided time */
            struct timespec ts;

            HgfsConvertFromNtTimeNsec(&ts, attr->accessTime);
            accessTime->tv_sec = ts.tv_sec;
            accessTime->tv_usec = ts.tv_nsec / 1000;
         } else {
            /* Use the host's time */
            struct timeval tv;

            if (gettimeofday(&tv, NULL) != 0) {
               error = errno;
               LOG(4, ("%s: gettimeofday error: %s\n", __FUNCTION__,
                       strerror(error)));
               status = error;
               goto exit;
            }

            accessTime->tv_sec = tv.tv_sec;
            accessTime->tv_usec = tv.tv_usec;
         }
      }

      if (attr->mask & HGFS_ATTR_VALID_WRITE_TIME) {
         if (!alwaysUseHostTime && (hints & HGFS_ATTR_HINT_SET_WRITE_TIME)) {
            struct timespec ts;

            HgfsConvertFromNtTimeNsec(&ts, attr->writeTime);
            modTime->tv_sec = ts.tv_sec;
            modTime->tv_usec = ts.tv_nsec / 1000;
         } else {
            struct timeval tv;

            if (gettimeofday(&tv, NULL) != 0) {
               error = errno;
               LOG(4, ("%s: gettimeofday error: %s\n", __FUNCTION__,
                       strerror(error)));
               status = error;
               goto exit;
            }

            modTime->tv_sec = tv.tv_sec;
            modTime->tv_usec = tv.tv_usec;
         }
      }
      *timesChanged = TRUE;
   }

exit:
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSetattrFromFd --
 *
 *    Handle a Setattr request by file descriptor.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsSetattrFromFd(HgfsHandle file,          // IN: file descriptor
                  HgfsSessionInfo *session, // IN: session info
                  HgfsFileAttrInfo *attr,   // OUT: attrs to set
                  HgfsAttrHint hints)       // IN: attr hints
{
   HgfsInternalStatus status = 0, timesStatus;
   int error;
   struct stat statBuf;
   struct timeval times[2];
   mode_t newPermissions;
   uid_t newUid = -1;
   gid_t newGid = -1;
   Bool permsChanged = FALSE;
   Bool timesChanged = FALSE;
   Bool idChanged = FALSE;
   int fd;
   HgfsServerLock serverLock;
   HgfsOpenMode shareMode;

   ASSERT(session);
   ASSERT(file != HGFS_INVALID_HANDLE);

   status = HgfsGetFd(file, session, FALSE, &fd);
   if (status != 0) {
      LOG(4, ("%s: Could not get file descriptor\n", __FUNCTION__));
      goto exit;
   }

   if (!HgfsHandle2ShareMode(file, session, &shareMode)) {
      LOG(4, ("%s: could not get share mode fd %u\n", __FUNCTION__, fd));
      status = EBADF;
      goto exit;
   }

   if (shareMode == HGFS_OPEN_MODE_READ_ONLY) {
      status = EACCES;
      goto exit;
   }

   /* We need the old stats so that we can preserve times. */
   if (fstat(fd, &statBuf) == -1) {
      error = errno;
      LOG(4, ("%s: error stating file %u: %s\n", __FUNCTION__, 
              fd, strerror(error)));
      status = error;
      goto exit;
   }

   /*
    * Try to make each requested attribute change. In the event that
    * one operation fails, we still attempt to perform any other
    * operations that the driver requested. We return success only
    * if all operations succeeded.
    */

   /*
    * Set permissions based on what we got in the packet. If we didn't get
    * a particular bit, use the existing permissions. In that case we don't
    * toggle permsChanged since it should not influence our decision of
    * whether to actually call chmod or not.
    */

   permsChanged = HgfsSetattrMode(&statBuf, attr, &newPermissions);
   if (permsChanged) {
      LOG(4, ("%s: set mode %o\n", __FUNCTION__, (unsigned)newPermissions));

      if (fchmod(fd, newPermissions) < 0) {
         error = errno;
         LOG(4, ("%s: error chmoding file %u: %s\n", __FUNCTION__,
                 fd, strerror(error)));
         status = error;
      }
   }

   idChanged = HgfsSetattrOwnership(attr, &newUid, &newGid);
   if (idChanged) {
      LOG(4, ("%s: set uid %"FMTUID" and gid %"FMTUID"\n", __FUNCTION__,
              newUid, newGid));
      if (fchown(fd, newUid, newGid) < 0) {
         error = errno;
         LOG(4, ("%s: error chowning file %u: %s\n", __FUNCTION__,
                 fd, strerror(error)));
         status = error;
      }
   }

   if (attr->mask & HGFS_ATTR_VALID_SIZE) {
      /*
       * XXX: Truncating the file will trigger an oplock break. The client
       * should have predicted this and removed the oplock prior to sending
       * the truncate request. At this point, the server must safeguard itself
       * against deadlock.
       */

      if (!HgfsHandle2ServerLock(file, session, &serverLock)) {
         LOG(4, ("%s: File handle is no longer valid.\n", __FUNCTION__));
         status = EBADF;
      } else if (serverLock != HGFS_LOCK_NONE) {
         LOG(4, ("%s: Client attempted to truncate an oplocked file\n",
                 __FUNCTION__));
         status = EBUSY;
      } else if (ftruncate(fd, attr->size) < 0) {
         error = errno;
         LOG(4, ("%s: error truncating file %u: %s\n", __FUNCTION__,
                 fd, strerror(error)));
         status = error;
      } else {
         LOG(4, ("%s: set size %"FMT64"u\n", __FUNCTION__, attr->size));
      }
   }

   if (attr->mask & HGFS_ATTR_VALID_FLAGS) {
       char *localName;
       size_t localNameSize;
       if (HgfsHandle2FileName(file, session, &localName, &localNameSize)) {
          status = HgfsSetHiddenXAttr(localName, attr->flags & HGFS_ATTR_HIDDEN);
          free(localName);
       }
   }

   timesStatus = HgfsSetattrTimes(&statBuf, attr, hints,
                                  &times[0], &times[1], &timesChanged);
   if (timesStatus == 0 && timesChanged) {
      uid_t uid;

      LOG(4, ("%s: setting new times\n", __FUNCTION__));

      /*
       * If the VMX is either the file owner or running as root, switch to
       * superuser briefly to set the files times using futimes. Otherwise
       * return an error.
       */

      if (!Id_IsSuperUser() && (getuid() != statBuf.st_uid)) {
         LOG(4, ("%s: only owner of file %u or root can call futimes\n",
                 __FUNCTION__, fd));
         /* XXX: Linux kernel says both EPERM and EACCES are valid here. */
         status = EPERM;
         goto exit;
      }
      uid = Id_BeginSuperUser();
      /*
       * XXX Newer glibc provide also lutimes() and futimes()
       *     when we politely ask with -D_GNU_SOURCE -D_BSD_SOURCE
       */

      if (futimes(fd, times) < 0) {
         error = errno;
         LOG(4, ("%s: futimes error on file %u: %s\n", __FUNCTION__,
                 fd, strerror(error)));
         status = error;
      }
      Id_EndSuperUser(uid);
   } else if (timesStatus != 0) {
      status = timesStatus;
   }

exit:
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSetattrFromName --
 *
 *    Handle a Setattr request by name.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */
static HgfsInternalStatus
HgfsSetattrFromName(char *cpName,             // IN: Name
                    size_t cpNameSize,        // IN: Name length
                    HgfsFileAttrInfo *attr,   // IN: attrs to set
                    HgfsAttrHint hints,       // IN: attr hints
                    uint32 caseFlags,         // IN: case-sensitivity flags
                    HgfsSessionInfo *session) // IN: session info
{
   HgfsInternalStatus status = 0, timesStatus;
   HgfsNameStatus nameStatus;
   int error, fd;
   char *localName;
   struct stat statBuf;
   struct timeval times[2];
   mode_t newPermissions;
   uid_t newUid = -1;
   gid_t newGid = -1;
   Bool permsChanged = FALSE;
   Bool timesChanged = FALSE;
   Bool idChanged = FALSE;
   HgfsServerLock serverLock;
   HgfsShareOptions configOptions;

   nameStatus = HgfsServerGetAccess(cpName, cpNameSize,
                                    HGFS_OPEN_MODE_WRITE_ONLY, caseFlags,
                                    &localName, NULL);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: access check failed\n", __FUNCTION__));
      status = HgfsConvertFromNameStatus(nameStatus);
      goto exit;
   }

   ASSERT(localName);

   /* Get the config options. */
   nameStatus = HgfsServerPolicy_GetShareOptions(cpName, cpNameSize,
                                                 &configOptions);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, cpName));
      goto exit;
   }

   if (!HgfsServerPolicy_IsShareOptionSet(configOptions,
                                          HGFS_SHARE_FOLLOW_SYMLINKS)) {
      /*
       * If followSymlink option is not set, verify that the pathname isn't a
       * symlink. Some of the following syscalls (chmod, for example) will
       * follow a link. So we need to verify the final component too. The
       * parent has already been verified in HgfsServerGetAccess.
       *
       * XXX: This is racy. But clients interested in preventing a race should
       * have sent us a Setattr packet with a valid HGFS handle.
       */

      if (File_IsSymLink(localName)) {
         LOG(4, ("%s: pathname contains a symlink\n", __FUNCTION__));
         status = EINVAL;
         goto exit_free;
      }
   }

   LOG(4, ("%s: setting attrs for \"%s\"\n", __FUNCTION__, localName));

   /* We need the old stats so that we can preserve times. */
   if (Posix_Lstat(localName, &statBuf) == -1) {
      error = errno;
      LOG(4, ("%s: error stating file \"%s\": %s\n", __FUNCTION__, 
              localName, strerror(error)));
      status = error;
      goto exit_free;
   }

   /*
    * Try to make each requested attribute change. In the event that
    * one operation fails, we still attempt to perform any other
    * operations that the driver requested. We return success only
    * if all operations succeeded.
    */

   /*
    * Set permissions based on what we got in the packet. If we didn't get
    * a particular bit, use the existing permissions. In that case we don't
    * toggle permsChanged since it should not influence our decision of
    * whether to actually call chmod or not.
    */

   permsChanged = HgfsSetattrMode(&statBuf, attr, &newPermissions);
   if (permsChanged) {
      LOG(4, ("%s: set mode %o\n", __FUNCTION__, (unsigned)newPermissions));

      if (Posix_Chmod(localName, newPermissions) < 0) {
         error = errno;
         LOG(4, ("%s: error chmoding file \"%s\": %s\n", __FUNCTION__,
                 localName, strerror(error)));
         status = error;
      }
   }

   idChanged = HgfsSetattrOwnership(attr, &newUid, &newGid);

   /*
    * Chown changes the uid and gid together. If one of them should
    * not be changed, we pass in -1.
    */

   if (idChanged) {
      if (Posix_Lchown(localName, newUid, newGid) < 0) {
         error = errno;
         LOG(4, ("%s: error chowning file \"%s\": %s\n", __FUNCTION__,
                 localName, strerror(error)));
         status = error;
      }
   }

   if (attr->mask & HGFS_ATTR_VALID_SIZE) {
      /*
       * XXX: Truncating the file will trigger an oplock break. The client
       * should have predicted this and removed the oplock prior to sending
       * the truncate request. At this point, the server must safeguard itself
       * against deadlock.
       */

      if (HgfsFileHasServerLock(localName, session, &serverLock, &fd)) {
         LOG(4, ("%s: Client attempted to truncate an oplocked file\n",
                 __FUNCTION__));
         status = EBUSY;
      } else if (Posix_Truncate(localName, attr->size) < 0) {
         error = errno;
         LOG(4, ("%s: error truncating file \"%s\": %s\n", __FUNCTION__,
                 localName, strerror(error)));
         status = error;
      } else {
         LOG(4, ("%s: set size %"FMT64"u\n", __FUNCTION__, attr->size));
      }
   }

   if (attr->mask & HGFS_ATTR_VALID_FLAGS) {
      status = HgfsSetHiddenXAttr(localName, attr->flags & HGFS_ATTR_HIDDEN);
   }

   timesStatus = HgfsSetattrTimes(&statBuf, attr, hints,
                             &times[0], &times[1], &timesChanged);
   if (timesStatus == 0 && timesChanged) {
      /*
       * XXX Newer glibc provide also lutimes() and futimes()
       *     when we politely ask with -D_GNU_SOURCE -D_BSD_SOURCE
       */

      if (Posix_Utimes(localName, times) < 0) {
         error = errno;
         LOG(4, ("%s: utimes error on file \"%s\": %s\n", __FUNCTION__,
                 localName, strerror(error)));
         status = error;
      }
   } else if (timesStatus != 0) {
      status = timesStatus;
   }

exit_free:
   free(localName);

exit:
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerScandir --
 *
 *    The cross-platform HGFS server code will call into this function
 *    in order to populate a list of dents. In the Linux case, we want to avoid
 *    using scandir(3) because it makes no provisions for not following
 *    symlinks. Instead, we'll open(2) the directory with O_DIRECTORY and
 *    O_NOFOLLOW, call getdents(2) directly, then close(2) the directory.
 *
 * Results:
 *    Zero on success. numDents contains the number of directory entries found.
 *    Non-zero on error.
 *
 * Side effects:
 *    Memory allocation.
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerScandir(char const *baseDir,      // IN: Directory to search in
                  size_t baseDirLen,        // IN: Ignored
                  Bool followSymlinks,      // IN: followSymlinks config option
                  DirectoryEntry ***dents,  // OUT: Array of DirectoryEntrys
                  int *numDents)            // OUT: Number of DirectoryEntrys
{
   int fd = -1, result;
   DirectoryEntry **myDents = NULL;
   int myNumDents = 0;
   HgfsInternalStatus status = 0;
   int openFlags = O_NONBLOCK | O_RDONLY | O_DIRECTORY | O_NOFOLLOW;

   /*
    * XXX: glibc uses 8192 (BUFSIZ) when it can't get st_blksize from a stat.
    * Should we follow its lead and use stat to get st_blksize?
    */
   char buffer[8192];

   /* Follow symlinks if config option is set. */
   if (followSymlinks) {
      openFlags &= ~O_NOFOLLOW;
   }

   /* We want a directory. No FIFOs. Symlinks only if config option is set. */
   result = Posix_Open(baseDir, openFlags);
   if (result < 0) {
      status = errno;
      LOG(4, ("%s: error in open: %d (%s)\n", __FUNCTION__, status, 
              strerror(status)));
      goto exit;
   }
   fd = result;

   /*
    * Rather than read a single dent at a time, batch up multiple dents
    * in each call by using a buffer substantially larger than one dent.
    */
   while ((result = getdents(fd, (void *)buffer, sizeof buffer)) > 0) {
      size_t offset = 0;
      while (offset < result) {
         DirectoryEntry *newDent, **newDents;

         newDent = (DirectoryEntry *)(buffer + offset);

         /* This dent had better fit in the actual space we've got left. */
         ASSERT(newDent->d_reclen <= result - offset);

         /* Add another dent pointer to the dents array. */
         newDents = realloc(myDents, sizeof *myDents * (myNumDents + 1));
         if (newDents == NULL) {
            status = ENOMEM;
            goto exit;
         }
         myDents = newDents;

         /*
          * Allocate the new dent and set it up. We do a straight memcpy of
          * the entire record to avoid dealing with platform-specific fields.
          */

         myDents[myNumDents] = malloc(newDent->d_reclen);
         if (myDents[myNumDents] == NULL) {
            status = ENOMEM;
            goto exit;
         }
         memcpy(myDents[myNumDents], newDent, newDent->d_reclen);

         /*
          * Dent is done. Bump the offset to the batched buffer to process the
          * next dent within it.
          */
         myNumDents++;
         offset += newDent->d_reclen;
      }
   }

   if (result == -1) {
      status = errno;
      LOG(4, ("%s: error in getdents: %d (%s)\n", __FUNCTION__, status, 
              strerror(status)));
      goto exit;
   }

  exit:
   if (fd != -1 && close(fd) < 0) {
      status = errno;
      LOG(4, ("%s: error in close: %d (%s)\n", __FUNCTION__, status,
              strerror(status)));
   }

   /*
    * On error, free all allocated dents. On success, set the dents pointer
    * given to us by the client.
    */

   if (status != 0) {
      size_t i;
      for (i = 0; i < myNumDents; i++) {
         free(myDents[i]);
      }
      free(myDents);
   } else {
      *dents = myDents;
      *numDents = myNumDents;
   }
   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * Request Handler Functions
 * -------------------------
 *
 * The functions that follow are all of the same type: they take a
 * request packet which came from the driver, process it, and fill out
 * a reply packet which is then sent back to the driver. They are
 * called by DispatchPacket, which dispatches an incoming packet to
 * the correct handler function based on the packet's opcode.
 *
 * These functions all take the following as input:
 *
 * - A pointer to a buffer containing the incoming request packet,
 * - A pointer to a buffer big enough to hold the outgoing reply packet,
 * - A pointer to the size of the incoming packet, packetSize.
 *
 * After processing the request, the handler functions write the reply
 * packet into the output buffer and set the packetSize to be the size
 * of the OUTGOING reply packet. The ServerLoop function uses the size
 * to send the reply back to the driver.
 *
 * Note that it is potentially okay for the caller to use the same
 * buffer for both input and output; handler functions should make
 * sure they are safe w.r.t. this possibility by storing any state
 * from the input buffer before they clobber it by potentially writing
 * output into the same buffer.
 *
 * Handler functions should return zero if they successfully processed
 * the request, or a negative error if an unrecoverable error
 * occurred. Normal errors (e.g. a poorly formed request packet)
 * should be handled by sending an error packet back to the driver,
 * NOT by returning an error code to the caller, because errors
 * returned by handler functions cause the server to terminate.
 *
 * [bac]
 *
 *----------------------------------------------------------------------
 */


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerOpen --
 *
 *    Handle an Open request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerOpen(char const *packetIn,     // IN: incoming packet
               size_t packetSize,        // IN: size of packet
               HgfsSessionInfo *session) // IN: session info
{
   HgfsNameStatus nameStatus;
   HgfsInternalStatus status;
   int newFd = -1;
   HgfsLocalId localId;
   HgfsFileOpenInfo openInfo;
   char *localName = NULL;
   HgfsServerLock serverLock = HGFS_LOCK_NONE;
   HgfsShareOptions configOptions;
   int followSymlinks;
   char *packetOut;
   size_t packetOutSize;
   HgfsOpenFlags savedOpenFlags = 0;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackOpenRequest(packetIn, packetSize, &openInfo)) {
      status = EPROTO;
      goto exit;
   }

   /* HGFS_OPEN_VALID_FILE_NAME is checked in the unpack function. */
   if (!(openInfo.mask & HGFS_OPEN_VALID_MODE)) {
      LOG(4, ("%s: filename or mode not provided\n", __FUNCTION__));
      status = EINVAL;
      goto exit;
   }

   nameStatus = HgfsServerGetAccess(openInfo.cpName, openInfo.cpNameSize,
                                    openInfo.mode, openInfo.caseFlags,
                                    &localName, NULL);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: access check failed\n", __FUNCTION__));
      status = HgfsConvertFromNameStatus(nameStatus);
      goto exit;
   }

   if (openInfo.mask & HGFS_OPEN_VALID_FLAGS) {
      savedOpenFlags = openInfo.flags;
      if (!HgfsServerCheckOpenFlagsForShare(&openInfo, &openInfo.flags)) {
         /* Incompatible open mode with share mode. */
         status = EACCES;
         goto exit;
      }
   }

   ASSERT(localName);
   openInfo.utf8Name = localName;

   LOG(4, ("%s: opening \"%s\", mode %u, flags %u, perms %u%u%u%u\n",
           __FUNCTION__, openInfo.utf8Name,
           (openInfo.mask & HGFS_OPEN_VALID_MODE) ? openInfo.mode : 0,
           (openInfo.mask & HGFS_OPEN_VALID_FLAGS) ? openInfo.flags : 0,
           (openInfo.mask & HGFS_OPEN_VALID_SPECIAL_PERMS) ?
           openInfo.specialPerms : 0,
           (openInfo.mask & HGFS_OPEN_VALID_OWNER_PERMS) ?
           openInfo.ownerPerms : 0,
           (openInfo.mask & HGFS_OPEN_VALID_GROUP_PERMS) ?
           openInfo.groupPerms : 0,
           (openInfo.mask & HGFS_OPEN_VALID_OTHER_PERMS) ?
           openInfo.otherPerms : 0));

   /*
    * XXX: Before opening the file, see if we already have this file opened on
    * the server with an oplock on it. If we do, we must fail the new open
    * request, otherwise we will trigger an oplock break that the guest cannot
    * handle at this time (since the HGFS server is running in the context of
    * the vcpu thread), and we'll deadlock.
    *
    * Until we overcome this limitation via Crosstalk, we will be extra smart
    * in the client drivers so as to prevent open requests on handles that
    * already have an oplock. And the server will protect itself like so.
    *
    * XXX: With some extra effort, we could allow a second open for read here,
    * since that won't break a shared oplock, but the clients should already
    * realize that the second open can be avoided via sharing handles, too.
    */

   if (HgfsFileHasServerLock(localName, session, &serverLock, &newFd)) {
      LOG (4, ("%s: Client tried to open new handle for oplocked file %s.\n",
               __FUNCTION__, localName));
      status = EBUSY;
      goto exit;
   }

   /* Get the config options. */
   nameStatus = HgfsServerPolicy_GetShareOptions(openInfo.cpName,
                                                 openInfo.cpNameSize,
                                                 &configOptions);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, openInfo.cpName));
      status = ENOENT;
      goto exit;
   }

   followSymlinks = HgfsServerPolicy_IsShareOptionSet(configOptions,
                                                      HGFS_SHARE_FOLLOW_SYMLINKS);

   /* See if the name is valid, and if so add it and return the handle. */
   status = HgfsValidateOpen(&openInfo, followSymlinks, session, &localId,
                             &newFd);
   if (status == 0) {
      ASSERT(newFd >= 0);

      /*
       * Open succeeded, so make new node and return its handle. If we fail,
       * it's almost certainly an internal server error.
       */

      if (!HgfsCreateAndCacheFileNode(&openInfo, &localId, newFd, FALSE,
                                      session)) {
         status = HGFS_INTERNAL_STATUS_ERROR;
         goto exit;
      }

      if (HgfsPackOpenReply(packetIn, status, &openInfo, &packetOut,
                            &packetOutSize)) {
         if (!HgfsPacketSend(packetOut, packetOutSize, session, 0)) {
            free(packetOut);
         }
      } else {
         status = EPROTO;
      }
   } else {
      /*
       * The open failed, if we modified the open flags, force the return
       * status to be access denied, not the error for the modified open.
       */
      if (openInfo.mask & HGFS_OPEN_VALID_FLAGS &&
          savedOpenFlags != openInfo.flags &&
          status == ENOENT) {
         status = EACCES;
      }
   }

  exit:
   free(localName);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerRead --
 *
 *    Handle a Read request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerRead(char const *packetIn,     // IN: incoming packet
               size_t packetSize,        // IN: size of packet
               HgfsSessionInfo *session) // IN: session info
{
   HgfsRequest *header = (HgfsRequest *)packetIn;
   int fd;
   int error;
   HgfsInternalStatus status;
   uint32 extra;
   Bool sequentialOpen;
   HgfsHandle file;
   uint64 offset;
   uint32 requiredSize;
   char *payload;
   uint32 *replyActualSize;
   size_t replySize;
   char *packetOut;

   ASSERT(packetIn);
   ASSERT(session);

   if (header->op == HGFS_OP_READ_V3) {
      HgfsRequestReadV3 *request =
                        (HgfsRequestReadV3 *)HGFS_REQ_GET_PAYLOAD_V3(packetIn);
      HgfsReplyReadV3 *reply;

      file = request->file;
      offset = request->offset;
      requiredSize = request->requiredSize;

      replySize = HGFS_REP_PAYLOAD_SIZE_V3(reply) - 1;
      ASSERT(HGFS_LARGE_PACKET_MAX >= replySize);
      extra = HGFS_LARGE_PACKET_MAX - replySize;

      /*
       * requiredSize is user-provided, so this test must be carefully
       * written to prevent wraparounds.
       */
      if (requiredSize > extra) {
         /*
          * The client wants to read more bytes than our payload can handle.
          * Truncate the request
          */
         requiredSize = extra;
      }

      packetOut = Util_SafeMalloc(replySize + requiredSize);
      reply = (HgfsReplyReadV3 *)HGFS_REP_GET_PAYLOAD_V3(packetOut);
      payload = reply->payload;
      replyActualSize = &reply->actualSize;
      reply->reserved = 0;
   } else {
      HgfsRequestRead *request = (HgfsRequestRead *)packetIn;
      HgfsReplyRead *reply;

      file = request->file;
      offset = request->offset;
      requiredSize = request->requiredSize;

      replySize = sizeof *reply - 1;
      ASSERT(HGFS_PACKET_MAX >= replySize);
      extra = HGFS_PACKET_MAX - replySize;

      /*
       * requiredSize is user-provided, so this test must be carefully
       * written to prevent wraparounds.
       */

      if (requiredSize > extra) {
         /*
          * The client wants to read more bytes than our payload can handle.
          * Truncate the request
          */

         requiredSize = extra;
      }

      packetOut = Util_SafeMalloc(replySize + requiredSize);
      reply = (HgfsReplyRead *)packetOut;
      payload = reply->payload;
      replyActualSize = &reply->actualSize;
   }

   LOG(4, ("%s: read fh %u, offset %"FMT64"u, count %u\n", __FUNCTION__,
           file, offset, requiredSize));

   /* Get the file descriptor from the cache */
   status = HgfsGetFd(file, session, FALSE, &fd);

   if (status != 0) {
      LOG(4, ("%s: Could not get file descriptor\n", __FUNCTION__));
      free(packetOut);

      return status;
   }

   if (!HgfsHandleIsSequentialOpen(file, session, &sequentialOpen)) {
      LOG(4, ("%s: Could not get sequenial open status\n", __FUNCTION__));
      free(packetOut);

      return EBADF;
   }

#if defined(GLIBC_VERSION_21) || defined(__APPLE__)
   /* Read from the file. */
   if (sequentialOpen) {
      error = read(fd, payload, requiredSize);
   } else {
      error = pread(fd, payload, requiredSize, offset);
   }
#else
   /*
    * Seek to the offset and read from the file. Grab the IO lock to make
    * this and the subsequent read atomic.
    */

   SyncMutex_Lock(&session->fileIOLock);
   if (!sequentialOpen) {
#   ifdef linux
      {
         uint64 res;
#      if !defined(VM_X86_64)
         error = _llseek(fd, offset >> 32, offset & 0xFFFFFFFF, &res, 0);
#      else
         error = llseek(fd, offset >> 32, offset & 0xFFFFFFFF, &res, 0);
#      endif
      }
#   else
      error = lseek(fd, offset, 0);
#   endif

      if (error < 0) {
         status = errno;
         LOG(4, ("%s: could not seek to %"FMT64"u: %s\n", __FUNCTION__,
                 offset, strerror(status)));
         SyncMutex_Unlock(&session->fileIOLock);
         goto error;
      }
   }

   error = read(fd, payload, requiredSize);
   SyncMutex_Unlock(&session->fileIOLock);
#endif
   if (error < 0) {
      status = errno;
      LOG(4, ("%s: error reading from file: %s\n", __FUNCTION__,
              strerror(status)));
      goto error;
   }

   LOG(4, ("%s: read %d bytes\n", __FUNCTION__, error));
   *replyActualSize = error;
   replySize += error;

   /* Send the reply. */
   if (!HgfsPackAndSendPacket(packetOut, replySize, 0, header->id, session, 0)) {
      status = 0;
      goto error;
   }
   return 0;

error:
   free(packetOut);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerWrite --
 *
 *    Handle a Write request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerWrite(char const *packetIn,     // IN: incoming packet
                size_t packetSize,        // IN: size of packet
                HgfsSessionInfo *session) // IN: session info
{
   HgfsRequest *header = (HgfsRequest *)packetIn;
   uint32 extra;
   HgfsInternalStatus status;
   int fd;
   int error;
   Bool sequentialOpen;
   HgfsHandle file;
   HgfsWriteFlags flags;
   uint64 offset;
   uint32 requiredSize;
   char *payload;
   uint32 *actualSize;
   size_t replySize;
   char *packetOut;

   ASSERT(packetIn);
   ASSERT(session);

   if (header->op == HGFS_OP_WRITE_V3) {
      HgfsRequestWriteV3 *request;
      HgfsReplyWriteV3 *reply;

      replySize = HGFS_REP_PAYLOAD_SIZE_V3(reply);
      packetOut = Util_SafeMalloc(replySize);

      request = (HgfsRequestWriteV3 *)HGFS_REQ_GET_PAYLOAD_V3(packetIn);
      reply = (HgfsReplyWriteV3 *)HGFS_REP_GET_PAYLOAD_V3(packetOut);

      /* Enforced by the dispatch function */
      ASSERT(packetSize >= HGFS_REQ_PAYLOAD_SIZE_V3(request) - 1);
      extra = packetSize - (HGFS_REQ_PAYLOAD_SIZE_V3(request) - 1);

      file = request->file;
      flags = request->flags;
      offset = request->offset;
      payload = request->payload;
      requiredSize = request->requiredSize;
      actualSize = &reply->actualSize;
      reply->reserved = 0;
   } else {
      HgfsRequestWrite *request;
      HgfsReplyWrite *reply;

      replySize = sizeof *reply;
      packetOut = Util_SafeMalloc(replySize);

      request = (HgfsRequestWrite *)packetIn;
      reply = (HgfsReplyWrite *)packetOut;

      /* Enforced by the dispatch function */
      ASSERT(packetSize >= sizeof *request - 1);
      extra = packetSize - (sizeof *request - 1);

      file = request->file;
      flags = request->flags;
      offset = request->offset;
      payload = request->payload;
      requiredSize = request->requiredSize;
      actualSize = &reply->actualSize;
   }

   LOG(4, ("%s: write fh %u, offset %"FMT64"u, count %u, extra %u\n",
           __FUNCTION__, file, offset, requiredSize, extra));

   /* Get the file desriptor from the cache */
   status = HgfsGetFd(file, session, ((flags & HGFS_WRITE_APPEND) ?
                                      TRUE : FALSE),
                      &fd);

   if (status != 0) {
      LOG(4, ("%s: Could not get file descriptor\n", __FUNCTION__));
      free(packetOut);

      return status;
   }

   if (!HgfsHandleIsSequentialOpen(file, session, &sequentialOpen)) {
      LOG(4, ("%s: Could not get sequential open status\n", __FUNCTION__));
      free(packetOut);

      return EBADF;
   }

   /*
    * requiredSize is user-provided, so this test must be carefully
    * written to prevent wraparounds.
    */

   if (requiredSize > extra) {
      /*
       * The driver wants to write more bytes than there is in its payload.
       * Truncate the request
       */

      requiredSize = extra;
   }

#if defined(GLIBC_VERSION_21) || defined(__APPLE__)
   /* Write to the file. */
   if (sequentialOpen) {
      error = write(fd, payload, requiredSize);
   } else {
      error = pwrite(fd, payload, requiredSize, offset);
   }
#else
   /*
    * Seek to the offset and write from the file. Grab the IO lock to make
    * this and the subsequent write atomic.
    */
   SyncMutex_Lock(&session->fileIOLock);
   if (!sequentialOpen) {
#   ifdef linux
      {
         uint64 res;
#      if !defined(VM_X86_64)
         error = _llseek(fd, offset >> 32, offset & 0xFFFFFFFF, &res, 0);
#      else
         error = llseek(fd, offset >> 32, offset & 0xFFFFFFFF, &res, 0);
#      endif
      }
#   else
      error = lseek(fd, offset, 0);
#   endif

      if (error < 0) {
         status = errno;
         LOG(4, ("%s: could not seek to %"FMT64"u: %s\n", __FUNCTION__,
                 offset, strerror(status)));
         SyncMutex_Unlock(&session->fileIOLock);
         goto error;
      }
   }

   error = write(fd, payload, requiredSize);
   SyncMutex_Unlock(&session->fileIOLock);
#endif
   if (error < 0) {
      status = errno;
      LOG(4, ("%s: error writing to file: %s\n", __FUNCTION__, 
              strerror(status)));
      goto error;
   }

   LOG(4, ("%s: wrote %d bytes\n", __FUNCTION__, error));
   *actualSize = error;
   status = 0;

   if (!HgfsPackAndSendPacket(packetOut, replySize, 0, header->id,
                              session, 0)) {
      goto error;
   }
   return 0;

error:
   free(packetOut);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSearchOpen --
 *
 *    Handle a "Search Open" request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerSearchOpen(char const *packetIn,     // IN: incoming packet
                     size_t packetSize,        // IN: size of packet
                     HgfsSessionInfo *session) // IN: session info
{
   HgfsRequest *header;
   HgfsHandle *replySearch;
   uint32 extra;
   size_t baseDirLen;
   char *baseDir;
   HgfsHandle handle;
   HgfsInternalStatus status;
   HgfsNameStatus nameStatus;
   char *dirName;
   uint32 dirNameLength;
   HgfsCaseType caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   size_t replySize;
   char *packetOut;

   ASSERT(packetIn);
   ASSERT(session);
   header = (HgfsRequest *)packetIn;

   if (header->op == HGFS_OP_SEARCH_OPEN_V3) {
      HgfsRequestSearchOpenV3 *requestV3;
      HgfsReplySearchOpenV3 *replyV3;

      replySize = HGFS_REP_PAYLOAD_SIZE_V3(replyV3);
      packetOut = Util_SafeMalloc(replySize);

      requestV3 = (HgfsRequestSearchOpenV3 *)HGFS_REQ_GET_PAYLOAD_V3(packetIn);
      replyV3 = (HgfsReplySearchOpenV3 *)HGFS_REP_GET_PAYLOAD_V3(packetOut);

      /* Enforced by the dispatch function */
      ASSERT(packetSize >= HGFS_REQ_PAYLOAD_SIZE_V3(requestV3));
      extra = packetSize - HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);

      caseFlags = requestV3->dirName.caseType;
      dirName = requestV3->dirName.name;
      dirNameLength = requestV3->dirName.length;
      replySearch = &replyV3->search;
      replyV3->reserved = 0;
      LOG(4, ("%s: HGFS_OP_SEARCH_OPEN_V3\n", __FUNCTION__));
   } else {
      HgfsRequestSearchOpen *request = (HgfsRequestSearchOpen *)packetIn;

      replySize = sizeof (HgfsReplySearchOpen);
      packetOut = Util_SafeMalloc(replySize);

      /* Enforced by the dispatch function */
      ASSERT(packetSize >= sizeof *request);
      extra = packetSize - sizeof *request;

      dirName = request->dirName.name;
      dirNameLength = request->dirName.length;
      replySearch = &((HgfsReplySearchOpen *)packetOut)->search;
   }

   /*
    * request->dirName.length is user-provided, so this test must be carefully
    * written to prevent wraparounds.
    */

   if (dirNameLength > extra) {
      /* The input packet is smaller than the request */
      status = EPROTO;
      goto exit;
   }
   /* It is now safe to read the dirName */

   nameStatus = HgfsServerGetAccess(dirName, dirNameLength,
                                    HGFS_OPEN_MODE_READ_ONLY,
                                    caseFlags, &baseDir, &baseDirLen);
   switch (nameStatus) {
   case HGFS_NAME_STATUS_COMPLETE:
   {
      char *inEnd;
      char *next;
      int len;

      ASSERT(baseDir);
      LOG(4, ("%s: searching in \"%s\", %s.\n", __FUNCTION__, baseDir,
              dirName));

      inEnd = dirName + dirNameLength;

      /* Get the first component. */
      len = CPName_GetComponent(dirName, inEnd, (char const **) &next);
      if (len < 0) {
         LOG(4, ("%s: get first component failed\n", __FUNCTION__));
         status = ENOENT;
         goto exit;
      }

      if (*inEnd != '\0') {
         /*
          * NT4 clients can send the name without a nul-terminator.
          * The space for the  nul is included and tested for in the size
          * calculations above. Size of structure (includes a single
          * character of the name) and the full dirname length.
          */
         *inEnd = '\0';
      }

      LOG(4, ("%s: dirName: %s.\n", __FUNCTION__, dirName));
      status = HgfsServerSearchRealDir(baseDir,
                                       baseDirLen,
                                       DIRECTORY_SEARCH_TYPE_DIR,
                                       dirName, session, &handle);
      free(baseDir);
      if (status != 0) {
         LOG(4, ("%s: couldn't scandir\n", __FUNCTION__));
         goto exit;
      }
      break;
   }

   case HGFS_NAME_STATUS_INCOMPLETE_BASE:
      /*
       * This is the base of our namespace, so enumerate all
       * shares. [bac]
       */

      LOG(4, ("%s: opened search on base\n", __FUNCTION__));
      status = HgfsServerSearchVirtualDir(HgfsServerPolicy_GetShares,
                                          HgfsServerPolicy_GetSharesInit,
                                          HgfsServerPolicy_GetSharesCleanup,
                                          DIRECTORY_SEARCH_TYPE_BASE,
                                          session,
                                          &handle);
      if (status != 0) {
         LOG(4, ("%s: couldn't enumerate shares\n", __FUNCTION__));
         goto exit;
      }
      break;

   default:
      LOG(4, ("%s: access check failed\n", __FUNCTION__));
      status = HgfsConvertFromNameStatus(nameStatus);
      goto exit;
   }

   if (DOLOG(4)) {
      HgfsServerDumpDents(handle, session);
   }

   /*
    * Return handle to the search object as the reply to the search
    * open.
    */

   *replySearch = handle;

   if (!HgfsPackAndSendPacket(packetOut, replySize, 0, header->id,
                              session, 0)) {
      status = 0;
      goto exit;
   }
   return 0;

exit:
   free(packetOut);

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSearchRead --
 *
 *    Handle a "Search Read" request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerSearchRead(char const *packetIn,     // IN: incoming packet
                     size_t packetSize,        // IN: size of packet
                     HgfsSessionInfo *session) // IN: session info
{
   uint32 requestedOffset;
   HgfsFileAttrInfo attr;
   HgfsInternalStatus status;
   HgfsNameStatus nameStatus;
   HgfsHandle hgfsSearchHandle;
   DirectoryEntry *dent;
   HgfsSearch search;
   HgfsShareOptions configOptions = 0;
   char *packetOut;
   size_t packetOutSize;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackSearchReadRequest(packetIn, packetSize, &attr,
                                    &hgfsSearchHandle, &requestedOffset)) {
      return EPROTO;
   }

   LOG(4, ("%s: read search #%u, offset %u\n", __FUNCTION__,
           hgfsSearchHandle, requestedOffset));

   if (!HgfsGetSearchCopy(hgfsSearchHandle, session, &search)) {
      LOG(4, ("%s: handle %u is invalid\n", __FUNCTION__, hgfsSearchHandle));
      return EBADF;
   }

   /* Get the config options. */
   if (search.utf8ShareNameLen != 0) {
      nameStatus = HgfsServerPolicy_GetShareOptions(search.utf8ShareName,
                                                    search.utf8ShareNameLen,
                                                    &configOptions);
      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__,
                 search.utf8ShareName));
         free(search.utf8Dir);
         free(search.utf8ShareName);

         return ENOENT;
      }
   }

   while ((dent = HgfsGetSearchResult(hgfsSearchHandle, session,
                                      requestedOffset, FALSE)) != NULL) {
      unsigned int length;
      char *fullName;
      char *sharePath;
      size_t sharePathLen;
      size_t fullNameLen;
      size_t entryNameLen;
      char *entryName = NULL;
      Bool freeEntryName = FALSE;

      length = strlen(dent->d_name);

      /* Each type of search gets a dent's attributes in a different way. */
      switch (search.type) {
      case DIRECTORY_SEARCH_TYPE_DIR:

         /*
          * Construct the UTF8 version of the full path to the file, and call
          * HgfsGetattrFromName to get the attributes of the file.
          */

         fullNameLen = search.utf8DirLen + 1 + length;
         fullName = (char *)malloc(fullNameLen + 1);
         if (!fullName) {
            LOG(4, ("%s: could not allocate space for \"%s\\%s\"\n",
                    __FUNCTION__, search.utf8Dir, dent->d_name));
            free(search.utf8Dir);
            free(search.utf8ShareName);
            free(dent);

            return ENOMEM;
         }
         memcpy(fullName, search.utf8Dir, search.utf8DirLen);
         fullName[search.utf8DirLen] = DIRSEPC;
         memcpy(&fullName[search.utf8DirLen + 1], dent->d_name, length + 1);

         LOG(4, ("%s: about to stat \"%s\"\n", __FUNCTION__, fullName));

         status = HgfsGetattrFromName(fullName, configOptions,
                                      search.utf8ShareName, &attr, NULL);
         if (status != 0) {
            HgfsOp savedOp = attr.requestType;
            LOG(4, ("%s: stat FAILED %s (%d)\n", __FUNCTION__, 
                    fullName, status));
            memset(&attr, 0, sizeof attr);
            attr.requestType = savedOp;
            attr.type = HGFS_FILE_TYPE_REGULAR;
            attr.mask = 0;
         }

         free(fullName);

#if defined(__APPLE__)
         /*
          * HGFS clients receive names in unicode normal form C,
          * (precomposed) so Mac hosts must convert from normal form D
          * (decomposed).
          */

         if (!CodeSet_Utf8FormDToUtf8FormC((const char *)dent->d_name, length,
                                           &entryName, &entryNameLen)) {
            LOG(4, ("%s: Unable to normalize form C \"%s\"\n", __FUNCTION__,
                    dent->d_name));
            /* Skip this entry and continue. */
            free(dent);
            continue;
         }

         freeEntryName = TRUE;
#else /* defined(__APPLE__) */
         entryName = dent->d_name;
         entryNameLen = length;
#endif /* defined(__APPLE__) */
         break;
      case DIRECTORY_SEARCH_TYPE_BASE:

         /*
          * For a search enumerating all shares, give the default attributes
          * for '.' and ".." (which aren't really shares anyway). Each real
          * share gets resolved into its full path, and gets its attributes
          * via HgfsGetattrFromName.
          */

         if (strcmp(dent->d_name, ".") == 0 ||
             strcmp(dent->d_name, "..") == 0) {
            LOG(4, ("%s: assigning %s default attributes\n",
                    __FUNCTION__, dent->d_name));
            HgfsServerGetDefaultDirAttrs(&attr);
         } else {
            /* Check permission on the share and get the share path */
            nameStatus =
               HgfsServerPolicy_GetSharePath(dent->d_name, length,
                                             HGFS_OPEN_MODE_READ_ONLY,
                                             &sharePathLen,
                                             (char const **)&sharePath);
            if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
               LOG(4, ("%s: No such share or access denied\n", __FUNCTION__));
               free(dent);
               free(search.utf8Dir);
               free(search.utf8ShareName);

               return HgfsConvertFromNameStatus(nameStatus);
            }

            /*
             * Server needs to produce list of shares that is consistent with
             * the list defined in UI. If a share can't be accessed because of
             * problems on the host, the server still enumerates it and
             * returns to the client.
             */

            status = HgfsGetattrFromName(sharePath, configOptions, dent->d_name,
					 &attr, NULL);
            if (status != 0) {
               /*
                * The dent no longer exists. Log the event.
                */

               LOG(4, ("%s: stat FAILED\n", __FUNCTION__));
            }
         }

	 /*
	  * No conversion needed on OS X because dent->d_name is the shareName
	  * that was converted to normal form C in hgfsServerPolicyHost.
	  */

	 entryName = dent->d_name;
	 entryNameLen = length;
         break;
      case DIRECTORY_SEARCH_TYPE_OTHER:

         /*
          * The POSIX implementation of HgfsSearchOpen could not have created
          * this kind of search.
          */

         NOT_IMPLEMENTED();
         break;
      default:
         NOT_IMPLEMENTED();
         break;
      }

      free(search.utf8Dir);
      free(search.utf8ShareName);
      LOG(4, ("%s: dent name is \"%s\" len = %"FMTSZ"u\n", __FUNCTION__,
              entryName, entryNameLen));

      /*
       * We need to unescape the name before sending it back to the client
       */

      entryNameLen = HgfsEscape_Undo(entryName, entryNameLen + 1);

      /*
       * XXX: HgfsPackSearchReadReply will error out if the dent we
       * give it is too large for the packet. Prior to
       * HgfsPackSearchReadReply, we'd skip the dent and return the next
       * one with success. Now we return an error. This may be a non-issue
       * since what filesystems allow dent lengths as high as 6144 bytes?
       */

      status = 0;
      if (!HgfsPackSearchReadReply(packetIn, status, entryName, entryNameLen,
                                   &attr, &packetOut, &packetOutSize)) {
         status = EPROTO;
      }

      if (freeEntryName) {
         free(entryName);
      }
      free(dent);

      if (status == 0 &&
          !HgfsPacketSend(packetOut, packetOutSize, session, 0)) {
         free(packetOut);
      }
      return status;
   }

   /* No entry at this offset */
   free(search.utf8Dir);
   free(search.utf8ShareName);
   LOG(4, ("%s: no entry\n", __FUNCTION__));
   status = 0;
   if (!HgfsPackSearchReadReply(packetIn, status, NULL, 0, &attr,
                                &packetOut, &packetOutSize)) {
      status = EPROTO;
   }
   if (status == 0 &&
       !HgfsPacketSend(packetOut, packetOutSize, session, 0)) {
      free(packetOut);
   }

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetattr --
 *
 *    Handle a Getattr request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerGetattr(char const *packetIn,     // IN: incoming packet
                  size_t packetSize,        // IN: size of packet
                  HgfsSessionInfo *session) // IN: session info
{
   char *localName;
   HgfsAttrHint hints = 0;
   HgfsFileAttrInfo attr;
   HgfsInternalStatus status = 0;
   HgfsNameStatus nameStatus;
   char *cpName;
   size_t cpNameSize;
   char *targetName = NULL;
   uint32 targetNameLen;
   HgfsHandle file = HGFS_INVALID_HANDLE; /* file handle from driver */
   uint32 caseFlags = 0;
   HgfsShareOptions configOptions;
   char *packetOut;
   size_t packetOutSize;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackGetattrRequest(packetIn, packetSize, &attr, &hints, &cpName,
                                 &cpNameSize, &file, &caseFlags)) {
      status = EPROTO;
      goto exit;
   }

   /* Client wants us to reuse an existing handle. */
   if (hints & HGFS_ATTR_HINT_USE_FILE_DESC) {
      int fd;
      status = HgfsGetFd(file, session, FALSE, &fd);

      if (status != 0) {
         LOG(4, ("%s: Could not get file descriptor\n", __FUNCTION__));
         goto exit;
      }
      status = HgfsGetattrFromFd(fd, session, &attr);
      targetNameLen = 0;

   } else {
      /*
       * Depending on whether this file/dir is real or virtual, either
       * forge its attributes or look them up in the actual filesystem.
       */

      nameStatus = HgfsServerGetAccess(cpName, cpNameSize,
                                       HGFS_OPEN_MODE_READ_ONLY,
                                       caseFlags, &localName, NULL);

      switch (nameStatus) {
      case HGFS_NAME_STATUS_INCOMPLETE_BASE:
         /*
          * This is the base of our namespace; make up fake status for
          * this directory.
          */

         LOG(4, ("%s: getting attrs for base dir\n", __FUNCTION__));
         HgfsServerGetDefaultDirAttrs(&attr);
         break;

      case HGFS_NAME_STATUS_COMPLETE:
         /* This is a regular lookup; proceed as usual */
         ASSERT(localName);

         /* Get the config options. */
         nameStatus = HgfsServerPolicy_GetShareOptions(cpName, cpNameSize,
                                                       &configOptions);
         if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
            LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, cpName));
            free(localName);
            status = ENOENT;
            goto exit;
         }

         status = HgfsGetattrFromName(localName, configOptions, cpName, &attr,
                                      &targetName);
         free(localName);
         if (status != 0) {
            /*
             * If it is a dangling share server should not return ENOENT
             * to the client because it causes confusion: a name that is returned
             * by directory enumeration should not produce "name not found"
             * error.
             * Replace it with a more appropriate error code: no such device.
             */
            if (status == ENOENT && HgfsIsShareRoot(cpName, cpNameSize)) {
               status = ENXIO;
            }
            goto exit;
         }
         break;

      default:
         status = HgfsConvertFromNameStatus(nameStatus);
         goto exit;
      }
      targetNameLen = targetName ? strlen(targetName) : 0;
   }
   status = HgfsPackGetattrReply(packetIn, status, &attr, targetName,
                                 targetNameLen, &packetOut, &packetOutSize) ? 0 : EPROTO;
   free(targetName);

   if (status == 0 &&
       !HgfsPacketSend(packetOut, packetOutSize, session, 0)) {
      free(packetOut);
   }

exit:
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSetattr --
 *
 *    Handle a Setattr request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerSetattr(char const *packetIn,     // IN: incoming packet
                  size_t packetSize,        // IN: size of packet
                  HgfsSessionInfo *session) // IN: session info
{
   HgfsInternalStatus status;
   HgfsFileAttrInfo attr;
   char *cpName;
   size_t cpNameSize = 0;
   HgfsAttrHint hints = 0;
   HgfsHandle file = HGFS_INVALID_HANDLE;
   uint32 caseFlags = 0;
   char *packetOut;
   size_t packetOutSize;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackSetattrRequest(packetIn, packetSize, &attr, &hints,
                                 &cpName, &cpNameSize, &file, &caseFlags)) {
      status = EPROTO;
      goto exit;
   }

   if (file != HGFS_INVALID_HANDLE) {
      status = HgfsSetattrFromFd(file, session, &attr, hints);
   } else {
      status = HgfsSetattrFromName(cpName, cpNameSize, &attr, hints,
				   caseFlags, session);
   }
   if (!HgfsPackSetattrReply(packetIn, status, &packetOut, &packetOutSize)) {
      status = EPROTO;
      goto exit;
   }
   if (!HgfsPacketSend(packetOut, packetOutSize, session, 0)) {
      free(packetOut);
      /* We can't send the packet, ignore the error if any. */
   }
   status = 0;

exit:
   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerCreateDir --
 *
 *    Handle a CreateDir request.
 *
 *    Simply converts to the local filename, calls mkdir on the
 *    file, and returns an appropriate response to the driver.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerCreateDir(char const *packetIn,     // IN: incoming packet
                    size_t packetSize,        // IN: size of packet
                    HgfsSessionInfo *session) // IN: session info
{
   HgfsNameStatus nameStatus;
   HgfsCreateDirInfo info;
   char *localName;
   int error;
   mode_t permissions;
   char *packetOut;
   size_t packetOutSize;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackCreateDirRequest(packetIn, packetSize, &info)) {
      return EPROTO;
   }

   nameStatus = HgfsServerGetAccess(info.cpName, info.cpNameSize,
                                    HGFS_OPEN_MODE_WRITE_ONLY,
                                    info.caseFlags, &localName, NULL);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: access check failed\n", __FUNCTION__));

      return HgfsConvertFromNameStatus(nameStatus);
   }

   ASSERT(localName);

   /*
    * Create mode_t for use in mkdir(). If owner permissions are missing, use
    * read/write/execute for the owner permissions. If group or other
    * permissions are missing, use the owner permissions.
    *
    * This sort of makes sense. If the Windows driver wants to make a dir
    * read-only, it probably intended for the dir to be 666. Since creating
    * a directory requires a valid mode, it's highly unlikely that we'll ever
    * be creating a directory without owner permissions.
    */

   permissions = ~ALLPERMS;
   permissions |= info.mask & HGFS_CREATE_DIR_VALID_SPECIAL_PERMS ?
                  info.specialPerms << 9 : 0;
   permissions |= info.mask & HGFS_CREATE_DIR_VALID_OWNER_PERMS ?
                  info.ownerPerms << 6 : S_IRWXU;
   permissions |= info.mask & HGFS_CREATE_DIR_VALID_GROUP_PERMS ?
                  info.groupPerms << 3 : (permissions & S_IRWXU) >> 3;
   permissions |= info.mask & HGFS_CREATE_DIR_VALID_OTHER_PERMS ?
                  info.otherPerms : (permissions & S_IRWXU) >> 6;

   LOG(4, ("%s: making dir \"%s\", mode %"FMTMODE"\n", __FUNCTION__,
           localName, permissions));

   error = Posix_Mkdir(localName, permissions);
   if ((info.mask & HGFS_CREATE_DIR_VALID_FILE_ATTR) &&
       (info.fileAttr & HGFS_ATTR_HIDDEN)) {
      /*
       *  Set hidden attribute when requested.
       *  Do not fail directory creation if setting hidden attribute fails.
       */

      HgfsSetHiddenXAttr(localName, TRUE);
   }

   free(localName);
   if (error) {
      error = errno;
      LOG(4, ("%s: error: %s\n", __FUNCTION__, strerror(error)));
      return error;
   }
   if (!HgfsPackCreateDirReply(packetIn, 0, &packetOut, &packetOutSize)) {
      return EPROTO;
   }
   if (!HgfsPacketSend(packetOut, packetOutSize, session, 0)) {
      free(packetOut);
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerDeleteFile --
 *
 *    Handle a Delete File request.
 *
 *    Simply converts to the local filename, calls unlink on the
 *    file, and returns an appropriate response to the driver.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerDeleteFile(char const *packetIn,     // IN: incoming packet
                     size_t packetSize,        // IN: size of packet
                     HgfsSessionInfo *session) // IN: session info
{
   HgfsNameStatus nameStatus;
   char *localName;
   int error;
   HgfsHandle file = HGFS_INVALID_HANDLE;
   HgfsDeleteHint hints = 0;
   char *cpName;
   size_t cpNameSize;
   uint32 caseFlags;
   char *packetOut;
   size_t packetOutSize;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackDeleteRequest(packetIn, packetSize, &cpName, &cpNameSize,
                                &hints, &file, &caseFlags)) {
      return EPROTO;
   }

   if (hints & HGFS_DELETE_HINT_USE_FILE_DESC) {
      if (!HgfsHandle2FileName(file, session, &cpName, &cpNameSize)) {
         LOG(4, ("%s: could not map cached file handle %u\n", __FUNCTION__,
                 file));

         return EBADF;
      }
      localName = cpName;

   } else {
      nameStatus = HgfsServerGetAccess(cpName, cpNameSize,
                                       HGFS_OPEN_MODE_WRITE_ONLY, caseFlags,
                                       &localName, NULL);
      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("%s: access check failed\n", __FUNCTION__));

         return HgfsConvertFromNameStatus(nameStatus);
      }
   }

   ASSERT(localName);

   LOG(4, ("%s: unlinking \"%s\"\n", __FUNCTION__, localName));
   error = Posix_Unlink(localName);
   free(localName);
   if (error) {
      error = errno;
      LOG(4, ("%s: error: %s\n", __FUNCTION__, strerror(error)));

      return error;
   }
   if (!HgfsPackDeleteReply(packetIn, 0, &packetOut, &packetOutSize)) {
      return EPROTO;
   }
   if (!HgfsPacketSend(packetOut, packetOutSize, session, 0)) {
      free(packetOut);
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerDeleteDir --
 *
 *    Handle a Delete Dir request.
 *
 *    Simply converts to the local filename, calls rmdir on the
 *    file, and returns an appropriate response to the driver.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerDeleteDir(char const *packetIn,     // IN: incoming packet
                    size_t packetSize,        // IN: size of packet
                    HgfsSessionInfo *session) // IN: session info
{
   HgfsNameStatus nameStatus;
   char *localName;
   int error;
   HgfsHandle file = HGFS_INVALID_HANDLE;
   HgfsDeleteHint hints = 0;
   char *cpName;
   size_t cpNameSize;
   uint32 caseFlags;
   char *packetOut;
   size_t packetOutSize;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackDeleteRequest(packetIn, packetSize, &cpName, &cpNameSize,
                                &hints, &file, &caseFlags)) {
      return EPROTO;
   }

   if (hints & HGFS_DELETE_HINT_USE_FILE_DESC) {
      if (!HgfsHandle2FileName(file, session, &cpName, &cpNameSize)) {
         LOG(4, ("%s: could not map cached file handle %u\n", __FUNCTION__,
            file));

         return EBADF;
      }
      localName = cpName;

   } else {
      nameStatus = HgfsServerGetAccess(cpName, cpNameSize,
                                       HGFS_OPEN_MODE_WRITE_ONLY,
                                       caseFlags, &localName, NULL);
      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("%s: access check failed\n", __FUNCTION__));

         return HgfsConvertFromNameStatus(nameStatus);
      }
   }

   /* Guest OS is not allowed to delete shared folder. */
   if (HgfsServerIsSharedFolderOnly(cpName, cpNameSize)){
      LOG(4, ("%s: Cannot delete shared folder\n", __FUNCTION__));

      return EPERM;
   }

   ASSERT(localName);

   LOG(4, ("%s: removing \"%s\"\n", __FUNCTION__, localName));
   error = Posix_Rmdir(localName);
   free(localName);
   if (error) {
      error = errno;
      LOG(4, ("%s: error: %s\n", __FUNCTION__, strerror(error)));

      return error;
   }

   if (!HgfsPackDeleteReply(packetIn, 0, &packetOut, &packetOutSize)) {
      return EPROTO;
   }
   if (!HgfsPacketSend(packetOut, packetOutSize, session, 0)) {
      free(packetOut);
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerRename --
 *
 *    Handle a Rename request.
 *
 *    Simply converts the new and old names to local filenames, calls
 *    rename(2), and returns an appropriate response to the driver.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerRename(char const *packetIn,     // IN: incoming packet
                 size_t packetSize,        // IN: size of packet
                 HgfsSessionInfo *session) // IN: session info
{
   HgfsNameStatus nameStatus;
   char *localOldName = NULL;
   char *localNewName = NULL;
   char *cpOldName;
   uint32 cpOldNameLen;
   char *cpNewName;
   uint32 cpNewNameLen;
   HgfsHandle srcFile = HGFS_INVALID_HANDLE;
   HgfsHandle targetFile = HGFS_INVALID_HANDLE;
   HgfsRenameHint hints = 0;
   int error;
   HgfsInternalStatus status;
   Bool sharedFolderOpen = FALSE;
   uint32 oldCaseFlags = 0;
   uint32 newCaseFlags = 0;
   char *packetOut;
   size_t packetOutSize;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackRenameRequest(packetIn, packetSize, &cpOldName, &cpOldNameLen,
                                &cpNewName, &cpNewNameLen, &hints, &srcFile,
                                &targetFile, &oldCaseFlags, &newCaseFlags)) {
      return EPROTO;
   }

   if (hints & HGFS_RENAME_HINT_USE_SRCFILE_DESC) {
      size_t localOldNameLen;

      if (!HgfsHandle2FileName(srcFile, session, &localOldName,
                               &localOldNameLen)) {
         LOG(4, ("%s: could not map cached source file handle %u\n",
                 __FUNCTION__, srcFile));

         return EBADF;
      }

      /* Guest OS is not allowed to rename shared folder. */
      if (HgfsHandleIsSharedFolderOpen(srcFile, session, &sharedFolderOpen) &&
          sharedFolderOpen) {
         LOG(4, ("%s: Cannot rename shared folder\n", __FUNCTION__));
         status = EPERM;
         goto exit;
      }

   } else {

      nameStatus = HgfsServerGetAccess(cpOldName, cpOldNameLen,
                                       HGFS_OPEN_MODE_READ_WRITE,
                                       oldCaseFlags, &localOldName, NULL);
      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("%s: old name access check failed\n", __FUNCTION__));

         return HgfsConvertFromNameStatus(nameStatus);
      }

      ASSERT(localOldName);

      /* Guest OS is not allowed to rename shared folder. */
      if (HgfsServerIsSharedFolderOnly(cpOldName, cpOldNameLen)) {
         LOG(4, ("%s: Cannot rename shared folder\n", __FUNCTION__));
         status = EPERM;
         goto exit;
      }
   }

   if (hints & HGFS_RENAME_HINT_USE_TARGETFILE_DESC) {
      size_t localNewNameLen;

      if (!HgfsHandle2FileName(targetFile, session, &localNewName,
                               &localNewNameLen)) {
         LOG(4, ("%s: could not map cached target file handle %u\n",
                 __FUNCTION__, targetFile));

         return EBADF;
      }

      /* Guest OS is not allowed to rename shared folder. */
      if (HgfsHandleIsSharedFolderOpen(targetFile, session, &sharedFolderOpen) &&
          sharedFolderOpen) {
         LOG(4, ("%s: Cannot rename shared folder\n", __FUNCTION__));
         status = EPERM;
         goto exit;
      }

   } else {

      nameStatus = HgfsServerGetAccess(cpNewName, cpNewNameLen,
                                       HGFS_OPEN_MODE_WRITE_ONLY,
                                       newCaseFlags, &localNewName, NULL);
      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("%s: new name access check failed\n", __FUNCTION__));
         status = HgfsConvertFromNameStatus(nameStatus);
         goto exit;
      }
   }

   ASSERT(localNewName);

   if (hints & HGFS_RENAME_HINT_NO_REPLACE_EXISTING) {
      HgfsFileAttrInfo attr;
      HgfsShareOptions configOptions;

      /* Get the config options. */
      nameStatus = HgfsServerPolicy_GetShareOptions(cpNewName, cpNewNameLen,
                                                    &configOptions);
      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, cpNewName));
         status = ENOENT;
         goto exit;
      }

      /*
       * We were asked to avoid replacing an existing file,
       * so fail if the target exists.
       */

      status = HgfsGetattrFromName(localNewName, configOptions, cpNewName,
                                   &attr, NULL);
      if (status == 0) {
         /* The target exists, and so must fail the rename. */
         LOG(4, ("%s: error: target %s exists\n", __FUNCTION__, localNewName));
         status = EEXIST;
         goto exit;
      }
   }

   LOG(4, ("%s: renaming \"%s\" to \"%s\"\n", __FUNCTION__,
           localOldName, localNewName));
   error = Posix_Rename(localOldName, localNewName);
   if (error) {
      error = errno;
      LOG(4, ("%s: error: %s\n", __FUNCTION__, strerror(error)));
      status = error;
      goto exit;
   }

   /*
    * Update all file nodes referring to this filename to the new name.
    *
    * XXX: Note that this operation can fail (out of memory), but we'd like
    * the client to see success anyway, because the rename succeeded.
    */

   status = 0;
   HgfsUpdateNodeNames(localOldName, localNewName, session);
   if (!HgfsPackRenameReply(packetIn, status, &packetOut, &packetOutSize)) {
      status = EPROTO;
      goto exit;
   }
   if (!HgfsPacketSend(packetOut, packetOutSize, session, 0)) {
      free(packetOut);
   }

  exit:
   free(localOldName);
   free(localNewName);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerQueryVolume --
 *
 *    Handle a Query Volume request.
 *
 *    Right now we only handle the volume space request. Call Wiper library
 *    to get the volume information.
 *    It is possible that shared folders can belong to different volumes on
 *    the server. If this is the case, default to return the space information
 *    of the volume that has the least amount of the available space, but it's
 *    configurable with a config option (tools.hgfs.volumeInfoType). 2 possible
 *    options, min and max.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerQueryVolume(char const *packetIn,     // IN: incoming packet
                      size_t packetSize,        // IN: size of packet
                      HgfsSessionInfo *session) // IN: session info
{
   HgfsRequest *header;
   uint32 extra;
   char *utf8Name = NULL;
   size_t utf8NameLen;
   HgfsHandle handle;
   int offset = 0;
   uint64 outFreeBytes = 0;
   uint64 outTotalBytes = 0;
   VolumeInfoType infoType;
   DirectoryEntry *dent;
   HgfsInternalStatus status;
   Bool firstShare = TRUE;
   Bool success;
   HgfsNameStatus nameStatus;
   size_t failed = 0;
   size_t shares = 0;
   HgfsInternalStatus firstErr = 0;
   char *fileName;
   uint32 fileNameLength;
   uint32 caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   uint64 *freeBytes;
   uint64 *totalBytes;
   char *packetOut;
   size_t packetOutSize;

   ASSERT(packetIn);
   ASSERT(session);

   header = (HgfsRequest *)packetIn;

   if (header->op == HGFS_OP_QUERY_VOLUME_INFO_V3) {
      HgfsRequestQueryVolumeV3 *requestV3 =
                        (HgfsRequestQueryVolumeV3 *)HGFS_REQ_GET_PAYLOAD_V3(packetIn);
      HgfsReplyQueryVolumeV3 *replyV3;

      packetOutSize = HGFS_REP_PAYLOAD_SIZE_V3(replyV3);
      packetOut = Util_SafeMalloc(packetOutSize);
      replyV3 = (HgfsReplyQueryVolumeV3 *)HGFS_REP_GET_PAYLOAD_V3(packetOut);

      /*
       * We don't yet support file handle for this operation.
       * Clients should retry using the file name.
       */

      if (requestV3->fileName.flags & HGFS_FILE_NAME_USE_FILE_DESC) {
         LOG(4, ("%s: Doesn't support file handle.\n", __FUNCTION__));
         status = EPARAMETERNOTSUPPORTED;
	 goto exit;
      }

      freeBytes = &replyV3->freeBytes;
      totalBytes = &replyV3->totalBytes;
      replyV3->reserved = 0;

      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= HGFS_REQ_PAYLOAD_SIZE_V3(requestV3));
      extra = packetSize - HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);

      caseFlags = requestV3->fileName.caseType;
      fileName = requestV3->fileName.name;
      fileNameLength = requestV3->fileName.length;
      LOG(4, ("%s: HGFS_OP_QUERY_VOLUME_INFO_V3\n", __FUNCTION__));
   } else {
      HgfsRequestQueryVolume *request = (HgfsRequestQueryVolume *)packetIn;
      HgfsReplyQueryVolume *reply;

      packetOutSize = sizeof *reply;
      packetOut = Util_SafeMalloc(packetOutSize);
      reply = (HgfsReplyQueryVolume *)packetOut;

      freeBytes = &reply->freeBytes;
      totalBytes = &reply->totalBytes;

      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *request);
      extra = packetSize - sizeof *request;

      fileName = request->fileName.name;
      fileNameLength = request->fileName.length;
   }

   /*
    * request->fileName.length is user-provided, so this test must be carefully
    * written to prevent wraparounds.
    */

   if (fileNameLength > extra) {
      /* The input packet is smaller than the request. */
      status = EPROTO;
      goto exit;
   }

   /* It is now safe to read the file name field. */
   nameStatus = HgfsServerGetAccess(fileName, fileNameLength,
                                    HGFS_OPEN_MODE_READ_WRITE,
                                    caseFlags, &utf8Name, &utf8NameLen);
   switch (nameStatus) {
   case HGFS_NAME_STATUS_INCOMPLETE_BASE:
      /*
       * This is the base of our namespace. Clients can request a
       * QueryVolumeInfo on it, on individual shares, or on just about
       * any pathname.
       */

      LOG(4,("%s: opened search on base\n", __FUNCTION__));
      status = HgfsServerSearchVirtualDir(HgfsServerPolicy_GetShares,
                                          HgfsServerPolicy_GetSharesInit,
                                          HgfsServerPolicy_GetSharesCleanup,
                                          DIRECTORY_SEARCH_TYPE_BASE,
                                          session,
                                          &handle);
      if (status != 0) {
         goto exit;
      }

      /*
       * If we're outside the Tools, find out if we're to compute the minimum
       * values across all shares, or the maximum values.
       */
      infoType = VOLUME_INFO_TYPE_MIN;
#ifndef VMX86_TOOLS
      {
         char *volumeInfoType = Config_GetString("min",
                                                 "tools.hgfs.volumeInfoType");
         if (!Str_Strcasecmp(volumeInfoType, "max")) {
            infoType = VOLUME_INFO_TYPE_MAX;
         }
         free(volumeInfoType);
      }
#endif

      /*
       * Now go through all shares and get share paths on the server.
       * Then retrieve space info for each share's volume.
       */

      offset = 0;
      while ((dent = HgfsGetSearchResult(handle, session, offset,
                                         TRUE)) != NULL) {
         char const *sharePath;
         size_t sharePathLen;
         uint64 freeBytes  = 0;
         uint64 totalBytes = 0;
         unsigned int length;

         length = strlen(dent->d_name);

         /*
          * Now that the server is passing '.' and ".." around as dents, we
          * need to make sure to handle them properly. In particular, they
          * should be ignored within QueryVolume, as they're not real shares.
          */

         if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
            LOG(4, ("%s: Skipping fake share %s\n", __FUNCTION__, 
                    dent->d_name));
            free(dent);
            continue;
         }

         /*
          * The above check ignores '.' and '..' so we do not include them in
          * the share count here.
          */

         shares++;

         /*
          * Check permission on the share and get the share path.  It is not
          * fatal if these do not succeed.  Instead we ignore the failures
          * (apart from logging them) until we have processed all shares.  Only
          * then do we check if there were any failures; if all shares failed
          * to process then we bail out with an error code.
          */

         nameStatus = HgfsServerPolicy_GetSharePath(dent->d_name, length,
                                                    HGFS_OPEN_MODE_READ_ONLY,
                                                    &sharePathLen,
                                                    &sharePath);
         free(dent);
         if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
            LOG(4, ("%s: No such share or access denied\n", __FUNCTION__));
            if (0 == firstErr) {
               firstErr = HgfsConvertFromNameStatus(nameStatus);
            }
            failed++;
            continue;
         }

         if (!HgfsServerStatFs(sharePath, sharePathLen,
                               &freeBytes, &totalBytes)) {
            LOG(4, ("%s: error getting volume information\n",
                    __FUNCTION__));
            if (0 == firstErr) {
               firstErr = EIO;
            }
            failed++;
            continue;
         }

         /*
          * Pick the drive with amount of space available and return that
          * according to different volume info type.
          */

         switch (infoType) {
         case VOLUME_INFO_TYPE_MIN:
            if ((outFreeBytes > freeBytes) || firstShare) {
               firstShare = FALSE;
               outFreeBytes  = freeBytes;
               outTotalBytes = totalBytes;
            }
            break;
         case VOLUME_INFO_TYPE_MAX:
            if ((outFreeBytes < freeBytes)) {
               outFreeBytes  = freeBytes;
               outTotalBytes = totalBytes;
            }
            break;
         default:
            NOT_IMPLEMENTED();
         }
      }
      if (!HgfsRemoveSearch(handle, session)) {
         LOG(4, ("%s: could not close search on base\n", __FUNCTION__));
      }
      break;
   case HGFS_NAME_STATUS_COMPLETE:
      ASSERT(utf8Name);
      LOG(4,("%s: querying path %s\n", __FUNCTION__, utf8Name));
      success = HgfsServerStatFs(utf8Name, utf8NameLen,
                                 &outFreeBytes, &outTotalBytes);
      free(utf8Name);
      if (!success) {
         LOG(4, ("%s: error getting volume information\n", __FUNCTION__));
         status = EIO;
         goto exit;
      }
      break;
   default:
      LOG(4,("%s: file access check failed\n", __FUNCTION__));
      status = HgfsConvertFromNameStatus(nameStatus);
      goto exit;
   }

   *freeBytes  = outFreeBytes;
   *totalBytes = outTotalBytes;
   status = 0;

   if (!HgfsPackAndSendPacket(packetOut, packetOutSize, status, header->id,
                              session, 0)) {
      goto exit;
   }
   return status;

exit:
   free(packetOut);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSymlinkCreate --
 *
 *    Handle a SymlinkCreate request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerSymlinkCreate(char const *packetIn,     // IN: incoming packet
                        size_t packetSize,        // IN: size of packet
                        HgfsSessionInfo *session) // IN: session info
{
   HgfsRequest *header;
   uint32 extra;
   char *localSymlinkName;
   char localTargetName[HGFS_PACKET_MAX];
   int error;
   HgfsNameStatus nameStatus;
   uint32 caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   char *symlinkName;
   uint32 symlinkNameLength;
   char *targetName;
   uint32 targetNameLength;
   HgfsShareOptions configOptions;
   char *packetOut;
   size_t packetOutSize;
   HgfsInternalStatus status = 0;

   ASSERT(packetIn);
   ASSERT(session);
   header = (HgfsRequest *)packetIn;

   if (header->op == HGFS_OP_CREATE_SYMLINK_V3) {
      HgfsRequestSymlinkCreateV3 *requestV3;
      HgfsReplySymlinkCreateV3 *replyV3;
      HgfsFileNameV3 *targetNameP;
      requestV3 = (HgfsRequestSymlinkCreateV3 *)HGFS_REQ_GET_PAYLOAD_V3(packetIn);
      LOG(4, ("%s: HGFS_OP_CREATE_SYMLINK_V3\n", __FUNCTION__));

      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= HGFS_REQ_PAYLOAD_SIZE_V3(requestV3));
      extra = packetSize - HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);

      caseFlags = requestV3->symlinkName.caseType;
      symlinkName = requestV3->symlinkName.name;
      symlinkNameLength = requestV3->symlinkName.length;

      /*
       * targetName starts after symlinkName + the variable length array
       * in symlinkName.
       */

      targetNameP = (HgfsFileNameV3 *)(symlinkName + 1 + symlinkNameLength);
      targetName = targetNameP->name;
      targetNameLength = targetNameP->length;

      /*
       * We don't yet support file handle for this operation.
       * Clients should retry using the file name.
       */

      if (requestV3->symlinkName.flags & HGFS_FILE_NAME_USE_FILE_DESC ||
          targetNameP->flags & HGFS_FILE_NAME_USE_FILE_DESC) {
		 LOG(4, ("%s: Doesn't support file handle.\n", __FUNCTION__));

		 return EPARAMETERNOTSUPPORTED;
	      }

	      packetOutSize = HGFS_REP_PAYLOAD_SIZE_V3(replyV3);
	      packetOut = Util_SafeMalloc(packetOutSize);
	      replyV3 = (HgfsReplySymlinkCreateV3 *)HGFS_REP_GET_PAYLOAD_V3(packetOut);
	      replyV3->reserved = 0;
	   } else {
	      HgfsRequestSymlinkCreate *request;
	      HgfsFileName *targetNameP;
	      request = (HgfsRequestSymlinkCreate *)packetIn;

	      /* Enforced by the dispatch function. */
	      ASSERT(packetSize >= sizeof *request);
	      extra = packetSize - sizeof *request;

	      symlinkName = request->symlinkName.name;
	      symlinkNameLength = request->symlinkName.length;

	      /*
	       * targetName starts after symlinkName + the variable length array
	       * in symlinkName.
	       */

	      targetNameP = (HgfsFileName *)(symlinkName + 1 + symlinkNameLength);
	      targetName = targetNameP->name;
	      targetNameLength = targetNameP->length;
	      packetOutSize = sizeof(struct HgfsReplySymlinkCreate);
	      packetOut = Util_SafeMalloc(packetOutSize);
	   }

	   /*
	    * request->symlinkName.length is user-provided, so this test must
	    * be carefully written to prevent wraparounds.
	    */

	   if (symlinkNameLength > extra) {
	      /* The input packet is smaller than the request */
	      status = EPROTO;
	      goto exit;
	   }

	   /*
	    * It is now safe to read the symlink file name and the
	    * "targetName" field
	    */

	   nameStatus = HgfsServerGetAccess(symlinkName, symlinkNameLength,
					    HGFS_OPEN_MODE_READ_WRITE,
					    caseFlags, &localSymlinkName,
					    NULL);
	   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
	      LOG(4, ("%s: symlink name access check failed\n",
                      __FUNCTION__));
      status = HgfsConvertFromNameStatus(nameStatus);
      goto exit;
   }

   ASSERT(localSymlinkName);
   extra -= symlinkNameLength;

   /*
    * targetNameLength is user-provided, so this test must be carefully
    * written to prevent wraparounds.
    */

   if (targetNameLength > extra) {
      /* The input packet is smaller than the request */
      free(localSymlinkName);
      status = EPROTO;
      goto exit;
   }

   /* It is now safe to read the target file name */

   /* Get the config options. */
   nameStatus = HgfsServerPolicy_GetShareOptions(symlinkName, symlinkNameLength,
                                                 &configOptions);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, symlinkName));
      status = HgfsConvertFromNameStatus(nameStatus);
      goto exit;
   }

   /* Convert from CPName-lite to normal and NUL-terminate. */
   memcpy(localTargetName, targetName, targetNameLength);
   CPNameLite_ConvertFrom(localTargetName, targetNameLength, DIRSEPC);
   localTargetName[targetNameLength] = '\0';

   /* Prohibit symlink ceation if symlink following is enabled. */
   if (HgfsServerPolicy_IsShareOptionSet(configOptions,
                                         HGFS_SHARE_FOLLOW_SYMLINKS)) {
      status = EPERM;
      goto exit;
   }

   LOG(4, ("%s: creating \"%s\" linked to \"%s\"\n", __FUNCTION__,
           localSymlinkName, localTargetName));

   /* XXX: Should make use of targetNameP->flags? */
   error = Posix_Symlink(localTargetName, localSymlinkName);
   free(localSymlinkName);
   if (error) {
      status = errno;
      LOG(4, ("%s: error: %s\n", __FUNCTION__, strerror(errno)));
      goto exit;
   }

   status = 0;
   if (!HgfsPackAndSendPacket(packetOut, packetOutSize, status, header->id,
                              session, 0)) {
      goto exit;
   }

   return status;

exit:
   free(packetOut);

   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsServerHasSymlink --
 *
 *      This function determines if any of the intermediate components of the
 *      fileName makes references outside the actual shared path. We do not
 *      check for the last component as none of the server operations follow
 *      symlinks. Also some ops that call us expect to operate on a symlink
 *      final component.
 *
 *      We use following algorithm. It takes 2 parameters, sharePath and
 *      fileName, and returns non-zero errno if fileName makes an invalid
 *      reference. The idea is to resolve both the sharePath and parent
 *      directory of the fileName. The sharePath is already resolved
 *      beforehand in HgfsServerPolicyRead. During resolution, we eliminate
 *      all the ".", "..", and symlinks handled by the realpath(3) libc call.
 *
 *      We use parent because last component could be a symlink or a component
 *      that doesn't exist. After resolving, we determine if sharePath is a
 *      prefix of fileName.
 *
 *      Note that realpath(3) behaves differently on GNU and BSD systems.
 *      Following table lists the difference:
 *
 *                                  GNU realpath          BSD realpath
 *                            -----------------------  -----------------------
 *
 *      "/tmp/existingFile"   "/tmp/existingFile" (0)  "/tmp/existingFile" (0)
 *       "/tmp/missingFile"   NULL           (ENOENT)  "/tmp/missingFile"  (0)
 *        "/missingDir/foo"   NULL           (ENOENT)  NULL           (ENOENT)
 *              In /tmp, ""   NULL           (ENOENT)  "/tmp"              (0)
 *             In /tmp, "."   "/tmp"              (0)  "/tmp"              (0)
 *
 * Results:
 *      HGFS_NAME_STATUS_COMPLETE if the given path has a symlink,
        an appropriate name status error otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

HgfsNameStatus
HgfsServerHasSymlink(const char *fileName,	// IN
                     size_t fileNameLength,     // IN
                     const char *sharePath,	// IN
                     size_t sharePathLength)    // IN
{
   char *resolvedFileDirPath = NULL;
   char *fileDirName = NULL;
   HgfsInternalStatus status;
   HgfsNameStatus nameStatus = HGFS_NAME_STATUS_COMPLETE;

   ASSERT(fileName);
   ASSERT(sharePath);
   ASSERT(sharePathLength <= fileNameLength);

   LOG(4, ("%s: fileName: %s, sharePath: %s#\n", __FUNCTION__,
           fileName, sharePath));

   /*
    * Return success if:
    * - empty fileName or
    * - sharePath is empty (this is for special root share that allows
    *   access to entire host) or
    * - fileName and sharePath are same.
    */

   if (fileNameLength == 0 ||
       sharePathLength == 0 ||
       Str_Strcmp(sharePath, fileName) == 0) {
      goto exit;
   }

   /* Separate out parent directory of the fileName. */
   File_GetPathName(fileName, &fileDirName, NULL);

   /*
    * File_GetPathName may return an empty string to signify the root of
    * the filesystem. To simplify subsequent processing, let's convert such
    * empty strings to "/" when found. See File_GetPathName header comment
    * for details.
    */

   if (strlen(fileDirName) == 0) {
      char *p;
      p = realloc(fileDirName, sizeof (DIRSEPS));
      if (p == NULL) {
         nameStatus = HGFS_NAME_STATUS_OUT_OF_MEMORY;
         LOG(4, ("%s: failed to realloc fileDirName.\n", __FUNCTION__));
         goto exit;
      } else {
         fileDirName = p;
         Str_Strcpy(fileDirName, DIRSEPS, sizeof (DIRSEPS));
      }
   }

   /*
    * Resolve parent directory of fileName.
    * Use realpath(2) to resolve the parent.
    */

   resolvedFileDirPath = Posix_RealPath(fileDirName);
   if (resolvedFileDirPath == NULL) {
      /* Let's return some meaningful errors if possible. */
      status = errno;
      switch (status) {
         case ENOENT:
            nameStatus = HGFS_NAME_STATUS_DOES_NOT_EXIST;
            break;
         case ENOTDIR:
            nameStatus = HGFS_NAME_STATUS_NOT_A_DIRECTORY;
            break;
         default:
            nameStatus = HGFS_NAME_STATUS_FAILURE;
            break;
      }
      LOG(4, ("%s: realpath failed: fileDirName: %s: %s\n",
              __FUNCTION__, fileDirName, strerror(errno)));
      goto exit;
   }

   /* Resolved parent should match with the shareName. */
   if (Str_Strncmp(sharePath, resolvedFileDirPath, sharePathLength) != 0) {
      nameStatus = HGFS_NAME_STATUS_ACCESS_DENIED;
      LOG(4, ("%s: resolved parent do not match, parent: %s, resolved: %s#\n",
              __FUNCTION__, fileDirName, resolvedFileDirPath));
      goto exit;
   }

exit:
   free(resolvedFileDirPath);
   free(fileDirName);

   return nameStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerServerLockChange --
 *
 *    Called by the client when it wants to either acquire an oplock on a file
 *    that was previously opened, or when it wants to release/downgrade an
 *    oplock on a file that was previously oplocked.
 *
 * Results:
 *    EOPNOTSUPP, because this is unimplemented.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerServerLockChange(char const *packetIn,     // IN: incoming packet
                           size_t packetSize,        // IN: size of packet
                           HgfsSessionInfo *session) // IN: session info
{
   return EOPNOTSUPP;
}


#ifdef HGFS_OPLOCKS
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAckOplockBreak --
 *
 *    Platform-dependent implementation of oplock break acknowledgement.
 *    This function gets called when the oplock break rpc command is completed.
 *    The rpc oplock break command (HgfsServerOplockBreak) is in hgfsServer.c
 *
 *    On Linux, we use fcntl() to downgrade the lease. Then we update the node
 *    cache, free the clientData, and call it a day.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsAckOplockBreak(ServerLockData *lockData, // IN: server lock info
                   HgfsServerLock replyLock) // IN: client has this lock
{
   int fileDesc, newLock;
   HgfsServerLock actualLock;

   ASSERT(lockData);
   fileDesc = lockData->fileDesc;
   LOG(4, ("%s: Acknowledging break on fd %d\n", __FUNCTION__, fileDesc));

   /*
    * The Linux server supports lock downgrading. We only downgrade to a shared
    * lock if our previous call to fcntl() said we could, and if the client
    * wants to downgrade to a shared lock. Otherwise, we break altogether.
    */

   if (lockData->serverLock == HGFS_LOCK_SHARED &&
       replyLock == HGFS_LOCK_SHARED) {
      newLock = F_RDLCK;
      actualLock = replyLock;
   } else {
      newLock = F_UNLCK;
      actualLock = HGFS_LOCK_NONE;
   }

   /* Downgrade or acknowledge the break altogether. */
   if (fcntl(fileDesc, F_SETLEASE, newLock) == -1) {
      int error = errno;
      Log("%s: Could not break lease on fd %d: %s\n",
          __FUNCTION__, fileDesc, strerror(error));
   }

   /* Cleanup. */
   HgfsUpdateNodeServerLock(fileDesc, actualLock);
   free(lockData);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsShareRoot --
 *
 *    Checks if the cpName represents the root directory for a share.
 *    Components in CPName format are separated by NUL characters.
 *    CPName for the root of a share contains only one component thus
 *    it does not have any embedded '\0' characters in the name.
 *
 * Results:
 *    TRUE if it is the root directory, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsIsShareRoot(char const *cpName,         // IN: name to test
                size_t cpNameSize)          // IN: length of the name
{
   size_t i;
   for (i = 0; i < cpNameSize; i++) {
      if (cpName[i] == '\0') {
         return FALSE;
      }
   }
   return TRUE;
}

#if defined(__APPLE__)
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetHiddenXattr --
 *
 *    For Mac hosts returns true if file has invisible bit set in the FileFinder
 *    extended attributes.
 *
 * Results:
 *    0 if succeeded getting attribute, error code otherwise otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsGetHiddenXAttr(char const *fileName,   // IN: File name
                   Bool *attribute)        // OUT: Hidden atribute
{
   struct attrlist attrList;
   struct FInfoAttrBuf attrBuf;
   HgfsInternalStatus err;

   ASSERT(fileName);
   ASSERT(attribute);

   memset(&attrList, 0, sizeof attrList);
   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.commonattr = ATTR_CMN_OBJTYPE | ATTR_CMN_FNDRINFO;
   err = getattrlist(fileName, &attrList, &attrBuf, sizeof attrBuf, 0);
   if (err == 0) {
      switch (attrBuf.objType) {
      case VREG: {
         FileInfo *info = (FileInfo*) attrBuf.finderInfo;
         uint16 finderFlags = CFSwapInt16BigToHost(info->finderFlags);
         *attribute = (finderFlags & kIsInvisible) != 0;
         break;
      }
      case VDIR: {
         FolderInfo *info = (FolderInfo*) attrBuf.finderInfo;
         uint16 finderFlags = CFSwapInt16BigToHost(info->finderFlags);
         *attribute = (finderFlags & kIsInvisible) != 0;
         break;
      }
      default:
         LOG(4, ("%s: Unrecognized object type %d\n", __FUNCTION__,
                 attrBuf.objType));
         err = EINVAL;
      }
   } else {
      LOG(4, ("%s: Error %d when getting attributes\n", __FUNCTION__, err));
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ChangeInvisibleFlag --
 *
 *    Changes value of the invisible bit in a flags variable to a value defined
 *    by setFlag parameter.
 *
 * Results:
 *    TRUE flag has been changed, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ChangeInvisibleFlag(uint16 *flags,           // IN: variable that contains flags
                    Bool setFlag)            // IN: new value for the invisible flag
{
   Bool changed = FALSE;
   /*
    * Finder keeps, reports and expects to set flags in big endian format.
    * Needs to convert to host endian before using constants
    * and then convert back to big endian before saving
    */
   uint16 finderFlags = CFSwapInt16BigToHost(*flags);
   Bool isInvisible = (finderFlags & kIsInvisible) != 0;
   if (setFlag != isInvisible) {
      if (setFlag) {
         finderFlags |= kIsInvisible;
      } else {
         finderFlags &= ~kIsInvisible;
      }
      *flags = CFSwapInt16HostToBig(finderFlags);
      changed = TRUE;
   }
   return changed;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSetHiddenXAttr --
 *
 *    Sets new value for the invisible attribute of a file.
 *
 * Results:
 *    0 if succeeded, error code otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsSetHiddenXAttr(char const *fileName,       // IN: path to the file
                   Bool value)                 // IN: new value to the invisible attribute
{
   HgfsInternalStatus err;
   Bool changed = FALSE;
   struct attrlist attrList;
   struct FInfoAttrBuf attrBuf;

   ASSERT(fileName);

   memset(&attrList, 0, sizeof attrList);
   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.commonattr = ATTR_CMN_OBJTYPE | ATTR_CMN_FNDRINFO;
   err = getattrlist(fileName, &attrList, &attrBuf, sizeof attrBuf, 0);
   if (err == 0) {
      switch (attrBuf.objType) {
      case VREG: {
         FileInfo *info = (FileInfo*) attrBuf.finderInfo;
         changed = ChangeInvisibleFlag(&info->finderFlags, value);
         break;
      }
      case VDIR: {
         FolderInfo *info = (FolderInfo*) attrBuf.finderInfo;
         changed = ChangeInvisibleFlag(&info->finderFlags, value);
         break;
      }
      default:
         LOG(4, ("%s: Unrecognized object type %d\n", __FUNCTION__,
                 attrBuf.objType));
         err = EINVAL;
      }
   } else {
      err = errno;
   }
   if (changed) {
      attrList.commonattr = ATTR_CMN_FNDRINFO;
      err = setattrlist(fileName, &attrList, attrBuf.finderInfo,
                        sizeof attrBuf.finderInfo, 0);
   }
   return err;
}

#else // __APPLE__

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetHiddenXAttr --
 *
 *    Always returns EINVAL since there is no support for invisible files in Linux
 *    HGFS server.
 *
 * Results:
 *    Currently always returns EINVAL.  Will return 0 when support for invisible files
 *    is implemented in Linux server.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsGetHiddenXAttr(char const *fileName,    // IN: File name
                   Bool *attribute)         // OUT: Value of the hidden attribute
{
   return EINVAL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSetHiddenXAttr --
 *
 *    Sets new value for the invisible attribute of a file.
 *    Currently Linux server does not support invisible or hiddden files thus
 *    the function fails when a attempt to mark a file as hidden is made.
 *
 * Results:
 *    0 if succeeded, error code otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsSetHiddenXAttr(char const *fileName,   // IN: File name
                   Bool value)             // IN: Value of the attribute to set
{
   return value ? EINVAL : 0;
}
#endif // __APPLE__
