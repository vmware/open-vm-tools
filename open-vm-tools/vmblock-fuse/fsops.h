/*********************************************************
 * Copyright (c) 2008-2018,2021 VMware, Inc. All rights reserved.
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
 * fsops.h --
 *
 *      Vmblock fuse filesystem operations.
 *
 *      The file ops and internal functions are prototyped here and the main
 *      function put in a separate file to enable unit testing of these
 *      functions directly. This is also the reason none of the functions are
 *      static.
 */

#ifndef _VMBLOCK_FUSE_H_
#define _VMBLOCK_FUSE_H_

/*
 *  FUSE_USE_VERSION must be set before the fuse or fuse3 headers are
 *  included.  If undefined, fall back to previous default used.
 */
#ifndef FUSE_USE_VERSION
/*
 * FUSE_USE_VERSION sets the version of the FUSE API that will be exported.
 * Version 25 is the newest version supported by the libfuse in our toolchain
 * as of 2008-07.
 */
#define FUSE_USE_VERSION 25
#endif

#include <fuse.h>

#include "vmblock.h"
#include "vm_assert.h"
#include "vm_basic_types.h"

#define REDIRECT_DIR_NAME VMBLOCK_CONTROL_MOUNTPOINT
#define REDIRECT_DIR "/" VMBLOCK_CONTROL_MOUNTPOINT
#define TARGET_DIR "/tmp/VMwareDnD"
#define CONTROL_FILE "/" VMBLOCK_DEVICE_NAME
#define NOTIFY_DIR_NAME VMBLOCK_FUSE_NOTIFY_MNTPNT
#define NOTIFY_DIR "/" NOTIFY_DIR_NAME

/*
 * FS operation functions
 */

int VMBlockReadLink(const char *path, char *buf, size_t bufSize);

#if FUSE_MAJOR_VERSION == 3
int VMBlockGetAttr(const char *path, struct stat *statBuf,
                   struct fuse_file_info *fi);
int VMBlockReadDir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fileInfo,
                   enum fuse_readdir_flags);
#else
int VMBlockGetAttr(const char *path, struct stat *statBuf);
int VMBlockReadDir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fileInfo);
#endif
int VMBlockOpen(const char *path, struct fuse_file_info *fileInfo);
int VMBlockWrite(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fileInfo);
int VMBlockRelease(const char *path, struct fuse_file_info *fileInfo);

extern struct fuse_operations vmblockOperations;

/*
 * Internal functions
 */

int RealReadLink(const char *path, char *buf, size_t bufSize);
void SetTimesToNow(struct stat *statBuf);
int ExternalReadDir(const char *blockPath, const char *realPath,
                    void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fileInfo);
size_t StripExtraPathSeparators(char *path);


/*
 * CharPointerToFuseFileHandle and FuseFileHandleToCharPointer --
 *
 *      Simple functions to keep all typecasting in one place.
 *
 *      Storing a pointer in the fh field of fuse_file_info is the recommended
 *      way to associate a pointer with an open file according to the fuse FAQ:
 *      http://fuse.sourceforge.net/wiki/index.php/FAQ#Is_it_possible_to_store_a_pointer_to_private_data_in_the_fuse_file_info_structurex3f.
 */

static INLINE uint64_t
CharPointerToFuseFileHandle(const char *pointer)               // IN
{
   ASSERT(sizeof (uint64_t) >= sizeof (char *));
   return (uintptr_t)(pointer);
}

static INLINE char *
FuseFileHandleToCharPointer(const uint64_t fileHandle)   // IN
{
   ASSERT(fileHandle <= (uintptr_t)((void *)(-1)));
   return (char *)((uintptr_t)(fileHandle));
}


#endif /* _VMBLOCK_FUSE_H_ */
