/*********************************************************
 * Copyright (c) 2013,2021 VMware, Inc. All rights reserved.
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
 * module.h --
 *
 * Global module definitions for the entire FUSE based HGFS
 */

#ifndef _VMHGFS_FUSE_MODULE_H_
#define _VMHGFS_FUSE_MODULE_H_

/*
 * FUSE_USE_VERSION must be set before the fuse or fuse3 headers are
 * included.  If undefined, fall back to previous default used.
 */
#ifndef FUSE_USE_VERSION
#   define FUSE_USE_VERSION 29
#endif

#include <sys/types.h>
#include "hgfsUtil.h"
#include "vm_assert.h"
#include <stdlib.h>
#include "vm_assert.h"
#include "cpName.h"
#include "cpNameLite.h"
#include "hgfsDevLinux.h"
#include "request.h"
#include "fsutil.h"
#include "filesystem.h"
#include "vm_basic_types.h"
#include "vmhgfs_version.h"
#include "vmware.h"
#include "str.h"
#include "codeset.h"
#include "rpcout.h"
#include "hgfsProto.h"
#include <errno.h>
#include <linux/list.h>
#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <sys/stat.h>
#ifndef __SOLARIS__
#include <sys/types.h>
#endif

#include "transport.h"
#include "session.h"
#include "config.h"

#if defined(__SOLARIS__) || defined(__APPLE__)
#define DT_UNKNOWN      0
#define DT_FIFO         1
#define DT_CHR          2
#define DT_DIR          4
#define DT_BLK          6
#define DT_REG          8
#define DT_LNK          10
#define DT_SOCK         12
#define DT_WHT          14
#define NAME_MAX        255    /* # chars in a file name */
#endif

#include "hgfsEscape.h"

#ifdef VMX86_DEVEL
extern int LOGLEVEL_THRESHOLD;

#define LGPFX        "vmhgfs-fuse"
#define LGPFX_FMT    "%s:%s:"

#define LOG(level, args)                                   \
   do {                                                    \
      if (level <= LOGLEVEL_THRESHOLD) {                   \
         Log(LGPFX_FMT, LGPFX, __FUNCTION__);              \
         Log args;                                         \
      }                                                    \
   } while (0)

#else
#define LOG(level, args)
#endif

/* Blocksize to be set in superblock. (XXX how is this used?) */
#define HGFS_BLOCKSIZE 1024

/* if st_mtime is a define, assume the higher resolution st_mtim is available */
#if HAVE_STRUCT_STAT_ST_MTIMESPEC || HAVE_STRUCT_STAT_ST_MTIM
#define HGFS_SET_TIME(unixtm,nttime) HgfsConvertFromNtTimeNsec(&unixtm, nttime)
//#define HGFS_GET_TIME(unixtm) HgfsConvertToNtTime(unixtm, 0L)
static INLINE uint64
HGFS_GET_CURRENT_TIME()
{
   struct timespec unixTime;
   clock_gettime(CLOCK_REALTIME, &unixTime);
   return HgfsConvertToNtTime(unixTime.tv_sec, unixTime.tv_nsec);
}
#else
#define HGFS_SET_TIME(unixtm,nttime) HgfsConvertFromNtTime(&unixtm, nttime)
//#define HGFS_GET_TIME(unixtm) HgfsConvertToNtTime(unixtm, 0L)
#define HGFS_GET_CURRENT_TIME() HGFS_GET_TIME(time(NULL))
#endif

/*
 * Global synchronization primitives.
 */

/* Other global state. */
extern HgfsOp hgfsVersionCreateSession;
extern HgfsOp hgfsVersionDestroySession;
extern HgfsOp hgfsVersionOpen;
extern HgfsOp hgfsVersionRead;
extern HgfsOp hgfsVersionWrite;
extern HgfsOp hgfsVersionClose;
extern HgfsOp hgfsVersionSearchOpen;
extern HgfsOp hgfsVersionSearchRead;
extern HgfsOp hgfsVersionSearchClose;
extern HgfsOp hgfsVersionGetattr;
extern HgfsOp hgfsVersionSetattr;
extern HgfsOp hgfsVersionCreateDir;
extern HgfsOp hgfsVersionDeleteFile;
extern HgfsOp hgfsVersionDeleteDir;
extern HgfsOp hgfsVersionRename;
extern HgfsOp hgfsVersionQueryVolumeInfo;
extern HgfsOp hgfsVersionCreateSymlink;

extern HgfsFuseState *gState;

#endif // _VMHGFS_FUSE_MODULE_H_
