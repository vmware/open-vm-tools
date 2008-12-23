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
 * state.c --
 *
 *	Vnode, HgfsOpenFile, and HgfsFile state manipulation routines.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/fcntl.h>

#if defined(__FreeBSD__)
#  include <sys/libkern.h>
#  include <sys/malloc.h>
#  include "sha1.h"
#  include "compat_freebsd.h"
#elif defined(__APPLE__)
#  include <string.h>
/*
 * The OS X kernel includes the same exact SHA1 routines as those
 * provided by bora/lib/misc. Use the kernel ones under OS X.
 */
#  include <libkern/crypto/sha1.h>
#endif

#include "hgfs_kernel.h"
#include "state.h"
#include "debug.h"
#include "os.h"

/*
 * Macros
 */

#define HGFS_FILE_HT_HEAD(ht, index)    (ht->hashTable[index]).next
#define HGFS_FILE_HT_BUCKET(ht, index)  (&ht->hashTable[index])

#define HGFS_IS_ROOT_FILE(sip, file)    (HGFS_VP_TO_FP(sip->rootVnode) == file)

#if defined(__APPLE__)
#  define SHA1_HASH_LEN SHA_DIGEST_LENGTH
#endif

/*
 * Local functions (prototypes)
 */

static int HgfsVnodeGetInt(struct vnode **vpp,
			   struct HgfsSuperInfo *sip,
			   struct mount *vfsp,
			   const char *fileName,
			   HgfsFileType fileType,
			   HgfsFileHashTable *htp,
			   Bool rootVnode);

/* Allocation/initialization/free of open file state */
static HgfsOpenFile *HgfsAllocOpenFile(const char *fileName, HgfsFileType fileType,
                                       HgfsFileHashTable *htp);
static void HgfsFreeOpenFile(HgfsOpenFile *ofp, HgfsFileHashTable *htp);

/* Acquiring/releasing file state */
static HgfsFile *HgfsGetFile(const char *fileName, HgfsFileType fileType,
			     HgfsFileHashTable *htp);
static void HgfsReleaseFile(HgfsFile *fp, HgfsFileHashTable *htp);
static int HgfsInitFile(HgfsFile *fp, const char *fileName, HgfsFileType fileType);

/* Adding/finding/removing file state from hash table */
static void HgfsAddFile(HgfsFile *fp, HgfsFileHashTable *htp);
static void HgfsRemoveFile(HgfsFile *fp, HgfsFileHashTable *htp);
static HgfsFile *HgfsFindFile(const char *fileName, HgfsFileHashTable *htp);

/* Other utility functions */
static unsigned int HgfsFileNameHash(const char *fileName);
static void HgfsNodeIdHash(const char *fileName, uint32_t fileNameLength,
                           ino_t *outHash);
/*
 * Global functions
 */

/*
 *----------------------------------------------------------------------------
 *
 * HgfsVnodeGet --
 *
 *      Creates a vnode for the provided filename.
 *
 *      This will always allocate a vnode and HgfsOpenFile.  If a HgfsFile
 *      already exists for this filename then that is used, if a HgfsFile doesn't
 *      exist, one is created.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.  The new
 *      vnode is returned locked.
 *
 * Side effects:
 *      If the HgfsFile already exists, its reference count is incremented;
 *      otherwise a HgfsFile is created.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsVnodeGet(struct vnode **vpp,        // OUT: Filled with address of created vnode
             HgfsSuperInfo *sip,        // IN:  Superinfo
             struct mount *vfsp,        // IN:  Filesystem structure
             const char *fileName,      // IN:  Name of this file
             HgfsFileType fileType,     // IN:  Type of file
             HgfsFileHashTable *htp)    // IN:  File hash table
{
   return HgfsVnodeGetInt(vpp, sip, vfsp, fileName, fileType, htp, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVnodeGetRoot --
 *
 *      Creates a root vnode. This should only be called by the VFS mount
 *      function when the filesystem is first being mounted.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.  The new
 *      vnode is returned locked on FreeBSD.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsVnodeGetRoot(struct vnode **vpp,      // OUT: Filled with address of created vnode
		 HgfsSuperInfo *sip,      // IN:  Superinfo
		 struct mount *vfsp,      // IN:  Filesystem structure
		 const char *fileName,    // IN:  Name of this file
		 HgfsFileType fileType,   // IN:  Type of file
		 HgfsFileHashTable *htp)  // IN:  File hash table
{
   return HgfsVnodeGetInt(vpp, sip, vfsp, fileName, fileType, htp, TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVnodePut --
 *
 *      Releases the provided vnode.
 *
 *      This will free the associated vnode.
 *      The HgfsFile's reference count is decremented and, if 0, freed.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

int
HgfsVnodePut(struct vnode *vp,          // IN: Vnode to release
             HgfsFileHashTable *htp)    // IN: Hash table pointer
{
   HgfsOpenFile *ofp;

   ASSERT(vp);
   ASSERT(htp);

   DEBUG(VM_DEBUG_ENTRY, "Entering HgfsVnodePut\n");

   /* Get our private open-file state. */
   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   /*
    * We need to free the open file structure.  This takes care of releasing
    * our reference on the underlying file structure (and freeing it if
    * necessary).
    */
   HgfsFreeOpenFile(ofp, htp);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsNodeIdGet --
 *
 *      Gets the node id for the provided file.  This will only calculate the
 *      node id again if a per-file state structure doesn't yet exist for this
 *      file.  (This situation exists on a readdir since dentries are filled in
 *      rather than creating vnodes.)
 *
 *      In Solaris, node ids are provided in vnodes and inode numbers are
 *      provided in dentries.  For applications to work correctly, we must make
 *      sure that the inode number of a file's dentry and the node id in a file's
 *      vnode match one another.  This poses a problem since vnodes typically do
 *      not exist when dentries need to be created, and once a dentry is created
 *      we have no reference to it since it is copied to the user and freed from
 *      kernel space.  An example of a program that breaks when these values
 *      don't match is /usr/bin/pwd.  This program first acquires the node id of
 *      "." from its vnode, then traverses backwards to ".." and looks for the
 *      dentry in that directory with the inode number matching the node id.
 *      (This is how it obtains the name of the directory it was just in.)
 *      /usr/bin/pwd repeats this until it reaches the root directory, at which
 *      point it concatenates the filenames it acquired along the way and
 *      displays them to the user.  When inode numbers don't match the node id,
 *      /usr/bin/pwd displays an error saying it cannot determine the directory.
 *
 *      The Hgfs protocol does not provide us with unique identifiers for files
 *      since it must support filesystems that do not have the concept of inode
 *      numbers.  Therefore, we must maintain a mapping from filename to node id/
 *      inode numbers.  This is done in a stateless manner by calculating the
 *      SHA-1 hash of the filename.  All points in the Hgfs code that need a node
 *      id/inode number obtain it by either calling this function or directly
 *      referencing the saved node id value in the vnode, if one is available.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsNodeIdGet(HgfsFileHashTable *htp,   // IN:  File hash table
              const char *fileName,     // IN:  Filename to get node id for
              uint32_t fileNameLength,  // IN:  Length of filename
              ino_t *outNodeId)         // OUT: Destination for nodeid
{
   HgfsFile *fp;

   ASSERT(htp);
   ASSERT(fileName);
   ASSERT(outNodeId);

   os_mutex_lock(htp->mutex);

   fp = HgfsFindFile(fileName, htp);
   if (fp) {
      *outNodeId = fp->nodeId;
   } else {
      HgfsNodeIdHash(fileName, fileNameLength, outNodeId);
   }

   os_mutex_unlock(htp->mutex);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsInitFileHashTable --
 *
 *      Initializes the hash table used to track per-file state.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsInitFileHashTable(HgfsFileHashTable *htp)   // IN: Hash table to initialize
{
   int i;

   ASSERT(htp);

   htp->mutex = os_mutex_alloc_init("HgfsHashChain");
   if (!htp->mutex) {
      return HGFS_ERR;
   }

   for (i = 0; i < ARRAYSIZE(htp->hashTable); i++) {
      DblLnkLst_Init(&htp->hashTable[i]);
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDestroyFileHashTable --
 *
 *      Cleanup the hash table used to track per-file state.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsDestroyFileHashTable(HgfsFileHashTable *htp)
{
   ASSERT(htp);
   os_mutex_free(htp->mutex);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileHashTableIsEmpty --
 *
 *      Determines whether the hash table is in an acceptable state to unmount
 *      the file system.
 *
 *      Note that this is not strictly empty: if the only file in the table is
 *      the root of the filesystem and its reference count is 1, this is
 *      considered empty since this is part of the operation of unmounting the
 *      filesystem.
 *
 * Results:
 *      Returns TRUE if the hash table is empty and false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsFileHashTableIsEmpty(HgfsSuperInfo *sip,            // IN: Superinfo
                         HgfsFileHashTable *htp)        // IN: File hash table
{
   int i;

   ASSERT(sip);
   ASSERT(htp);

   os_mutex_lock(htp->mutex);

   /* Traverse each bucket. */
   for (i = 0; i < ARRAYSIZE(htp->hashTable); i++) {
      DblLnkLst_Links *currNode = HGFS_FILE_HT_HEAD(htp, i);

      /* Visit each file in this bucket */
      while (currNode != HGFS_FILE_HT_BUCKET(htp, i)) {
         HgfsFile *currFile = DblLnkLst_Container(currNode, HgfsFile, listNode);

         /*
          * Here we special case the root of our filesystem.  In a correct
          * unmount, the root vnode of the filesystem will have an entry in the
          * hash table and will have a reference count of 1.  We check if the
          * current entry is the root file, and if so, make sure its vnode's
          * reference count is not > 1.  Note that we are not mapping from file
          * to vnode here (which is not possible), we are using the root vnode
          * stored in the superinfo structure.  This is the only vnode that
          * should have multiple references associated with it because whenever
          * someone calls HgfsRoot(), we return that vnode.
          */
         if (HGFS_IS_ROOT_FILE(sip, currFile)) {
            HGFS_VP_VI_LOCK(sip->rootVnode);
            if (!HGFS_VP_ISINUSE(sip->rootVnode, 1)) {
               HGFS_VP_VI_UNLOCK(sip->rootVnode);

               /* This file is okay; skip to the next one. */
               currNode = currNode->next;
               continue;
            }

            DEBUG(VM_DEBUG_FAIL, "HgfsFileHashTableIsEmpty: %s is in use.\n",
		  currFile->fileName);

            HGFS_VP_VI_UNLOCK(sip->rootVnode);
            /* Fall through to failure case */
         }

         /* Fail if a file is found. */
         os_mutex_unlock(htp->mutex);
         DEBUG(VM_DEBUG_FAIL, "HgfsFileHashTableIsEmpty: %s "
               "still in use (file count=%d).\n",
               currFile->fileName, currFile->refCount);
         return FALSE;
      }
   }

   os_mutex_unlock(htp->mutex);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsHandleIsSet --
 *
 *      Determines whether one of vnode's open file handles is currently set.
 *
 * Results:
 *      Returns TRUE if the handle is set, FALSE if the handle is not set.
 *      HGFS_ERR is returned on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsHandleIsSet(struct vnode *vp)            // IN: Vnode to check handle of
{
   HgfsOpenFile *ofp;
   Bool ret;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   os_mutex_lock(ofp->handleMutex);

   ret = ofp->handleRefCount ? TRUE : FALSE;

   os_mutex_unlock(ofp->handleMutex);

   return ret;
 }


/*
 *----------------------------------------------------------------------------
 *
 * HgfsHandleIncrementRefCount --
 *
 *      Increments the reference count of the specified handle associated with
 *      the vnode (vp).
 *
 * Results:
 *      Returns 0 on success and HGFS_ERR on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsHandleIncrementRefCount(struct vnode *vp)           // IN
{
   int ret = 0;
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   os_mutex_lock(ofp->handleMutex);

   ++ofp->handleRefCount;

   os_mutex_unlock(ofp->handleMutex);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetOpenFileHandle --
 *
 *      Sets the file handle for the provided vnode if it has reference count
 *      equal to zero. The reference count of the handle must be increased when
 *      the handle is set. This is done with HgfsHandleIsSet.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      The handle may not be set again until it is cleared.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSetOpenFileHandle(struct vnode *vp,          // IN: Vnode to set handle for
		      HgfsHandle handle)         // IN: Value of handle
{
   int ret = 0;
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   os_mutex_lock(ofp->handleMutex);

   if (ofp->handleRefCount != 0) {
      ret = HGFS_ERR;
      goto out;
   }
   ++ofp->handleRefCount;
   ofp->handle = handle;

   DEBUG(VM_DEBUG_STATE, "HgfsSetOpenFileHandle: set handle for %s to %d\n",
         HGFS_VP_TO_FILENAME(vp), ofp->handle);

out:
   os_mutex_unlock(ofp->handleMutex);

   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "could not set file handle for %s\n",
	    HGFS_VP_TO_FILENAME(vp));
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetOpenFileHandle --
 *
 *      Gets the file handle for the provided vnode.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.  On success,
 *      the value of the vnode's handle is placed in outHandle.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsGetOpenFileHandle(struct vnode *vp,          // IN:  Vnode to get handle for
		      HgfsHandle *outHandle)     // OUT: Filled with value of handle
{
   HgfsOpenFile *ofp;
   int ret = 0;

   ASSERT(vp);
   ASSERT(outHandle);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   os_mutex_lock(ofp->handleMutex);

   if (ofp->handleRefCount == 0) {
      os_mutex_unlock(ofp->handleMutex);
      ret = HGFS_ERR;
   }

   *outHandle = ofp->handle;

   os_mutex_unlock(ofp->handleMutex);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReleaseOpenFileHandle --
 *
 *      Decrements the reference count of one of the handles for the provided
 *      vnode. If the reference count becomes zero, then the handle is cleared.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      The handle may be cleared.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsReleaseOpenFileHandle(struct vnode *vp,          // IN
			  Bool *closed)              // OUT
{
   int ret = 0;
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   *closed = FALSE;

   os_mutex_lock(ofp->handleMutex);

   /* Make sure the reference count is not going negative! */
   ASSERT(ofp->handleRefCount > 0);

   --ofp->handleRefCount;

   /* If the reference count has gone to zero, clear the handle. */
   if (ofp->handleRefCount == 0) {
      DEBUG(VM_DEBUG_LOG, "closing directory handle\n");
      ofp->handle = 0;
      *closed = TRUE;
   } else {
      DEBUG(VM_DEBUG_LOG, "ReleaseOpenFileHandle with a refcount of: %d\n",
	    ofp->handleRefCount);
   }

   os_mutex_unlock(ofp->handleMutex);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsShouldCloseOpenFileHandle --
 *
 *      Checks to see if the next call to HgfsReleaseOpenFileHandle will result in
 *      the file handle being cleared.
 *
 * Results:
 *      Returns TRUE if the file handle will be cleared on the next
 *      HgfsReleaseOpenFileHandle call and FALSE otherwise.
 *      Returns HGFS_ERR if an error was encountered.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsShouldCloseOpenFileHandle(struct vnode *vp)  // IN: Vnode to clear handle for
{
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   os_mutex_lock(ofp->handleMutex);

   if (ofp->handleRefCount == 1) {
      os_mutex_unlock(ofp->handleMutex);
      return TRUE;
   }

   os_mutex_unlock(ofp->handleMutex);

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetOpenFileMode --
 *
 *      Sets the mode of the open file for the provided vnode.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      The mode may not be set again until cleared.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSetOpenFileMode(struct vnode *vp,   // IN: Vnode to set mode for
                    HgfsMode mode)      // IN: Mode to set to
{
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   os_mutex_lock(ofp->modeMutex);

   if (ofp->modeIsSet) {
      DEBUG(VM_DEBUG_FAIL, "**HgfsSetOpenFileMode: mode for %s already set to %d; "
            "cannot set to %d\n", HGFS_VP_TO_FILENAME(vp), ofp->mode, mode);
      os_mutex_unlock(ofp->modeMutex);
      return HGFS_ERR;
   }

   ofp->mode = mode;
   ofp->modeIsSet = TRUE;

   DEBUG(VM_DEBUG_STATE, "HgfsSetOpenFileMode: set mode for %s to %d\n",
         HGFS_VP_TO_FILENAME(vp), ofp->mode);

   os_mutex_unlock(ofp->modeMutex);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetOpenFileMode --
 *
 *      Gets the mode of the file for the provided vnode.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsGetOpenFileMode(struct vnode *vp,   // IN:  Vnode to get mode for
                    HgfsMode *outMode)  // OUT: Filled with mode
{
   HgfsOpenFile *ofp;

   ASSERT(vp);
   ASSERT(outMode);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   os_mutex_lock(ofp->modeMutex);

   if (!ofp->modeIsSet) {
      os_mutex_unlock(ofp->modeMutex);
      return HGFS_ERR;
   }

   *outMode = ofp->mode;

   os_mutex_unlock(ofp->modeMutex);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsClearOpenFileMode --
 *
 *      Clears the mode of the file for the provided vnode.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      The mode may be set again.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsClearOpenFileMode(struct vnode *vp) // IN: Vnode to clear mode for
{
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   os_mutex_lock(ofp->modeMutex);

   ofp->mode = 0;
   ofp->modeIsSet = FALSE;

   DEBUG(VM_DEBUG_STATE, "HgfsClearOpenOpenFileMode: cleared %s's mode\n",
         HGFS_VP_TO_FILENAME(vp));

   os_mutex_unlock(ofp->modeMutex);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileLock --
 *
 *      Locks the HgfsFile associated with the vnode (vp). The type specifies is
 *      we are locking for reads or writes. We only lock the HgfsFile on OS X
 *      because FreeBSD vnodes are locked when handed to the VFS layer and there
 *      is a 1:1 mapping between vnodes and HgfsFile objects so no extra locking
 *      is required.
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsFileLock(struct vnode *vp,  // IN: Vnode to lock
	     HgfsLockType type) // IN: Reader or Writer lock?
{
#if defined(__APPLE__)
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   return HgfsFileLockOfp(ofp, type);
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileUnlock --
 *
 *      Unlocks the HgfsFile associated with the vnode (vp). Results are
 *      undefined the type of lock specified is different than the one that the
 *      vnode was locked with originally.
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsFileUnlock(struct vnode *vp,  // IN: Vnode to unlock
	       HgfsLockType type) // IN: Reader or Writer lock?
{
#if defined(__APPLE__)
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   return HgfsFileUnlockOfp(ofp, type);
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileLockOfp --
 *
 *      Locks the HgfsFile associated with the HgfsOpenFile. This funciton should
 *      only be called by HgfsFileLock
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsFileLockOfp(HgfsOpenFile *ofp, // IN: HgfsOpenFile to lcok
		HgfsLockType type) // IN: Reader or Writer lock?
{
#if defined(__APPLE__)
   ASSERT(ofp);

   if (type == HGFS_READER_LOCK) {
      os_rw_lock_lock_shared(ofp->rwFileLock);
   } else if (type == HGFS_WRITER_LOCK) {
      os_rw_lock_lock_exclusive(ofp->rwFileLock);
   } else {
      return HGFS_ERR;
   }
   return 0;
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileUnlockOfp --
 *
 *      Unlocks the HgfsFile associated with the HgfsOpenFile. This funciton should
 *      only be called by inactive, reclaim and the HgfsFileUnlock routine.
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsFileUnlockOfp(HgfsOpenFile *ofp, // IN: HgfsOpenFile to unlcok
		  HgfsLockType type) // IN: Reader or Writer lock?
{
#if defined(__APPLE__)
   ASSERT(ofp);

   if (type == HGFS_READER_LOCK) {
      os_rw_lock_unlock_shared(ofp->rwFileLock);
   } else if (type == HGFS_WRITER_LOCK) {
      os_rw_lock_unlock_exclusive(ofp->rwFileLock);
   } else {
      return HGFS_ERR;
   }
   return 0;
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 * Local functions (definitions)
 */

/* Internal versions of public functions to allow bypassing htp locking */

/*
 *----------------------------------------------------------------------------
 *
 * HgfsVnodeGetInt --
 *
 *      Creates a vnode for the provided filename.
 *
 *      This will always allocate a vnode and HgfsOpenFile.  If a HgfsFile
 *      already exists for this filename then that is used, if a HgfsFile doesn't
 *      exist, one is created.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.  The new
 *      vnode is returned locked.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

#if defined(__FreeBSD__)
static int
HgfsVnodeGetInt(struct vnode **vpp,        // OUT:  Filled with address of created vnode
                HgfsSuperInfo *sip,        // IN:   Superinfo
                struct mount *vfsp,        // IN:   Filesystem structure
                const char *fileName,      // IN:   Name of this file
                HgfsFileType fileType,     // IN:   Tyoe of file
                HgfsFileHashTable *htp,    // IN:   File hash
		Bool rootVnode)            // IN:   Is this a root vnode?
{
   struct vnode *vp;
   int ret = 0;

   ASSERT(vpp);
   ASSERT(sip);
   ASSERT(vfsp);
   ASSERT(fileName);
   ASSERT(htp);

   /*
    * Here we need to construct the vnode for the kernel as well as our
    * internal file system state.  Our internal state consists of
    * a HgfsOpenFile and a HgfsFile.  The HgfsOpenFile is state kept per-open
    * file; the HgfsFile state is kept per-file.  We have a one-to-one mapping
    * between vnodes and HgfsOpenFiles, and a many-to-one mapping from each of
    * those to a HgfsFile.
    *
    * Note that it appears the vnode is intended to be used as a per-file
    * structure, but we are using it as a per-open-file. The sole exception
    * for this is the root vnode because it is returned by HgfsRoot().  This
    * also means that reference counts for all vnodes except the root should
    * be one; the reference count in our HgfsFile takes on the role of the
    * vnode reference count.
    */
   if ((ret = getnewvnode(HGFS_FS_NAME, vfsp, &HgfsVnodeOps, &vp)) != 0) {
      return ret;
   }

   /*
    * Return a locked vnode to the caller.
    */
   ret = compat_lockmgr(vp->v_vnlock, LK_EXCLUSIVE, NULL, curthread);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "Fatal: could not acquire lock on vnode\n");
      goto destroyVnode;
   }

   /*
    * Now we'll initialize the vnode.  We need to set the file type, vnode
    * operations, flags, filesystem pointer, reference count, and device.
    * After that we'll create our private structures and hang them from the
    * vnode's v_data pointer.
    */
   switch (fileType) {
   case HGFS_FILE_TYPE_REGULAR:
      vp->v_type = VREG;
      break;

   case HGFS_FILE_TYPE_DIRECTORY:
      vp->v_type = VDIR;
      break;

   case HGFS_FILE_TYPE_SYMLINK:
      vp->v_type = VLNK;
      break;

   default:
      /* Hgfs only supports directories and regular files */
      ret = EPERM;
      goto destroyOut;
   }

   /*
    * We now allocate our private open file structure.  This will correctly
    * initialize the per-open-file state, as well as locate (or create if
    * necessary) the per-file state.
    */
   vp->v_data = (void *)HgfsAllocOpenFile(fileName, fileType, htp);
   if (vp->v_data == NULL) {
      ret = ENOMEM;
      goto destroyOut;
   }

   /* If this is going to be the root vnode, we have to mark it as such. */
   if (rootVnode) {
      vp->v_vflag |= VV_ROOT;
   }

   /* Fill in the provided address with the new vnode. */
   *vpp = vp;

   /* Return success */
   return 0;

   /* Cleanup points for errors. */
destroyOut:
   compat_lockmgr(vp->v_vnlock, LK_RELEASE, NULL, curthread);
destroyVnode:
   vrele(vp);
   return ret;
}

#elif defined(__APPLE__)
static int
HgfsVnodeGetInt(struct vnode **vpp,        // OUT
		HgfsSuperInfo *sip,        // IN
		struct mount *vfsp,        // IN
		const char *fileName,      // IN
		HgfsFileType fileType,     // IN
		HgfsFileHashTable *htp,    // IN
		Bool rootVnode)            // IN
{
   struct vnode *vp;
   struct vnode_fsparam params;
   int ret = 0;
   HgfsOpenFile *ofp;

   ASSERT(vpp);
   ASSERT(sip);
   ASSERT(vfsp);
   ASSERT(fileName);
   ASSERT(htp);

   params.vnfs_mp         = vfsp;
   params.vnfs_str        = NULL;
   params.vnfs_dvp        = NULL;
   params.vnfs_fsnode     = NULL;
   params.vnfs_vops       = HgfsVnodeOps;
   params.vnfs_marksystem = FALSE;
   params.vnfs_rdev       = 0;
   params.vnfs_filesize   = 0;
   params.vnfs_cnp        = NULL;
   /* Do not let OS X cache vnodes for us. */
   params.vnfs_flags      = VNFS_NOCACHE | VNFS_CANTCACHE;

   if (rootVnode) {
      params.vnfs_markroot = TRUE;
   } else {
      params.vnfs_markroot   = FALSE;
   }

   /*
    * Now we'll initialize the vnode.  We need to set the file type, vnode
    * operations, flags, filesystem pointer, reference count, and device.
    * After that we'll create our private structures and hang them from the
    * vnode's v_data pointer.
    */
   switch (fileType) {
   case HGFS_FILE_TYPE_REGULAR:
      params.vnfs_vtype = VREG;
      break;

   case HGFS_FILE_TYPE_DIRECTORY:
      params.vnfs_vtype = VDIR;
      break;

   case HGFS_FILE_TYPE_SYMLINK:
      params.vnfs_vtype = VLNK;
      break;

   default:
      /* Hgfs only supports directories and regular files */
      ret = EINVAL;
      goto out;
   }

   /*
    * We now allocate our private open file structure.  This will correctly
    * initialize the per-open-file state, as well as locate (or create if
    * necessary) the per-file state.
    */

   ofp = HgfsAllocOpenFile(fileName, fileType, htp);

   params.vnfs_fsnode = (void *)ofp;
   if (params.vnfs_fsnode == NULL) {
      ret = ENOMEM;
      goto out;
   }


   ret = vnode_create(VNCREATE_FLAVOR, sizeof(params), &params, &vp);
   ofp->vnodep = vp;

   /* Get a soft FS reference to the vnode. This tells the system that the vnode
    * has data associated with it. It is considered a weak reference though, in that
    * it does not prevent the system from reusing the vnode.
    */
   vnode_addfsref(vp);

   if (ret != 0) {
      DEBUG(VM_DEBUG_FAIL, "Failed to create vnode");
      ret = EINVAL;
      goto destroyVnode;
   }

   /* Fill in the provided address with the new vnode. */
   *vpp = vp;

   /* Return success */
   return 0;

   /* Cleanup points for errors. */
destroyVnode:
   vnode_put(vp);
out:
   return ret;
}
#else
   NOT_IMPLEMENTED();
#endif

/* Allocation/initialization/free of open file state */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAllocOpenFile --
 *
 *      Allocates and initializes an open file structure.  Also finds or, if
 *      necessary, creates the underlying HgfsFile per-file state.
 *
 * Results:
 *      Returns a pointer to the open file on success, NULL on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static HgfsOpenFile *
HgfsAllocOpenFile(const char *fileName,         // IN: Name of file
                  HgfsFileType fileType,        // IN: Type of file
                  HgfsFileHashTable *htp)       // IN: Hash table
{
   HgfsOpenFile *ofp;

   ASSERT(fileName);
   ASSERT(htp);

   /*
    * We allocate and initialize our open-file state.
    */
   ofp = os_malloc(sizeof *ofp, M_ZERO | M_WAITOK);
   if (!ofp) {
      DEBUG(VM_DEBUG_FAIL, "Failed to allocate memory");
      return NULL;
   }

   ofp->mode = 0;
   ofp->modeIsSet = FALSE;

   ofp->handleRefCount = 0;
   ofp->handle = 0;

   ofp->handleMutex = os_mutex_alloc_init("hgfs_mtx_handle");
   if (!ofp->handleMutex) {
      goto destroyOut;
   }

   ofp->modeMutex = os_mutex_alloc_init("hgfs_mtx_mode");
   if (!ofp->modeMutex) {
      goto destroyOut;
   }

#if defined(__APPLE__)
   ofp->rwFileLock = os_rw_lock_alloc_init("hgfs_rw_file_lock");
   if (!ofp->rwFileLock) {
      goto destroyOut;
   }
#endif

   /*
    * Now we get a reference to the underlying per-file state.
    */
   ofp->hgfsFile = HgfsGetFile(fileName, fileType, htp);
   if (!ofp->hgfsFile) {
      goto destroyOut;
   }

   /* Success */
   return ofp;

destroyOut:
   ASSERT(ofp);

   if (ofp->handleMutex) {
      os_mutex_free(ofp->handleMutex);
   }

   if (ofp->modeMutex) {
      os_mutex_free(ofp->modeMutex);
   }

#if defined(__APPLE__)
   if (ofp->rwFileLock) {
      os_rw_lock_free(ofp->rwFileLock);
   }
#endif

   os_free(ofp, sizeof *ofp);
   return NULL;

}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFreeOpenFile --
 *
 *      Frees the provided open file.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsFreeOpenFile(HgfsOpenFile *ofp,             // IN: Open file to free
                 HgfsFileHashTable *htp)        // IN: File hash table
{
   ASSERT(ofp);
   ASSERT(htp);

   /*
   * First we release our reference to the underlying per-file state.
   */
   HgfsReleaseFile(ofp->hgfsFile, htp);

   /*
    * Then we destroy anything initialized and free the open file.
    */
#if defined(__APPLE__)
   os_rw_lock_free(ofp->rwFileLock);
#endif
   os_mutex_free(ofp->handleMutex);
   os_mutex_free(ofp->modeMutex);

   os_free(ofp, sizeof *ofp);
}


/* Acquiring/releasing file state */

/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetFile --
 *
 *      Gets the file for the provided filename.
 *
 * Results:
 *      Returns a pointer to the file on success, NULL on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static HgfsFile *
HgfsGetFile(const char *fileName,       // IN: Filename to get file for
	    HgfsFileType fileType,      // IN: Type of file
	    HgfsFileHashTable *htp)     // IN: Hash table to look in
{
   HgfsFile *fp;
   HgfsFile *newfp;
   int err;

   ASSERT(fileName);
   ASSERT(htp);

   /*
    * We try to find the file in the hash table.  If it exists we increment its
    * reference count and return it.
    */
   os_mutex_lock(htp->mutex);

   fp = HgfsFindFile(fileName, htp);
   if (fp) {
      /* Signify our reference to this file. */
      os_mutex_lock(fp->mutex);
      fp->refCount++;
      os_mutex_unlock(fp->mutex);
      goto out;
   }

   /* Drop the lock here, since we can block on os_malloc */
   os_mutex_unlock(htp->mutex);

   /*
    * If it doesn't exist we create one. Ideally this should never fail, since 
    * we are ready to block till we get memory. Note that while we are creating
    * HgfsFile, other thread(s) could also be creating HgfsFile at the same time.
    * Thus once we get memory, we acquire the lock and check hash table to detect
    * any race condition.     
    */
   newfp = os_malloc(sizeof *newfp, M_ZERO | M_WAITOK);

   if (!newfp) {
      /* newfp is NULL already */
      DEBUG(VM_DEBUG_FAIL, "Failed to allocate memory");
      return NULL;
   }

   /* Acquire the lock and check for races */
   os_mutex_lock(htp->mutex);

   fp = HgfsFindFile(fileName, htp);
   if (fp) {
      /* 
       * Some other thread allocated HgfsFile before us. Free newfp 
       * and get the reference on this file.
       */
      os_free(newfp, sizeof(*newfp));
      os_mutex_lock(fp->mutex);
      fp->refCount++;
      os_mutex_unlock(fp->mutex);
      goto out;
   }

   DEBUG(VM_DEBUG_INFO, "HgfsGetFile: allocated HgfsFile for %s.\n", fileName);

   err = HgfsInitFile(newfp, fileName, fileType);
   if (err) {
      os_free(newfp, sizeof(*newfp));
      newfp = NULL;
      goto out;
   }

   /*
    * This is guaranteed to not add a duplicate since after acquiring the lock on the 
    * hash table, we rechecked above to detect if the file was present and have held 
    * the lock until now.
    */
   HgfsAddFile(newfp, htp);
   fp = newfp;

out:
   os_mutex_unlock(htp->mutex);

   DEBUG(VM_DEBUG_DONE, "HgfsGetFile: done\n");
   return fp;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReleaseFile --
 *
 *      Releases a reference to the provided file.  If the reference count of
 *      this file becomes zero, the file structure is removed from the hash table
 *      and freed.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsReleaseFile(HgfsFile *fp,           // IN: File to release
                HgfsFileHashTable *htp) // IN: Hash table to look in/remove from
{
   ASSERT(fp);
   ASSERT(htp);

   /*
    * Decrement this file's reference count.  If it becomes zero, then we
    * remove it from the hash table and free it.
    */
   os_mutex_lock(fp->mutex);

   if ( !(--fp->refCount) ) {
      os_mutex_unlock(fp->mutex);

      /* Remove file from hash table, then clean up. */
      HgfsRemoveFile(fp, htp);

      DEBUG(VM_DEBUG_INFO, "HgfsReleaseFile: freeing HgfsFile for %s.\n",
            fp->fileName);

      os_mutex_free(fp->mutex);
      os_free(fp, sizeof *fp);
      return;
   }

   DEBUG(VM_DEBUG_INFO, "HgfsReleaseFile: %s has %d references.\n",
         fp->fileName, fp->refCount);

   os_mutex_unlock(fp->mutex);
}


/* Allocation/initialization/free of file state */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsInitFile --
 *
 *      Initializes a file structure.
 *
 *      This sets the filename of the file and initializes other structure
 *      elements.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsInitFile(HgfsFile *fp,              // IN: File to initialize
             const char *fileName,      // IN: Name of file
             HgfsFileType fileType)     // IN: Type of file
{
   int len;

   ASSERT(fp);
   ASSERT(fileName);

   /* Make sure the filename will fit. */
   len = strlen(fileName);
   if (len > sizeof fp->fileName - 1) {
      return HGFS_ERR;
   }

   fp->mutex = os_mutex_alloc_init("hgfs_file_mutex_lock");
   if (!fp->mutex) {
      return HGFS_ERR;
   }

   fp->fileNameLength = len;
   memcpy(fp->fileName, fileName, len + 1);
   fp->fileName[fp->fileNameLength] = '\0';

   /*
    * We save the file type so we can recreate a vnode for the HgfsFile without
    * sending a request to the Hgfs Server.
    */
   fp->fileType = fileType;

   /* Initialize the links to place this file in our hash table. */
   DblLnkLst_Init(&fp->listNode);

   /*
    * Fill in the node id.  This serves as the inode number in directory
    * entries and the node id in vnode attributes.
    */
   HgfsNodeIdHash(fp->fileName, fp->fileNameLength, &fp->nodeId);

   /* The caller is the single reference. */
   fp->refCount = 1;

   return 0;
}


/* Adding/finding/removing file state from hash table */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAddFile --
 *
 *      Adds the file to the hash table.
 *
 *      This function must be called with the hash table lock held.  This is done
 *      so adding the file in the hash table can be made with any other
 *      operations (such as previously finding out that this file wasn't in the
 *      hash table).
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsAddFile(HgfsFile *fp,               // IN: File to add
            HgfsFileHashTable *htp)     // IN: Hash table to add to
{
   unsigned int index;

   ASSERT(fp);
   ASSERT(htp);

   index = HgfsFileNameHash(fp->fileName);

   /* Add this file to the end of the bucket's list */
   DblLnkLst_LinkLast(HGFS_FILE_HT_HEAD(htp, index), &fp->listNode);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRemoveFile --
 *
 *      Removes file from the hash table.
 *
 *      Note that unlike the other two hash functions, this one performs its own
 *      locking since the removal doesn't need to be atomic with other
 *      operations.  (This could change in the future if the functions that use
 *      this one are reorganized.)
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsRemoveFile(HgfsFile *fp,            // IN: File to remove
               HgfsFileHashTable *htp)  // IN: Hash table to remove from
{
   ASSERT(fp);
   ASSERT(htp);

   os_mutex_lock(htp->mutex);

   /* Take this file off its list */
   DblLnkLst_Unlink1(&fp->listNode);

   os_mutex_unlock(htp->mutex);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFindFile --
 *
 *      Looks for a filename in the hash table.
 *
 *      This function must be called with the hash table lock held.  This is done
 *      so finding the file in the hash table and using it (after this function
 *      returns) can be atomic.
 *
 * Results:
 *      Returns a pointer to the file if found, NULL otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static HgfsFile *
HgfsFindFile(const char *fileName,      // IN: Filename to look for
             HgfsFileHashTable *htp)    // IN: Hash table to look in
{
   HgfsFile *found = NULL;
   DblLnkLst_Links *currNode;
   unsigned int index;

   ASSERT(fileName);
   ASSERT(htp);

   /* Determine which bucket. */
   index = HgfsFileNameHash(fileName);

   /* Traverse the bucket's list. */
   for (currNode = HGFS_FILE_HT_HEAD(htp, index);
        currNode != HGFS_FILE_HT_BUCKET(htp, index);
        currNode = currNode->next) {
      HgfsFile *curr;
      curr = DblLnkLst_Container(currNode, HgfsFile, listNode);

      if (strcmp(curr->fileName, fileName) == 0) {
         /* We found the file we want. */
         found = curr;
         break;
      }
   }

   /* Return file if found. */
   return found;
}


/* Other utility functions */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileNameHash --
 *
 *      Hashes the filename to get an index into the hash table.  This is known
 *      as the PJW string hash function and it was taken from "Mastering
 *      Algorithms in C".
 *
 * Results:
 *      Returns an index between 0 and HGFS_HT_NR_BUCKETS.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static unsigned int
HgfsFileNameHash(const char *fileName)  // IN: Filename to hash
{
   unsigned int val = 0;

   ASSERT(fileName);

   while (*fileName != '\0') {
      unsigned int tmp;

      val = (val << 4) + (*fileName);
      if ((tmp = (val & 0xf0000000))) {
        val = val ^ (tmp >> 24);
        val = val ^ tmp;
      }

      fileName++;
   }

   return val % HGFS_HT_NR_BUCKETS;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsNodeIdHash --
 *
 *      Hashes the provided filename to generate a node id.
 *
 * Results:
 *      None.  The value of the hash is filled into outHash.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsNodeIdHash(const char *fileName,    // IN:  Filename to hash
               uint32_t fileNameLength, // IN:  Length of the filename
               ino_t *outHash)          // OUT: Location to write hash to
{
   SHA1_CTX hashContext;
   unsigned char digest[SHA1_HASH_LEN];
   int i;

   ASSERT(fileName);
   ASSERT(outHash);

   /* Make sure we start at a consistent state. */
   memset(&hashContext, 0, sizeof hashContext);
   memset(digest, 0, sizeof digest);
   memset(outHash, 0, sizeof *outHash);

   /* Generate a SHA1 hash of the filename */
   SHA1Init(&hashContext);
   SHA1Update(&hashContext, (unsigned const char *)fileName, fileNameLength);
   SHA1Final(digest, &hashContext);

   /*
    * Fold the digest into the allowable size of our hash.
    *
    * For each group of bytes the same size as our output hash, xor the
    * contents of the digest together.  If there are less than that many bytes
    * left in the digest, xor each byte that's left.
    */
   for(i = 0; i < sizeof digest; i += sizeof *outHash) {
      int bytesLeft = sizeof digest - i;

      /* Do a byte-by-byte xor if there aren't enough bytes left in the digest */
      if (bytesLeft < sizeof *outHash) {
         int j;

         for (j = 0; j < bytesLeft; j++) {
            uint8 *outByte = (uint8 *)outHash + j;
            uint8 *inByte = (uint8 *)((uint32_t *)(digest + i)) + j;
            *outByte ^= *inByte;
         }
         break;
      }

      /* Block xor */
      *outHash ^= *((uint32_t *)(digest + i));
   }

   /*
    * Clear the most significant byte so that user space apps depending on
    * a node id/inode number that's only 32 bits won't break.  (For example,
    * gedit's call to stat(2) returns EOVERFLOW if we don't do this.)
    */
#if 0
#  ifndef HGFS_BREAK_32BIT_USER_APPS
   *((uint32_t *)outHash) ^= *((uint32_t *)outHash + 1);
   *((uint32_t *)outHash + 1) = 0;
#  endif
#endif

   DEBUG(VM_DEBUG_INFO, "Hash of: %s (%d) is %u\n", fileName, fileNameLength, *outHash);

   return;
}

