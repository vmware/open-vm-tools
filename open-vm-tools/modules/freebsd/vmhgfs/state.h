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
#  define HGFS_VP_TO_OFP(vp)                            \
           ((HgfsOpenFile *)(vp)->v_data)
#elif defined(__APPLE__)
#  define HGFS_VP_TO_OFP(vp)                            \
           ((HgfsOpenFile *)vnode_fsnode(vp))
#endif

#define HGFS_VP_TO_FP(vp)                               \
         ((HgfsFile *)(HGFS_VP_TO_OFP(vp))->hgfsFile)

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

#define HGFS_VP_TO_HGFSFILETYPE(vp)                     \
         HGFS_VP_TO_FP(vp)->fileType

#define HGFS_OFP_TO_FP(ofp)                             \
         (ofp)->hgfsFile

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
    * The file type is saved so additional per-open-file vnodes can be
    * recreated from a Hgfs file without sending a request to the Hgfs server.
    */
   HgfsFileType fileType;

   OS_MUTEX_T *mutex;
   uint32_t refCount;
} HgfsFile;

/*
 * State kept per vnode, which implies per open file within a process.
 *
 * Once created, the hgfsFile and vnode/vnodep values are read-only.  The
 * handle and mode will change throughout this structure's existence.
 */
typedef struct HgfsOpenFile {
   /*
    * Pointer to the single Hgfs file state structure shared amongst all open
    * instances of this file.
    */
   HgfsFile *hgfsFile;

   /*
    * A pointer back to the vnode this open-file state is for.
    */
   struct vnode *vnodep;

   /*
    * Mode specified for this file to be created with.  This is necessary
    * because create is called with the mode then open is called without it.
    * We save this value in create and access it in open.
    */
   HgfsMode mode;
   Bool modeIsSet;
   OS_MUTEX_T *modeMutex;

   /*
    * Handle provided by reply to a request. If the reference count is > 0, the
    * the handle is valid.
    */
   uint32_t handleRefCount;
   HgfsHandle handle;
   OS_MUTEX_T *handleMutex;

   /*
    * One big difference between the OS X and FreeBSD VFS layers is that the
    * XNU kernel does not lock a vnode before it calls our VFS functions. As a
    * result, we have to provide our RwLock which is locked in macos/vnops.c
    * before any common functions are called.
    */
#if defined(__APPLE__)
   OS_RWLOCK_T *rwFileLock;
#endif

} HgfsOpenFile;

/* The hash table for file state. */
typedef struct HgfsFileHashTable {
   OS_MUTEX_T *mutex;
   DblLnkLst_Links hashTable[HGFS_HT_NR_BUCKETS];
} HgfsFileHashTable;

/* Forward declaration to prevent circular dependency between this and hgfsbsd.h. */
struct HgfsSuperInfo;

int HgfsVnodeGet(struct vnode **vpp, struct HgfsSuperInfo *sip, struct mount *vfsp,
		 const char *fileName, HgfsFileType fileType, HgfsFileHashTable *htp);
int HgfsVnodeGetRoot(struct vnode **vpp, struct HgfsSuperInfo *sip, struct mount *vfsp,
		     const char *fileName, HgfsFileType fileType, HgfsFileHashTable *htp);
int HgfsVnodePut(struct vnode *vp, HgfsFileHashTable *htp);
void HgfsNodeIdGet(HgfsFileHashTable *ht, const char *fileName,
                   uint32_t fileNameLength, ino_t *outNodeId);
int HgfsInitFileHashTable(HgfsFileHashTable *htp);
void HgfsDestroyFileHashTable(HgfsFileHashTable *htp);
Bool HgfsFileHashTableIsEmpty(struct HgfsSuperInfo *sip, HgfsFileHashTable *htp);

/* Handle get/set/clear functions */
int HgfsSetOpenFileHandle(struct vnode *vp, HgfsHandle handle);
int HgfsGetOpenFileHandle(struct vnode *vp, HgfsHandle *outHandle);
int HgfsReleaseOpenFileHandle(struct vnode *vp, Bool *closed);
Bool HgfsShouldCloseOpenFileHandle(struct vnode *vp);
Bool HgfsHandleIsSet(struct vnode *vp);
int HgfsHandleIncrementRefCount(struct vnode *vp);

/* Mode get/set/clear functions */
int HgfsSetOpenFileMode(struct vnode *vp, HgfsMode mode);
int HgfsGetOpenFileMode(struct vnode *vp, HgfsMode *outMode);
int HgfsClearOpenFileMode(struct vnode *);

/* HgfsFile locking functions */
int HgfsFileLock(struct vnode *vp, HgfsLockType type);
int HgfsFileLockOfp(HgfsOpenFile *ofp, HgfsLockType type);
int HgfsFileUnlock(struct vnode *vp, HgfsLockType type);
int HgfsFileUnlockOfp(HgfsOpenFile *ofp, HgfsLockType type);

#endif /* _STATE_H_ */
