/*********************************************************
 * Copyright (C) 2004-2016, 2021 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * hgfsState.h --
 *
 * Public functions, types, and macros for Hgfs state that is attached to
 * vnodes.
 */

#ifndef __HGFS_STATE_H_
#define __HGFS_STATE_H_

/*
 * Includes
 */
#include <sys/param.h>          /* MAXPATHLEN */
#include <sys/rwlock.h>         /* krwlock_t */
#include <sys/ksynch.h>         /* kmutex_t */
#include <sys/vnode.h>          /* struct vnode */

#include "hgfsProto.h"
#include "dbllnklst.h"


/*
 * Macros
 */
/* Number of buckets for the HgfsInode hash table */
#define HGFS_HT_NR_BUCKETS             5

/* Conversion between different state structures */
#define HGFS_VP_TO_OFP(vp)      ((HgfsOpenFile *)(vp)->v_data)
#define HGFS_VP_TO_FP(vp)       ((HgfsFile *)(HGFS_VP_TO_OFP(vp))->hgfsFile)
#define HGFS_OFP_TO_VP(ofp)     (ofp)->vnodep

#define HGFS_VP_TO_FILENAME(vp)                         \
         HGFS_VP_TO_FP(vp)->fileName

#define HGFS_VP_TO_FILENAME_LENGTH(vp)                  \
         HGFS_VP_TO_FP(vp)->fileNameLength

#define HGFS_VP_TO_NODEID(vp)                           \
         HGFS_VP_TO_FP(vp)->nodeId

#define HGFS_VP_TO_RWLOCK(vp)                           \
         HGFS_VP_TO_FP(vp)->rwlock

#define HGFS_VP_TO_RWLOCKP(vp)  &(HGFS_VP_TO_RWLOCK(vp))

#define HGFS_VP_TO_HGFSFILETYPE(vp)                     \
         HGFS_VP_TO_FP(vp)->fileType

/*
 * This macro is used for sanity checking the fact that Solaris won't ever call
 * one of our vnode operations without first calling lookup(), which is the
 * place where we acquire the filename.  I have never seen a function fail
 * because of this, so it is likely that we can remove this macro and the checks
 * on it throughout vnode.c.
 */
#define HGFS_KNOW_FILENAME(vp)                          \
         (                                              \
          vp &&                                         \
          HGFS_VP_TO_FP(vp) &&                          \
          HGFS_VP_TO_FP(vp)->fileName[0] != '\0'        \
         )

/*
 * Types
 */

typedef uint32_t HgfsMode;

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
   ino64_t nodeId;
   /*
    * The file type is saved so additional per-open-file vnodes can be
    * recreated from a Hgfs file without sending a request to the Hgfs server.
    */
   HgfsFileType fileType;
   /*
    * Readers/writers lock for this file.  Needed by rwlock() and rwunlock()
    * entry points.
    */
   krwlock_t rwlock;
   /* Lock to protect the reference count of this file state. */
   kmutex_t mutex;
   uint32_t refCount;
} HgfsFile;


/*
 * State kept per vnode, which implies per open file within a process.
 *
 * Once created, the hgfsFile and vnode/vnodep values are read-only.  The
 * handle and mode will change throughout this structure's existence, and are
 * both protected by their own mutex.
 */
typedef struct HgfsOpenFile {
   /* Handle provided by reply to a request */
   HgfsHandle handle;
   Bool handleIsSet;
   kmutex_t handleMutex;
   /*
    * Mode specified for this file to be created with.  This is necessary
    * because create is called with the mode then open is called without it.
    * We save this value in create and access it in open.
    */
   HgfsMode mode;
   Bool modeIsSet;
   kmutex_t modeMutex;
   /*
    * Pointer to the single Hgfs file state structure shared amongst all open
    * instances of this file.
    */
   HgfsFile *hgfsFile;
   /*
    * A pointer back to the vnode this open-file state is for.
    */
   struct vnode *vnodep;
} HgfsOpenFile;

/* The hash table for file state. */
typedef struct HgfsFileHashTable {
   kmutex_t mutex;
   DblLnkLst_Links hashTable[HGFS_HT_NR_BUCKETS];
} HgfsFileHashTable;

/* Forward declaration to prevent circular dependency between this and hgfs.h. */
struct HgfsSuperInfo;


/*
 * Functions
 */
int HgfsVnodeGet(struct vnode **vpp, struct HgfsSuperInfo *sip, struct vfs *vfsp,
                 const char *fileName, HgfsFileType fileType, HgfsFileHashTable *htp);
int HgfsVnodePut(struct vnode *vp, HgfsFileHashTable *ht);
int HgfsVnodeDup(struct vnode **newVpp, struct vnode *origVp, struct HgfsSuperInfo *sip,
                 HgfsFileHashTable *htp);
int HgfsFileNameToVnode(const char *fileName, struct vnode **vpp,
                        struct HgfsSuperInfo *sip, struct vfs *vfsp,
                        HgfsFileHashTable *htp);
void HgfsNodeIdGet(HgfsFileHashTable *ht, const char *fileName,
                   uint32_t fileNameLength, ino64_t *outNodeId);
void HgfsInitFileHashTable(HgfsFileHashTable *htp);
Bool HgfsFileHashTableIsEmpty(struct HgfsSuperInfo *sip, HgfsFileHashTable *htp);

/* Handle get/set/clear functions */
int HgfsSetOpenFileHandle(struct vnode *vp, HgfsHandle handle);
int HgfsGetOpenFileHandle(struct vnode *vp, HgfsHandle *outHandle);
int HgfsClearOpenFileHandle(struct vnode *vp);
Bool HgfsHandleIsSet(struct vnode *vp);

/* Mode get/set/clear functions */
int HgfsSetOpenFileMode(struct vnode *vp, HgfsMode mode);
int HgfsGetOpenFileMode(struct vnode *vp, HgfsMode *outMode);
int HgfsClearOpenFileMode(struct vnode *vp);

/* Debugging function */
void HgfsDebugPrintFileHashTable(HgfsFileHashTable *htp, int level);

#endif /* __HGFS_STATE_H_ */
