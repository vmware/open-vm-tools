/*********************************************************
 * Copyright (C) 2005-2019 VMware, Inc. All rights reserved.
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
 * dndLinux.c --
 *
 *     Some common dnd functions for UNIX guests and hosts.
 */


#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "vmware.h"
#include "dndInt.h"
#include "err.h"
#include "dnd.h"
#include "posix.h"
#include "file.h"
#include "strutil.h"
#include "vm_assert.h"
#include "util.h"
#include "escape.h"
#include "su.h"
#if defined(__linux__) || defined(sun) || defined(__FreeBSD__)
#include "vmblock_user.h"
#include "mntinfo.h"
#endif

#define LOGLEVEL_MODULE dnd
#include "loglevel_user.h"

#include "unicodeOperations.h"


#define DND_ROOTDIR_PERMS     (S_IRWXU | S_IRWXG | S_IRWXO)
#define DND_STAGINGDIR_PERMS  (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#ifdef sun
#define ACCESSPERMS           (S_IRWXU | S_IRWXG | S_IRWXO)
#endif
#ifdef __ANDROID__
/*
 * Android doesn't support setmntent(), endmntent() or MOUNTED.
 */
#define NO_SETMNTENT
#define NO_ENDMNTENT
#define ACCESSPERMS           (S_IRWXU | S_IRWXG | S_IRWXO)
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_GetFileRoot --
 *
 *       Gets the root path of the staging directory for DnD file transfers.
 *
 * Results:
 *       The path to the staging directory.
 *
 * Side effects:
 *	 None
 *
 *-----------------------------------------------------------------------------
 */

const char *
DnD_GetFileRoot(void)
{
   return "/tmp/VMwareDnD/";
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDUriListGetFile --
 *
 *    Retrieves the filename and length from the file scheme (file://) entry at
 *    the specified index in a text/uri-list string.
 *
 * Results:
 *    The address of the beginning of the next filename on success, NULL if
 *    there are no more entries or on error.  index is updated with the
 *    location of the next entry in the list, and length is updated with the
 *    size of the filename starting at the returned value.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static char *
DnDUriListGetFile(char const *uriList,  // IN    : text/uri-list string
                  size_t *index,        // IN/OUT: current index
                  size_t *length)       // OUT   : length of returned string
{
   char const *nameStart;
   char const *nameEnd;
   char const *curr;

   ASSERT(uriList);
   ASSERT(index);
   ASSERT(length);

   /* The common case on the last entry */
   if (uriList[*index] == '\0') {
      return NULL;
   }

   /*
    * Ensure the URI list is formatted properly.  This is ugly, but we have to
    * special case for KDE (which doesn't follow the standard).
    *
    * XXX Note that this assumes we only support dropping files, based on the
    * definition of the macro that is used.
    */

   nameStart = &uriList[*index];

   if (strncmp(nameStart,
               DND_URI_LIST_PRE,
               sizeof DND_URI_LIST_PRE - 1) == 0) {
      nameStart += sizeof DND_URI_LIST_PRE - 1;
   } else if (strncmp(nameStart,
                      DND_URI_LIST_PRE_KDE,
                      sizeof DND_URI_LIST_PRE_KDE - 1) == 0) {
      nameStart += sizeof DND_URI_LIST_PRE_KDE - 1;
#if defined(__linux__)
   } else if (DnD_UriIsNonFileSchemes(nameStart)) {
      /* Do nothing. */
#endif
   } else {
      Warning("%s: the URI list did not begin with %s or %s\n", __func__,
              DND_URI_LIST_PRE, DND_URI_LIST_PRE_KDE);

      return NULL;
    }

   nameEnd = NULL;

   /* Walk the filename looking for the end */
   curr = nameStart;
   while (*curr != '\0' && *curr != '\r' && *curr != '\n') {
      curr++;
   }

   nameEnd = curr - 1;

   /* Bump curr based on trailing newline characters found */
   while (*curr == '\r' || *curr == '\n') {
      curr++;
   }

   *index = curr - uriList;
   *length = nameEnd - nameStart + 1;

   return (char *)nameStart;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_UriListGetNextFile --
 *
 *    Retrieves and unescapes the next file from the provided test/uri-list
 *    mime type string.  The index provided is used to iteratively retrieve
 *    successive files from the list.
 *
 * Results:
 *    An allocated, unescaped, NUL-terminated string containing the filename
 *    within the uri-list after the specified index.  index is updated to point
 *    to the next entry of the uri-list, and length (if provided) is set to the
 *    length of the allocated string (not including the NUL terminator).
 *    NULL if there are no more entries in the list, or on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

char *
DnD_UriListGetNextFile(char const *uriList,  // IN    : text/uri-list string
                       size_t *index,        // IN/OUT: current index
                       size_t *length)       // OUT   : length of returned string
{
   char const *file;
   size_t nextIndex;
   size_t fileLength = 0;
   char *unescapedName;
   size_t unescapedLength;

   ASSERT(uriList);
   ASSERT(index);

   nextIndex = *index;

   /* Get pointer to and length of next filename */
   file = DnDUriListGetFile(uriList, &nextIndex, &fileLength);
   if (!file) {
      return NULL;
   }

   /*
    * Retrieve an allocated, unescaped name.  This undoes the ' ' -> "%20"
    * escaping as required by RFC 1630 for entries in a uri-list.
    */

   unescapedName = Escape_Undo('%', file, fileLength, &unescapedLength);
   if (!unescapedName) {
      Warning("%s: error unescaping filename\n", __func__);

      return NULL;
   }

   *index = nextIndex;
   if (length) {
      *length = unescapedLength;
   }

   return unescapedName;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_UriIsNonFileSchemes --
 *
 *    Check if the uri contains supported non-file scheme.
 *
 * Results:
 *    TRUE if the uri contains supported non-file scheme. FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnD_UriIsNonFileSchemes(const char *uri)
{
   const char *schemes[] = DND_URI_NON_FILE_SCHEMES;
   int i = 0;

   while (schemes[i] != NULL) {
      if (strncmp(uri,
                  schemes[i],
                  strlen(schemes[i])) == 0) {
         return TRUE;
      }
      i++;
   }
   return FALSE;
}


/* We need to make this suck less. */
#if defined(__linux__) || defined(sun) || defined(__FreeBSD__)

/*
 *----------------------------------------------------------------------------
 *
 * DnD_AddBlockLegacy --
 *
 *    Adds a block to blockPath.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    Processes trying to access this path will block until DnD_RemoveBlock
 *    is called.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnD_AddBlockLegacy(int blockFd,                    // IN
                   const char *blockPath)          // IN
{
   LOG(1, "%s: placing block on %s\n", __func__, blockPath);

   ASSERT(blockFd >= 0);

   if (VMBLOCK_CONTROL(blockFd, VMBLOCK_ADD_FILEBLOCK, blockPath) != 0) {
      LOG(1, "%s: Cannot add block on %s (%s)\n",
          __func__, blockPath, Err_Errno2String(errno));

      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_RemoveBlockLegacy --
 *
 *    Removes block on blockedPath.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    Processes blocked on accessing this path will continue.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnD_RemoveBlockLegacy(int blockFd,                    // IN
                      const char *blockedPath)        // IN
{
   LOG(1, "%s: removing block on %s\n", __func__, blockedPath);

   if (blockFd >= 0) {
      if (VMBLOCK_CONTROL(blockFd, VMBLOCK_DEL_FILEBLOCK, blockedPath) != 0) {
         Log("%s: Cannot delete block on %s (%s)\n",
             __func__, blockedPath, Err_Errno2String(errno));

         return FALSE;
      }
   } else {
      LOG(4, "%s: Could not remove block on %s: "
          "fd to vmblock no longer exists.\n", __func__, blockedPath);
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_CheckBlockLegacy --
 *
 *    Verifies that given file descriptor is truly a control file of
 *    kernel-based vmblock implementation. Just a stub, for now at
 *    least since we don't have a good way to check.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
DnD_CheckBlockLegacy(int blockFd)                    // IN
{
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_AddBlockFuse --
 *
 *    Adds a block to blockPath.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    Processes trying to access this path will block until DnD_RemoveBlock
 *    is called.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnD_AddBlockFuse(int blockFd,                    // IN
                 const char *blockPath)          // IN
{
   LOG(1, "%s: placing block on %s\n", __func__, blockPath);

   ASSERT(blockFd >= 0);

   if (VMBLOCK_CONTROL_FUSE(blockFd, VMBLOCK_FUSE_ADD_FILEBLOCK,
                            blockPath) != 0) {
      LOG(1, "%s: Cannot add block on %s (%s)\n",
          __func__, blockPath, Err_Errno2String(errno));

      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_RemoveBlockFuse --
 *
 *    Removes block on blockedPath.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    Processes blocked on accessing this path will continue.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnD_RemoveBlockFuse(int blockFd,                    // IN
                    const char *blockedPath)        // IN
{
   LOG(1, "%s: removing block on %s\n", __func__, blockedPath);

   if (blockFd >= 0) {
      if (VMBLOCK_CONTROL_FUSE(blockFd, VMBLOCK_FUSE_DEL_FILEBLOCK,
                               blockedPath) != 0) {
         Log("%s: Cannot delete block on %s (%s)\n",
             __func__, blockedPath, Err_Errno2String(errno));

         return FALSE;
      }
   } else {
      LOG(4, "%s: Could not remove block on %s: "
          "fd to vmblock no longer exists.\n", __func__, blockedPath);
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_CheckBlockFuse --
 *
 *    Verifies that given file descriptor is truly a control file of
 *    FUSE-based vmblock implementation.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
DnD_CheckBlockFuse(int blockFd)                    // IN
{
   char buf[sizeof(VMBLOCK_FUSE_READ_RESPONSE)];
   ssize_t size;

   size = read(blockFd, buf, sizeof(VMBLOCK_FUSE_READ_RESPONSE));
   if (size < 0) {
      LOG(4, "%s: read failed, error %s.\n", __func__, Err_Errno2String(errno));

      return FALSE;
   }

   if (size != sizeof(VMBLOCK_FUSE_READ_RESPONSE)) {
      /*
       * Refer to bug 817761 of casting size to size_t.
       */
      LOG(4, "%s: Response too short (%"FMTSZ"u vs. %"FMTSZ"u).\n",
          __func__, (size_t)size, sizeof(VMBLOCK_FUSE_READ_RESPONSE));

      return FALSE;
   }

   if (memcmp(buf, VMBLOCK_FUSE_READ_RESPONSE,
              sizeof(VMBLOCK_FUSE_READ_RESPONSE))) {
      LOG(4, "%s: Invalid response %.*s\n",
          __func__, (int)sizeof(VMBLOCK_FUSE_READ_RESPONSE) - 1, buf);

      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_TryInitVmblock --
 *
 *    Initializes file blocking needed to prevent access to file before
 *    transfer has finished.
 *
 * Results:
 *    Block descriptor on success, -1 on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
DnD_TryInitVmblock(const char *vmbFsName,          // IN
                   const char *vmbMntPoint,        // IN
                   const char *vmbDevice,          // IN
                   mode_t vmbDeviceMode,           // IN
                   Bool (*verifyBlock)(int fd))    // IN
{
#if defined NO_SETMNTENT || defined NO_ENDMNTENT
   NOT_IMPLEMENTED();
   errno = ENOSYS;
   return -1;
#else
   Bool found = FALSE;
   int blockFd = -1;
   char *realMntPoint;
   MNTHANDLE fp;
   DECLARE_MNTINFO(mnt);

   ASSERT(vmbFsName);
   ASSERT(vmbMntPoint);
   ASSERT(vmbDevice);

   /* Resolve desired mount point in case it is symlinked somewhere */
   realMntPoint = Posix_RealPath(vmbMntPoint);
   if (!realMntPoint) {
      /*
       * If resolve failed for some reason try to fall back to
       * original mount point specification.
       */
      realMntPoint = Util_SafeStrdup(vmbMntPoint);
   }

   /* Make sure the vmblock file system is mounted. */
   fp = OPEN_MNTFILE("r");
   if (fp == NULL) {
      LOG(1, "%s: could not open mount file\n", __func__);
      goto out;
   }

   while (GETNEXT_MNTINFO(fp, mnt)) {
      /*
       * In the future we can publish the mount point in VMDB so that the UI
       * can use it rather than enforcing the VMBLOCK_MOUNT_POINT check here.
       */

      if (strcmp(MNTINFO_FSTYPE(mnt), vmbFsName) == 0 &&
          strcmp(MNTINFO_MNTPT(mnt), realMntPoint) == 0) {
         found = TRUE;
         break;
      }
   }

   (void) CLOSE_MNTFILE(fp);

   if (found) {
      /* Open device node for communication with vmblock. */
      blockFd = Posix_Open(vmbDevice, vmbDeviceMode);
      if (blockFd < 0) {
         LOG(1, "%s: Can not open blocker device (%s)\n",
             __func__, Err_Errno2String(errno));
      } else {
         LOG(4, "%s: Opened blocker device at %s\n", __func__, VMBLOCK_DEVICE);
         if (verifyBlock && !verifyBlock(blockFd)) {
            LOG(4, "%s: Blocker device at %s did not pass checks, closing.\n",
                __func__, VMBLOCK_DEVICE);
            close(blockFd);
            blockFd = -1;
         }
      }
   }

out:
   free(realMntPoint);
   return blockFd;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_InitializeBlocking --
 *
 *    Initializes file blocking needed to prevent access to file before
 *    transfer has finished.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnD_InitializeBlocking(DnDBlockControl *blkCtrl)   // OUT
{
   uid_t uid;
   int blockFd;

   /* Root access is needed for opening the vmblock device. */
   uid = Id_BeginSuperUser();

   /* Fitrst try FUSE and see if it is available. */
   blockFd = DnD_TryInitVmblock(VMBLOCK_FUSE_FS_NAME, VMBLOCK_FUSE_MOUNT_POINT,
                                VMBLOCK_FUSE_DEVICE, VMBLOCK_FUSE_DEVICE_MODE,
                                DnD_CheckBlockFuse);
   if (blockFd != -1) {
      blkCtrl->fd = blockFd;
      /* Setup FUSE methods. */
      blkCtrl->blockRoot = VMBLOCK_FUSE_FS_ROOT;
      blkCtrl->AddBlock = DnD_AddBlockFuse;
      blkCtrl->RemoveBlock = DnD_RemoveBlockFuse;
      goto out;
   }

   /* Now try OS-specific VMBlock driver. */
   blockFd = DnD_TryInitVmblock(VMBLOCK_FS_NAME, VMBLOCK_MOUNT_POINT,
                                VMBLOCK_DEVICE, VMBLOCK_DEVICE_MODE,
                                NULL);
   if (blockFd != -1) {
      blkCtrl->fd = blockFd;
      /* Setup legacy in-kernel methods. */
      blkCtrl->blockRoot = VMBLOCK_FS_ROOT;
      blkCtrl->AddBlock = DnD_AddBlockLegacy;
      blkCtrl->RemoveBlock = DnD_RemoveBlockLegacy;
      goto out;
   }

   LOG(4, "%s: could not find vmblock mounted\n", __func__);
out:
   Id_EndSuperUser(uid);

   return blockFd != -1;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_UninitializeBlocking --
 *
 *    Uninitialize file blocking.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    All existing blocks will be removed.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnD_UninitializeBlocking(DnDBlockControl *blkCtrl)    // IN
{
   Bool ret = TRUE;

   if (blkCtrl->fd >= 0) {
      if (close(blkCtrl->fd) < 0) {
         Log("%s: Can not close blocker device (%s)\n",
             __func__, Err_Errno2String(errno));
         ret = FALSE;
      } else {
         blkCtrl->fd = -1;
      }
   }

   return ret;
}

/*
 * DnD_CompleteBlockInitialization --
 *
 *    Complete block initialization in case when we were handed
 *    blocking file descriptor (presumably opened for us by a
 *    suid application).
 *
 * Results:
 *    TRUE on success, FALSE on failure (invalid type).
 *
 * Side effects:
 *    Adjusts blkCtrl to match blocking device type (legacy or fuse).
 *
 */

Bool
DnD_CompleteBlockInitialization(int fd,                     // IN
                                DnDBlockControl *blkCtrl)   // OUT
{
   blkCtrl->fd = fd;

   if (DnD_CheckBlockFuse(fd)) {
      /* Setup FUSE methods. */
      blkCtrl->blockRoot = VMBLOCK_FUSE_FS_ROOT;
      blkCtrl->AddBlock = DnD_AddBlockFuse;
      blkCtrl->RemoveBlock = DnD_RemoveBlockFuse;
   } else if (DnD_CheckBlockLegacy(fd)) {
      /* Setup legacy methods. */
      blkCtrl->blockRoot = VMBLOCK_FS_ROOT;
      blkCtrl->AddBlock = DnD_AddBlockLegacy;
      blkCtrl->RemoveBlock = DnD_RemoveBlockLegacy;
   } else {
      Log("%s: Can't determine block type.\n", __func__);

      return FALSE;
   }

   return TRUE;
}

#endif /* linux || sun || FreeBSD */


/*
 *----------------------------------------------------------------------------
 *
 * DnDRootDirUsable --
 *
 *    Determines whether the provided directory is usable as the root for
 *    staging directories.
 *
 * Results:
 *    TRUE if the root directory is usable, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDRootDirUsable(const char *pathName)  // IN:
{
   struct stat buf;

   if (Posix_Stat(pathName, &buf) < 0) {
      return FALSE;
   }

   return S_ISDIR(buf.st_mode) &&
          (buf.st_mode & S_ISVTX) == S_ISVTX &&
          (buf.st_mode & ACCESSPERMS) == DND_ROOTDIR_PERMS;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDSetPermissionsOnRootDir --
 *
 *    Sets the correct permissions for the root staging directory.  We set the
 *    root directory to 1777 so that all users can create their own staging
 *    directories within it and that other users cannot delete that directory.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDSetPermissionsOnRootDir(const char *pathName)  // IN:
{
   return Posix_Chmod(pathName, S_ISVTX | DND_ROOTDIR_PERMS) == 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDStagingDirectoryUsable --
 *
 *    Determines whether a staging directory is usable for the current
 *    process.  A directory is only usable by the current process if it is
 *    owned by the effective uid of the current process.
 *
 * Results:
 *    TRUE if the directory is usable, FALSE if it is not.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDStagingDirectoryUsable(const char *pathName)  // IN:
{
   struct stat buf;

   if (Posix_Stat(pathName, &buf) < 0) {
      return FALSE;
   }

   return buf.st_uid == Id_GetEUid();
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDSetPermissionsOnStagingDir --
 *
 *    Sets the correct permissions for staging directories.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDSetPermissionsOnStagingDir(const char *pathName)  // IN:
{
   return Posix_Chmod(pathName, DND_STAGINGDIR_PERMS) == 0;
}
