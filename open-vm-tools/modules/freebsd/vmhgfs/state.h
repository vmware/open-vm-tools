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
#if defined(__FreeBSD__)
#  define HGFS_VP_TO_FP(vp)                            \
           ((HgfsFile *)(vp)->v_data)
#elif defined(__APPLE__)
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


/*
 * Types
 */

typedef uint32_t HgfsMode;

typedef enum HgfsLockType {
   HGFS_WRITER_LOCK = 0,
   HGFS_READER_LOCK = 1
} HgfsLockType;

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
    * One big difference between the OS X and FreeBSD VFS layers is that the
    * XNU kernel does not lock a vnode before it calls our VFS functions. As a
    * result, we have to provide our RwLock which is locked in macos/vnops.c
    * before any common functions are called.
    */
#if defined(__APPLE__)
   OS_RWLOCK_T *rwFileLock;
#endif

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
                 HgfsFileHashTable *htp, Bool createFile, int permissions);
int HgfsVnodeGetRoot(struct vnode **vpp, struct HgfsSuperInfo *sip, struct mount *vfsp,
		     const char *fileName, HgfsFileType fileType, HgfsFileHashTable *htp);
int HgfsReleaseVnodeContext(struct vnode *vp, HgfsFileHashTable *htp);
void HgfsNodeIdGet(HgfsFileHashTable *ht, const char *fileName,
                   uint32_t fileNameLength, ino_t *outNodeId);
int HgfsInitFileHashTable(HgfsFileHashTable *htp);
void HgfsDestroyFileHashTable(HgfsFileHashTable *htp);
Bool HgfsFileHashTableIsEmpty(struct HgfsSuperInfo *sip, HgfsFileHashTable *htp);

/* Handle get/set/clear functions */
void HgfsSetOpenFileHandle(struct vnode *vp, HgfsHandle handle, HgfsMode openMode);
int HgfsGetOpenFileHandle(struct vnode *vp, HgfsHandle *outHandle);
int HgfsReleaseOpenFileHandle(struct vnode *vp, HgfsHandle *handleToClose);
int HgfsCheckAndReferenceHandle(struct vnode *vp, int requestedOpenMode);
int HgfsHandleIncrementRefCount(struct vnode *vp);

#endif /* _STATE_H_ */
