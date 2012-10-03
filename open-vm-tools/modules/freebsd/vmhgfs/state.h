/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * state.h --
 *
 * Public functions, types, and macros for Hgfs state that is attached to
 * vnodes.
 */

#ifndef _STATE_H_
#define _STATE_H_

/*
 * Includes
 */

#include <sys/param.h>          /* MAXPATHLEN */
#include <sys/lock.h>
#include <sys/vnode.h>          /* struct vnode */

#include "hgfsProto.h"
#include "dbllnklst.h"
#include "os.h"

/*
 * Macros
 */

/* Number of buckets for the HgfsInode hash table */
#define HGFS_HT_NR_BUCKETS             5

/* Conversion between different state structures */
#if defined __FreeBSD__
#  define HGFS_VP_TO_FP(vp)                            \
           ((HgfsFile *)(vp)->v_data)
#elif defined __APPLE__
#  define HGFS_VP_TO_FP(vp)                            \
           ((HgfsFile *)vnode_fsnode(vp))
#endif

#define HGFS_FP_TO_VP(fp)                               \
         (fp)->vnodep

#define HGFS_VP_TO_FILENAME(vp)                         \
         HGFS_VP_TO_FP(vp)->fileName

#define HGFS_VP_TO_FILENAME_LENGTH(vp)                  \
         HGFS_VP_TO_FP(vp)->fileNameLength

#define HGFS_VP_TO_NODEID(vp)                           \
         HGFS_VP_TO_FP(vp)->nodeId

#define HGFS_VP_TO_RWLOCK(vp)                           \
         HGFS_VP_TO_FP(vp)->rwlock

#define HGFS_VP_TO_RWLOCKP(vp)                          \
         &(HGFS_VP_TO_RWLOCK(vp))

#define HGFS_VP_TO_PERMISSIONS(vp)                      \
         HGFS_VP_TO_FP(vp)->permissions

#define HGFS_VP_TO_MMAPPED(vp)                          \
         HGFS_VP_TO_FP(vp)->mmapped

#define HGFS_VP_TO_FILESIZE(vp)                          \
         HGFS_VP_TO_FP(vp)->fileSize


/*
 * Types
 */

typedef uint32_t HgfsMode;

typedef enum HgfsLockType {
   HGFS_WRITER_LOCK = 0,
   HGFS_READER_LOCK = 1
} HgfsLockType;

typedef enum HgfsOpenType {
   OPENREQ_OPEN,      /* A referenced handle in response to a vnode_open. */
   OPENREQ_CREATE,    /* A referenced handle in response to a vnode_create. */
   OPENREQ_READ,      /* A referenced handle in response to a vnode_read. */
   OPENREQ_MMAP,      /* A referenced handle in response to a vnode_mmap. */
} HgfsOpenType;

/*
 * State kept per shared file from the host.
 *
 * All fields are read-only after initialization except reference count, which
 * is protected by the mutex.
 */
typedef struct HgfsFile {
   /* Link to place this state on the file state hash table */
   DblLnkLst_Links listNode;
   /*
    * Full path of file within the filesystem (that is, taking /mnt/hgfs as /).
    * These are built from / in HgfsMount() and appending names as provided to
    * HgfsLookup(). Saving the length in HgfsIget() saves having to calculate
    * it in each HgfsMakeFullName().
    */
   char fileName[MAXPATHLEN + 1];
   uint32_t fileNameLength;
   ino_t nodeId;

   /*
    * A pointer back to the vnode this HgfsFile is for.
    */
   struct vnode *vnodep;

   /*
    * A pointer back to the parent directory vnode.
    */
   struct vnode *parent;

   /*
    * Mode (permissions) specified for this file to be created with.  This is necessary
    * because create is called with the mode then open is called without it.
    * We save this value in create and access it in open.
    * This field is set during initialization of HGFS and is never changed.
    */
   int permissions;

   /* Mode that was used to open the handle on the host */
   HgfsMode mode;
   Bool modeIsSet;
   OS_MUTEX_T *modeMutex;

   /*
    * Handle provided by reply to a request. If the reference count is > 0, the
    * the handle is valid.
    */
   uint32_t handleRefCount;
   HgfsHandle handle;
   OS_RWLOCK_T *handleLock;
   /*
    * Locked along with the above, the additional reference which is the
    * internal references for opening which occurs alongside the actual open.
    */
   uint32_t intHandleRefCount;

   /*
    * Indicates that memory mapping has been established for the file.
    * Used with the reference counting above.
    */
   Bool mmapped;

   /*
    * One big difference between the Mac OS and FreeBSD VFS layers is that the
    * XNU kernel does not lock a vnode before it calls our VFS functions. As a
    * result, we have to provide our RwLock which is locked in macos/vnops.c
    * before any common functions are called.
    */
#if defined __APPLE__
   OS_RWLOCK_T *rwFileLock;
#endif
   /*
    * File size. HGFS must tell memory management system when file size is changed.
    * It implies that HGFS has to know if a write request writes data beyond EOF thus
    * it has to maintain local copy of file size that is kept in sync with the size
    * reported to memory manager/pager.
    */
   off_t fileSize;
} HgfsFile;

/* The hash table for file state. */
typedef struct HgfsFileHashTable {
   OS_MUTEX_T *mutex;
   DblLnkLst_Links hashTable[HGFS_HT_NR_BUCKETS];
} HgfsFileHashTable;

/* Forward declaration to prevent circular dependency between this and hgfsbsd.h. */
struct HgfsSuperInfo;

int HgfsVnodeGet(struct vnode **vpp, struct vnode *dp, struct HgfsSuperInfo *sip,
                 struct mount *vfsp, const char *fileName, HgfsFileType fileType,
                 HgfsFileHashTable *htp, Bool createFile, int permissions,
                 off_t fileSize);
int HgfsVnodeGetRoot(struct vnode **vpp, struct HgfsSuperInfo *sip, struct mount *vfsp,
		     const char *fileName, HgfsFileType fileType, HgfsFileHashTable *htp);
int HgfsReleaseVnodeContext(struct vnode *vp, HgfsFileHashTable *htp);
void HgfsNodeIdGet(HgfsFileHashTable *ht, const char *fileName,
                   uint32_t fileNameLength, ino_t *outNodeId);
int HgfsInitFileHashTable(HgfsFileHashTable *htp);
void HgfsDestroyFileHashTable(HgfsFileHashTable *htp);
Bool HgfsFileHashTableIsEmpty(struct HgfsSuperInfo *sip, HgfsFileHashTable *htp);

/* Handle get/set/clear functions */
void HgfsSetOpenFileHandle(struct vnode *vp, HgfsHandle handle,
                           HgfsMode openMode, HgfsOpenType openType);
int HgfsGetOpenFileHandle(struct vnode *vp, HgfsHandle *outHandle);
int HgfsReleaseOpenFileHandle(struct vnode *vp, HgfsOpenType openType, HgfsHandle *handleToClose);
int HgfsCheckAndReferenceHandle(struct vnode *vp, int requestedOpenMode, HgfsOpenType openType);
int HgfsHandleIncrementRefCount(struct vnode *vp);
void HgfsSetFileSize(struct vnode *vp, off_t newSize);

#endif /* _STATE_H_ */
