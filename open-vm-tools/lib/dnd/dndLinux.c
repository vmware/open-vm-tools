/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
#include "dnd.h"
#include "posix.h"
#include "file.h"
#include "strutil.h"
#include "vm_assert.h"
#include "util.h"
#include "escape.h"
#include "su.h"
#if defined(linux) || defined(sun) || defined(__FreeBSD__)
#include "vmblock.h"
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

ConstUnicode
DnD_GetFileRoot(void)
{
   return "/tmp/VMwareDnD/";
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_PrependFileRoot --
 *
 *    Given a buffer of '\0' delimited filenames, this prepends the file root
 *    to each one and uses '\0' delimiting for the output buffer.  The buffer
 *    pointed to by *src will be freed and *src will point to a new buffer
 *    containing the results.  *srcSize is set to the size of the new buffer,
 *    not including the NUL-terminator.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    *src will be freed, and a new buffer will be allocated. This buffer must
 *    be freed by the caller.
 *
 *-----------------------------------------------------------------------------
 */

Bool
DnD_PrependFileRoot(const char *fileRoot,  // IN    : file root to append
                    char **src,            // IN/OUT: NUL-delimited list of paths
                    size_t *srcSize)       // IN/OUT: size of list
{
   ASSERT(fileRoot);
   ASSERT(src);
   ASSERT(*src);
   ASSERT(srcSize);

   return DnDPrependFileRoot(fileRoot, '\0', src, srcSize);
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
   } else {
      Warning("DnDUriListGetFile: the URI list did not begin with %s or %s\n",
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
      Warning("DnD_UriListGetNextFile: error unescaping filename\n");
      return NULL;
   }

   *index = nextIndex;
   if (length) {
      *length = unescapedLength;
   }
   return unescapedName;
}


/* We need to make this suck less. */
#if defined(linux) || defined(sun) || defined(__FreeBSD__)
/*
 *----------------------------------------------------------------------------
 *
 * DnD_InitializeBlocking --
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

int
DnD_InitializeBlocking(void)
{
   uid_t uid;
   Bool found = FALSE;
   int blockFd = -1;
   MNTHANDLE fp;
   DECLARE_MNTINFO(mnt);

   /* root access is needed for opening the vmblock device */
   uid = Id_BeginSuperUser();

   /* Make sure the vmblock file system is mounted. */
   fp = OPEN_MNTFILE("r");
   if (fp == NULL) {
      LOG(1, ("DnD_InitializeBlocking: could not open mount file\n"));
      (void) CLOSE_MNTFILE(fp);
      goto out;
   }

   while (GETNEXT_MNTINFO(fp, mnt)) {
      /*
       * In the future we can publish the mount point in VMDB so that the UI
       * can use it rather than enforcing the VMBLOCK_MOUNT_POINT check here.
       */
      if (strcmp(MNTINFO_FSTYPE(mnt), VMBLOCK_FS_NAME) == 0 &&
          strcmp(MNTINFO_MNTPT(mnt), VMBLOCK_MOUNT_POINT) == 0) {
         found = TRUE;
         break;
      }
   }

   (void) CLOSE_MNTFILE(fp);

   if (!found) {
      LOG(4, ("DnD_InitializeBlocking: could not find vmblock mounted\n"));
      goto out;
   }

   /* Open device node for communication with vmblock. */
   blockFd = Posix_Open(VMBLOCK_DEVICE, VMBLOCK_DEVICE_MODE);
   if (blockFd < 0) {
      LOG(1, ("DnD_InitializeBlocking: Can not open blocker device (%s)\n",
              strerror(errno)));
      goto out;
   }
   LOG(4, ("DnD_InitializeBlocking: Opened blocker device at %s\n",
           VMBLOCK_DEVICE));

out:
   Id_EndSuperUser(uid);

   return blockFd;
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
DnD_UninitializeBlocking(int blockFd)        // IN
{
   Bool ret = TRUE;
   if (blockFd >= 0) {
      if (close(blockFd) < 0) {
         Log("DnD_UninitializeBlocking: Can not close blocker device (%s)\n",
             strerror(errno));
         ret = FALSE;
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_AddBlock --
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
DnD_AddBlock(int blockFd,                    // IN
             const char *blockPath)          // IN
{
   ASSERT(blockFd >= 0);

   if (VMBLOCK_CONTROL(blockFd, VMBLOCK_ADD_FILEBLOCK, blockPath) != 0) {
      LOG(1, ("DnD_AddBlock: Cannot add block on %s (%s)\n",
              blockPath, strerror(errno)));
      return FALSE;
   }
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_RemoveBlock --
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
DnD_RemoveBlock(int blockFd,                    // IN
                const char *blockedPath)        // IN
{
   if (blockFd >= 0) {
      if (VMBLOCK_CONTROL(blockFd, VMBLOCK_DEL_FILEBLOCK, blockedPath) != 0) {
         Log("DnD_RemoveBlock: Cannot delete block on %s (%s)\n",
             blockedPath, strerror(errno));
         return FALSE;
      }
   } else {
      LOG(4, ("DnD_RemoveBlock: Could not remove block on %s: fd to vmblock no "
              "longer exists.\n", blockedPath));
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
DnDRootDirUsable(ConstUnicode pathName)  // IN:
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
DnDSetPermissionsOnRootDir(ConstUnicode pathName)  // IN:
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
DnDStagingDirectoryUsable(ConstUnicode pathName)  // IN:
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
DnDSetPermissionsOnStagingDir(ConstUnicode pathName)  // IN:
{
   return Posix_Chmod(pathName, DND_STAGINGDIR_PERMS) == 0;
}
