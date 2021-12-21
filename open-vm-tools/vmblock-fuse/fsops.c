/*********************************************************
 * Copyright (C) 2008-2021 VMware, Inc. All rights reserved.
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
 * fsops.c --
 *
 *      Vmblock fuse filesystem operations.
 *
 *      See design.txt for more information.
 */

#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include "fsops.h"
#include "block.h"
#include "vm_basic_types.h"
#include "vm_assert.h"

/* Regular directories on a linux ext3 partition are 4K. */

#define DIR_SIZE (4 * 1024)

typedef struct vmblockSpecialDirEntry {
   char *path;
   int mode;
   unsigned int nlink;
   size_t size;
} vmblockSpecialDirEntry;

static vmblockSpecialDirEntry specialDirEntries[] = {
   { "/",               S_IFDIR | 0555, 3, DIR_SIZE },
   { CONTROL_FILE,      S_IFREG | 0600, 1, 0 },
   { REDIRECT_DIR,      S_IFDIR | 0555, 3, DIR_SIZE },
   { NOTIFY_DIR,        S_IFDIR | 0555, 3, DIR_SIZE },
   { NULL,              0,              0, 0 }
};
static vmblockSpecialDirEntry symlinkDirEntry =
   { REDIRECT_DIR "/*", S_IFLNK | 0777, 1, -1 };

static vmblockSpecialDirEntry notifyDirEntry =
   { NOTIFY_DIR "/*",   S_IFREG | 0444, 1, 0 };

#if FUSE_MAJOR_VERSION == 3
#define CALL_FUSE_FILLER(buf, name, stbuf, off, flags) \
   filler(buf, name, stbuf, off, flags)
#else
#define CALL_FUSE_FILLER(buf, name, stbuf, off, flags) \
   filler(buf, name, stbuf, off)
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * RealReadLink --
 *
 *      Gets the target of a symlink.
 *
 *      The same idea as unix readlink() except that it returns 0 on
 *      success, and fills buf with a null terminated string. If target doesn't
 *      fit in buf, it is truncated.
 *
 * Results:
 *      0 on success. Possible errors (as negative values):
 *      -EINVAL       bufSize is not positive.
 *      -ENOENT       path does not exist.
 *      -ENAMETOOLONG path or target of symlink was too long.
 *      Any other errors lstat can return.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
RealReadLink(const char *path,   // IN: Path within vmblock filesystem. Must be
                                 //     within the redirect directory.
             char *buf,          // OUT: Target of link if valid (or part of it).
             size_t bufSize)     // IN: Size of buf.
{
   int status;
   const char redirectPrefix[] = REDIRECT_DIR "/";
   const int redirectPrefixLength = sizeof redirectPrefix - 1;
   const char targetPrefix[] = TARGET_DIR "/";
   const int targetPrefixLength = sizeof targetPrefix - 1;
   const char *relativeTarget = path + redirectPrefixLength;
   char target[PATH_MAX + 1];
   const size_t spaceForRelativeTarget = sizeof target - targetPrefixLength;
   struct stat dummyStatBuf; // Don't care what goes here.

   /* TARGET_DIR + '/' needs to leave room for relative target. */
   ASSERT_ON_COMPILE(sizeof TARGET_DIR + 1 < PATH_MAX);

   ASSERT(strncmp(path, redirectPrefix, redirectPrefixLength) == 0 &&
          strlen(path) > redirectPrefixLength);
   if (bufSize < 1) {
      return -EINVAL;
   }

   /*
    * Assemble path to destination of link. This goes into a temporary buffer
    * instead of directly into buf, because buf may not be big enough for the
    * whole thing, but this should still return success if the target
    * exists which means it must lstat the full target path.
    */

   strlcpy(target, targetPrefix, sizeof target);

   /*
    * spaceForRelativeTarget must be greater than strlen(relativeTarget) to
    * leave room for the nul terminator.
    */

   if (spaceForRelativeTarget <= strlen(relativeTarget)) {
      return -ENAMETOOLONG;
   }
   strlcpy(target + targetPrefixLength, relativeTarget, spaceForRelativeTarget);

   /* Verify that target exists. */

   status = lstat(target, &dummyStatBuf);
   if (status != 0) {
      return -errno;
   }

   strlcpy(buf, target, bufSize);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockReadLink --
 *
 *      Gets the target of a symlink. Blocks if there is a block on the path.
 *
 *      Basically the same as unix readlink() except that it returns 0 on
 *      success, fills buf with a null terminated string, and adds our blocking
 *      functionality.
 *
 * Results:
 *      0 on success. Possible errors (as negative values):
 *      -EINVAL       bufSize is not positive.
 *      -ENOENT       path is not within the redirect directory.
 *      -ENAMETOOLONG path or target of symlink was too long.
 *      Any other errors lstat can return.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockReadLink(const char *path,   // IN: Path within vmblock (top level) mount point.
                char *buf,          // OUT: Target of link if valid (or part of it).
                size_t bufSize)     // IN: Size of buf.
{
   int status;
   char target[PATH_MAX + 1];

   if (strncmp(path, REDIRECT_DIR, strlen(REDIRECT_DIR)) != 0) {
      return -ENOENT;
   }
   status = RealReadLink(path, target, sizeof target);
   if (status < 0) {
      return status;
   }

   BlockWaitOnFile(target, NULL);

   strlcpy(buf, target, bufSize);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SetTimesToNow --
 *
 *      Sets the atime, mtime, and ctime of a stat struct to the current time.
 *      If there's an error getting the current time, sets them to 0.
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
SetTimesToNow(struct stat *statBuf)      // OUT
{
   int status;
   struct timeval time;

   status = gettimeofday(&time, NULL);
   if (status < 0) {
      statBuf->st_atime = statBuf->st_mtime = statBuf->st_ctime = 0;
   } else {
      statBuf->st_atime = statBuf->st_mtime = statBuf->st_ctime = time.tv_sec;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockGetAttr --
 *
 *      Gets the attributes of a directory entry. Equivalent to stat().
 *
 *      Returns fixed results for /, /dev, and /blockdir. For anything within
 *      /blockdir, if a target exists with that name in the target directory,
 *      it returns fixed stats for a symlink. Reads the stats to return from the
 *      globals specialDirEntries and symlinkDirEntries.
 *
 * Results:
 *      0 on success. Possible errors (as negative values):
 *      -ENOENT  File or directory does not exist.
 *      -ENAMETOOLONG path too long.
 *      Any other errors lstat can return.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
int
VMBlockGetAttr(const char *path,          // IN: File to get attributes of.
               struct stat *statBuf,      // OUT: Where to put the attributes.
               struct fuse_file_info *fi) // IN: Ignored
#else
int
VMBlockGetAttr(const char *path,        // IN: File to get attributes of.
               struct stat *statBuf)    // OUT: Where to put the attributes.
#endif
{
   vmblockSpecialDirEntry *dirEntry;
   ASSERT(path != NULL);
   ASSERT(statBuf != NULL);

   if (strlen(path) > PATH_MAX) {
      return -ENAMETOOLONG;
   }

   for (dirEntry = specialDirEntries; dirEntry->path != NULL; ++dirEntry) {
      if (strcmp(path, dirEntry->path) == 0) {
         memset(statBuf, 0, sizeof *statBuf);
         statBuf->st_mode = dirEntry->mode;
         statBuf->st_nlink = dirEntry->nlink;
         statBuf->st_size = dirEntry->size;
         SetTimesToNow(statBuf);
         return 0;
      }
   }
   if (strncmp(path, REDIRECT_DIR, strlen(REDIRECT_DIR)) == 0) {
      char target[PATH_MAX + 1];
      int status = RealReadLink(path, target, sizeof target);

      LOG(4, "%s: Called RealReadLink which returned: %d\n", __func__, status);
      if (status != 0) {
         return status;
      }
      memset(statBuf, 0, sizeof *statBuf);
      statBuf->st_mode = symlinkDirEntry.mode;
      statBuf->st_nlink = symlinkDirEntry.nlink;
      statBuf->st_size = strlen(target);
      SetTimesToNow(statBuf);
      return 0;
   }
   if (strncmp(path, NOTIFY_DIR, strlen(NOTIFY_DIR)) == 0) {
      memset(statBuf, 0, sizeof *statBuf);
      statBuf->st_mode = notifyDirEntry.mode;
      statBuf->st_nlink = notifyDirEntry.nlink;
      statBuf->st_size = notifyDirEntry.size;
      SetTimesToNow(statBuf);
      return 0;
   }
   return -ENOENT;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ExternalReadDir --
 *
 *      Gets the contents of a directory outside the vmblock-fuse filesystem.
 *
 * Results:
 *      0 on success. Possible errors (as negative values):
 *      -EACCES  Permission denied.
 *      -EBADF   Invalid directory stream descriptor (I don't think this can
 *               happen).
 *      -EMFILE  Too many file descriptors in use by process.
 *      -ENFILE  Too many files are currently open in the system.
 *      -ENOENT  Directory does not exist, or name is an empty string.
 *      -ENOMEM  Insufficient memory to complete the operation.
 *      -ENOTDIR path is not a directory.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
ExternalReadDir(const char *blockPath,           // IN:
                const char *realPath,            // IN: Full (real) path to
                                                 //     directory to read.
                void *buf,                       // OUT: Destination for
                                                 //      directory listing.
                fuse_fill_dir_t filler,          // IN: Function to use to add
                                                 //     an entry to buf. Returns
                                                 //     1 if buffer full, 0
                                                 //     otherwise.
                off_t offset,                    // IN: Ignored.
                struct fuse_file_info *fileInfo) // IN: Ignored.
{
   int status;
   struct dirent *dentry = NULL;
   struct stat statBuf;
   DIR *dir = NULL;

   LOG(4, "%s: blockPath: %s, realPath: %s\n", __func__, blockPath, realPath);
   dir = opendir(realPath);
   if (dir == NULL) {
      return -errno;
   }

   /*
    * readdir() only needs to fill in the type bits of the mode in the stat
    * struct it passes to filler().
    * http://sourceforge.net/mailarchive/forum.php?thread_name=E1KNlwx-00008e-Fb%40pomaz-ex.szeredi.hu&forum_name=fuse-devel
    */

   memset(&statBuf, 0, sizeof statBuf);
   if (strncmp(blockPath, NOTIFY_DIR, strlen(NOTIFY_DIR)) == 0) {
      statBuf.st_mode = S_IFREG;
   } else {
      statBuf.st_mode = S_IFLNK;
   }

   /* Clear errno because readdir won't change it if it succeeds. */
   errno = 0;

   while ((dentry = readdir(dir)) != NULL) {
      status = CALL_FUSE_FILLER(buf, dentry->d_name, &statBuf, 0, 0);
      if (status == 1) {
         break;
      }
   }

   if (errno != 0) {
      return -errno;
   }

   status = closedir(dir);
   if (status != 0) {
      return -errno;
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockReadDir --
 *
 *      Gets the contents of a directory within the filesystem.
 *
 *      If filler() runs out of memory, it will store an error in buf (which fuse
 *      will check) and then return 1.
 *      http://sourceforge.net/mailarchive/forum.php?thread_name=E1KMUnu-0007gI-29%40pomaz-ex.szeredi.hu&forum_name=fuse-devel
 *
 * Results:
 *      0 on success or filler() fail. Possible errors (as negative values):
 *      -EACCES  Permission denied.
 *      -EBADF   Invalid directory stream descriptor (I don't think this can
 *               happen).
 *      -EMFILE  Too many file descriptors in use by process.
 *      -ENFILE  Too many files are currently open in the system.
 *      -ENOENT  Directory does not exist, or name is an empty string.
 *      -ENOMEM  Insufficient memory to complete the operation.
 *      -ENOTDIR path is not a directory.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
int
VMBlockReadDir(const char *path,                // IN: Directory to read.
               void *buf,                       // OUT: Where to put directory
                                                //      listing.
               fuse_fill_dir_t filler,          // IN: Function to add an entry
                                                //     to buf.
               off_t offset,                    // IN: Ignored.
               struct fuse_file_info *fileInfo, // IN: Ignored.
               enum fuse_readdir_flags flags)   // IN: Ignored.
#else
int
VMBlockReadDir(const char *path,                // IN: Directory to read.
               void *buf,                       // OUT: Where to put directory
                                                //      listing.
               fuse_fill_dir_t filler,          // IN: Function to add an entry
                                                //     to buf.
               off_t offset,                    // IN: Ignored.
               struct fuse_file_info *fileInfo) // IN: Ignored.
#endif
{
   struct stat fileStat;
   struct stat dirStat;
   LOG(4, "%s: path: %s\n", __func__, path);

   /*
    * readdir() only needs to fill in the type bits of the mode in the stat
    * struct it passes to filler().
    * http://sourceforge.net/mailarchive/forum.php?thread_name=E1KNlwx-00008e-Fb%40pomaz-ex.szeredi.hu&forum_name=fuse-devel
    */

   memset(&fileStat, 0, sizeof fileStat);
   fileStat.st_mode = S_IFREG;
   memset(&dirStat, 0, sizeof dirStat);
   dirStat.st_mode = S_IFDIR;

   if (strcmp(path, "/") == 0) {
      (void)(CALL_FUSE_FILLER(buf, ".", &dirStat, 0, 0) ||
             CALL_FUSE_FILLER(buf, "..", &dirStat, 0, 0) ||
             CALL_FUSE_FILLER(buf, VMBLOCK_DEVICE_NAME, &fileStat, 0, 0) ||
             CALL_FUSE_FILLER(buf, REDIRECT_DIR_NAME, &dirStat, 0, 0) ||
             CALL_FUSE_FILLER(buf, NOTIFY_DIR_NAME, &dirStat, 0, 0));
      return 0;
   } else if (   (strcmp(path, REDIRECT_DIR) == 0)
              || (strcmp(path, NOTIFY_DIR) == 0)) {
      return ExternalReadDir(path, TARGET_DIR, buf, filler, offset, fileInfo);
   } else {
      return -ENOENT;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockOpen --
 *
 *      Opens the control file. Trying to open anything else will fail.
 *
 *      The file handle that is filled into fileInfo is a memory address so it
 *      can be any number that is at least 1 and at most (void *)(-1);
 *
 *      fileInfo will be marked as requiring direct io. This disables any
 *      caching, which may not be needed, but can't hurt. It also somehow helps
 *      allow the offset to advance past 4k which matters if we take out seeking
 *      to 0 before writing.
 *
 * Results:
 *      0 on success. Possible errors (as negative values):
 *      -ENOENT  path is anything other than the control file.
 *      -ENOMEM  Not enough memory available.
 *      If this was a proper file system, it would probably do additional
 *      checking and return -EISDIR or -ENAMETOOLONG in the right situations,
 *      but we don't care about that. If it's not the control file, it's
 *      -ENOENT.
 *
 * Side effects:
 *      Allocates one byte of memory, the address of which is used as a unique
 *      identifier of the open file.
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockOpen(const char *path,                   // IN
            struct fuse_file_info *fileInfo)    // IN/OUT
{
   /*
    * The blocking code needs a unique value associated with each open file to
    * know who owns what block. The memory address of a malloc()ed byte is used
    * for this purpose.
    */

   char *uniqueValue = NULL;

   if (   (strcmp(path, CONTROL_FILE) != 0)
       && (strncmp(path, NOTIFY_DIR, strlen(NOTIFY_DIR)) != 0)) {
      return -ENOENT;
   }

   uniqueValue = malloc(sizeof *uniqueValue);
   if (uniqueValue == NULL) {
      return -ENOMEM;
   }

   fileInfo->fh = CharPointerToFuseFileHandle(uniqueValue);
   fileInfo->direct_io = 1;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StripExtraPathSeparators --
 *
 *      Removes extra repeated '/' characters from a null terminated string.
 *      Eg: "/foo//bar" -> "/foo/bar".
 *      Also removes any trailing '/'s from the string.
 *
 *      Sometimes programs wind up building paths with extra '/'s in them which
 *      works, but we want to know that they're the same for blocking purposes.
 *      Similarly, we want /foo/bar/ and /foo/bar to match.
 *
 * Results:
 *      path has extra separators removed (see above).
 *      Returns the new length of the string.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

size_t
StripExtraPathSeparators(char *path)      // IN/OUT: nul terminated string.
{
   unsigned int i;
   size_t length;
   Bool lastCharWasSep = FALSE;

   ASSERT(path != NULL);
   length = strlen(path);

   for (i = 0; i < length; i++) {
      if (path[i] == '/') {
         if (lastCharWasSep) {
            memmove(path + i - 1, path + i, length - i + 1);
            --length;
            ASSERT(i > 0);
            --i;
         } else {
            lastCharWasSep = TRUE;
         }
      } else {
         lastCharWasSep = FALSE;
      }
   }

   /*
    * Strip trailing slash when appropriate.
    */

   if (length > 1 && path[length - 1] == '/') {
      --length;
   }

   path[length] = '\0';
   return length;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockWrite --
 *
 *      Writes to the control file to perform a blocking operation.
 *
 *      The write is the means by which a file block is add or removed. In a
 *      development build, it also allows all blocks to be listed. The desired
 *      operation is indicated by the first character in buf. Constants are
 *      defined in vmblock.h:
 *      VMBLOCK_ADD_FILEBLOCK: Add block
 *      VMBLOCK_DEL_FILEBLOCK: Remove block
 *      VMBLOCK_LIST_FILEBLOCKS: List blocks (only with VMX86_DEVEL)
 *
 *      The control file must already be open.
 *
 * Results:
 *      Returns size argument on success. Possible errors (as negative values):
 *      -EINVAL Operation character is not a valid value.
 *      -ENAMETOOLONG File to be blocked is too long.
 *      -ENOENT Tried to remove a block that doesn't exist.
 *      Maybe others.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockWrite(const char *path,                 // IN: Must be control file.
             const char *buf,                  // IN: Operation and file operand.
                                               //     Not nul terminated for
                                               //     consistency with other
                                               //     special fses, eg: procfs.
             size_t size,                      // IN: Size of buf.
             off_t offset,                     // IN: Ignored.
             struct fuse_file_info *fileInfo)  // IN: Ignored.
{
   int status;
   char trimmedBuf[PATH_MAX + 1];
   os_blocker_id_t blockerId;

   LOG(4, "%s: path: %s, size: %"FMTSZ"u\n", __func__, path, size);
   LOG(4, "%s: fileInfo->fh: %p\n", __func__,
      FuseFileHandleToCharPointer(fileInfo->fh));
   ASSERT(strcmp(path, CONTROL_FILE) == 0);
   if (size > PATH_MAX) {
      return -ENAMETOOLONG;
   }

   memcpy(trimmedBuf, buf, size);
   trimmedBuf[size] = '\0';
   LOG(4, "%s: buf: %s\n", __func__, trimmedBuf);
   StripExtraPathSeparators(trimmedBuf);

   blockerId = FuseFileHandleToCharPointer(fileInfo->fh);
   switch (trimmedBuf[0]) {
   case VMBLOCK_ADD_FILEBLOCK:
      status = BlockAddFileBlock(trimmedBuf + 1, blockerId);
      break;
   case VMBLOCK_DEL_FILEBLOCK:
      status = BlockRemoveFileBlock(trimmedBuf + 1, blockerId);
      break;
#ifdef VMX86_DEVEL
   case VMBLOCK_LIST_FILEBLOCKS:
      BlockListFileBlocks();
      status = 0;
      break;
#endif // VMX86_DEVEL
   default:
      status = -EINVAL;
   }
   return status == 0 ? size : status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockRead --
 *
 *      Reads from the control file yield the FUSE greeting string that is
 *      used by the vmware user process to detect whether it is dealing with
 *      FUSE-based or in-kernel block driver.
 *
 *      The control file must already be open.
 *
 * Results:
 *      Returns sizeof(VMBLOCK_FUSE_READ_RESPONSE) on success.
 *      Possible error (as negative value):
 *      -EINVAL if size of supplied buffer is too small.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockRead(const char *path,                 // IN: Must be control file.
            char *buf,                        // IN/OUT: oputput buffer
            size_t size,                      // IN: Size of buf.
            off_t offset,                     // IN: Ignored.
            struct fuse_file_info *fileInfo)  // IN: Ignored.
{
   const char redirectPrefix[] = REDIRECT_DIR "/";
   const char redirectPrefixLength = sizeof redirectPrefix - 1;
   const char notifyPrefix[] = NOTIFY_DIR "/";
   const char notifyPrefixLength = sizeof notifyPrefix - 1;
   const char *relativePath = path + notifyPrefixLength;

   LOG(4, "%s: path: %s, size: %"FMTSZ"u\n", __func__, path, size);
   LOG(4, "%s: fileInfo->fh: %p\n", __func__,
       FuseFileHandleToCharPointer(fileInfo->fh));

   if (strcmp(path, CONTROL_FILE) == 0) {
      if (size < sizeof VMBLOCK_FUSE_READ_RESPONSE) {
         return -EINVAL;
      }
      memcpy(buf, VMBLOCK_FUSE_READ_RESPONSE, sizeof VMBLOCK_FUSE_READ_RESPONSE);
      return sizeof VMBLOCK_FUSE_READ_RESPONSE;
   }

   if (strncmp(path, NOTIFY_DIR, strlen(NOTIFY_DIR)) == 0) {
      char target[PATH_MAX+1];
      char targetLink[PATH_MAX+1];

      strlcpy(target, redirectPrefix, sizeof target);
      strlcpy(target + redirectPrefixLength,
              relativePath,
              sizeof target - redirectPrefixLength);
      if (RealReadLink(target, targetLink, sizeof targetLink) < 0) {
         return -EINVAL;
      }
      return BlockWaitFileBlock(targetLink, OS_UNKNOWN_BLOCKER);
   }

   return -EINVAL;
}

/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockRelease --
 *
 *      Releases an open (control) file. Removes any blocks created via this
 *      file.
 *
 * Results:
 *      Returns 0.
 *
 * Side effects:
 *      Free's the memory allocated as a unique identifier for the open file.
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockRelease(const char *path,                   // IN: Must be control file.
               struct fuse_file_info *fileInfo)    // IN/OUT: Contains fh which is
                                                   //         freed.
{
   char *blockerId = FuseFileHandleToCharPointer(fileInfo->fh);

   ASSERT(path);
   ASSERT(fileInfo);

   if (strcmp(path, CONTROL_FILE) == 0) {
      ASSERT(blockerId != NULL);
      BlockRemoveAllBlocks(blockerId);
   }
   free(blockerId);
   blockerId = NULL;
   fileInfo->fh = CharPointerToFuseFileHandle(NULL);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockInit --
 *
 *      Initializes the filesystem.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Calls BlockInit which allocates memory and does other initialization.
 *
 *-----------------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
void *
VMBlockInit(struct fuse_conn_info *conn,
            struct fuse_config *config)
#else
#if FUSE_USE_VERSION < 26
void *
VMBlockInit(void)
#else
void *
VMBlockInit(struct fuse_conn_info *conn)
#endif
#endif
{
   BlockInit();
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockDestroy --
 *
 *      Cleans up after the filesystem.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Calls BlockDestroy which frees memory and does other cleanup.
 *
 *-----------------------------------------------------------------------------
 */

void
VMBlockDestroy(void *private_data)    // IN: Not used.
{
   BlockCleanup();
}


struct fuse_operations vmblockOperations = {
   .readlink = VMBlockReadLink,
   .getattr  = VMBlockGetAttr,
   .readdir  = VMBlockReadDir,
   .open     = VMBlockOpen,
   .write    = VMBlockWrite,
   .read     = VMBlockRead,
   .release  = VMBlockRelease,
   .init     = VMBlockInit,
   .destroy  = VMBlockDestroy
};
