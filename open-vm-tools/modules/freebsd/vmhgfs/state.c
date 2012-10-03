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
 *	Vnode and HgfsFile state manipulation routines.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/fcntl.h>

#if defined __FreeBSD__
#  include <sys/libkern.h>
#  include <sys/malloc.h>
#  include "sha1.h"
#  include "compat_freebsd.h"
#define vnode_get(vnode) vget(vnode, LK_SHARED, curthread)
#define vnode_rele(vnode) vrele(vnode)
#define vnode_ref(vnode) vref(vnode)
#elif defined __APPLE__
#  include <string.h>
/*
 * The Mac OS kernel includes the same exact SHA1 routines as those
 * provided by bora/lib/misc. Use the kernel ones under Mac OS.
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
#define LCK_MTX_ASSERT(mutex)

#if defined __APPLE__
#  define SHA1_HASH_LEN SHA_DIGEST_LENGTH

#if defined VMX86_DEVEL
#undef LCK_MTX_ASSERT
#define LCK_MTX_ASSERT(mutex) lck_mtx_assert(mutex, LCK_MTX_ASSERT_OWNED)
#endif

#endif

/*
 * Local functions (prototypes)
 */

static int HgfsVnodeGetInt(struct vnode **vpp,
                           struct vnode *dvp,
                           struct HgfsSuperInfo *sip,
                           struct mount *vfsp,
                           const char *fileName,
                           HgfsFileType fileType,
                           HgfsFileHashTable *htp,
                           Bool rootVnode,
                           Bool createFile,
                           int permissions,
                           off_t fileSize);

/* Allocation/initialization/free of open file state */
static HgfsFile *HgfsAllocFile(const char *fileName, HgfsFileType fileType,
                               struct vnode *dvp, HgfsFileHashTable *htp,
                               int permissions, off_t fileSize);

/* Acquiring/releasing file state */
static HgfsFile *HgfsInsertFile(const char *fileName,
                                HgfsFile *fp,
                                HgfsFileHashTable *htp);
static void HgfsReleaseFile(HgfsFile *fp, HgfsFileHashTable *htp);
static int HgfsInitFile(HgfsFile *fp, struct vnode *dvp, const char *fileName,
                        HgfsFileType fileType, int permissions, off_t fileSize);
static void HgfsFreeFile(HgfsFile *fp);

/* Adding/finding/removing file state from hash table */
static void HgfsAddFile(HgfsFile *fp, HgfsFileHashTable *htp);
static HgfsFile *HgfsFindFile(const char *fileName, HgfsFileHashTable *htp);

/* Other utility functions */
static unsigned int HgfsFileNameHash(const char *fileName);
static void HgfsNodeIdHash(const char *fileName, uint32_t fileNameLength,
                           ino_t *outHash);
static Bool HgfsIsModeCompatible(HgfsMode requestedMode, HgfsMode existingMode);
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
 *      This will always allocate a vnode and HgfsFile.  If a HgfsFile
 *      already exists for this filename then that is used, if a HgfsFile doesn't
 *      exist, one is created.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.  The new
 *      vnode is returned locked.
 *
 * Side effects:
 *      If the HgfsFile already exists and createFile is TRUE then the EEXIST error
 *      is returned. Otherwise if the HgfsFile already exists its reference count
 *      is incremented.
 *      If HgfsFile with the given name does not exist then HgfsFile is created.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsVnodeGet(struct vnode **vpp,        // OUT: Filled with address of created vnode
             struct vnode *dvp,         // IN:  Parent directory vnode
             HgfsSuperInfo *sip,        // IN:  Superinfo
             struct mount *vfsp,        // IN:  Filesystem structure
             const char *fileName,      // IN:  Name of this file
             HgfsFileType fileType,     // IN:  Type of file
             HgfsFileHashTable *htp,    // IN:  File hash table
             Bool createFile,           // IN:  Creating a new file or open existing?
             int permissions,           // IN: Permissions for the created file
             off_t fileSize)            // IN: File size if the vnode is VREG
{
   return HgfsVnodeGetInt(vpp, dvp, sip, vfsp, fileName, fileType, htp, FALSE,
                          createFile, permissions, fileSize);
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
   return HgfsVnodeGetInt(vpp, NULL, sip, vfsp, fileName, fileType, htp, TRUE,
                          FALSE, 0, 0);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReleaseVnodeContext --
 *
 *      Releases context for the provided vnode.
 *
 *      This will free the context information associated vnode.
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
HgfsReleaseVnodeContext(struct vnode *vp,          // IN: Vnode to release
                        HgfsFileHashTable *htp)    // IN: Hash table pointer
{
   HgfsFile *fp;

   ASSERT(vp);
   ASSERT(htp);

   DEBUG(VM_DEBUG_ENTRY, "Entering HgfsVnodePut\n");

   /* Get our private open-file state. */
   fp = HGFS_VP_TO_FP(vp);
   ASSERT(fp);

   /* We need to release private HGFS information asosiated with the vnode. */
   HgfsReleaseFile(fp, htp);

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
         DEBUG(VM_DEBUG_FAIL, "HgfsFileHashTableIsEmpty: %s still in use.\n",
               currFile->fileName);
         return FALSE;
      }
   }

   os_mutex_unlock(htp->mutex);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsCheckAndReferenceHandle --
 *
 *      Determines whether one of vnode's open file handles is currently set.
 *      If the handle is set the function increments its reference count.
 *      The function must be called while holding handleLock from the correspondent
 *      HgfsFile structure.  
 *
 * Results:
 *      Returns 0 if the handle is set and had been referenced,
 *              EACCES if the handle is set but has an incompatible open mode,
 *              ENOENT if no handle is set for the vnode
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsCheckAndReferenceHandle(struct vnode *vp,         // IN: Vnode to check handle of
                            int requestedOpenMode,    // IN: Requested open mode
                            HgfsOpenType openType)    // IN: Requested open type
{
   HgfsFile *fp;
   int ret = 0;

   ASSERT(vp);

   fp = HGFS_VP_TO_FP(vp);
   ASSERT(fp);

   if (0 == fp->handleRefCount && 0 == fp->intHandleRefCount) {
      ret = ENOENT;
      DEBUG(VM_DEBUG_LOG, "No handle: mode %d type %d\n", requestedOpenMode, openType);
      goto exit;
   }

   if (!HgfsIsModeCompatible(requestedOpenMode, fp->mode)) {
      ret = EACCES;
      DEBUG(VM_DEBUG_LOG, "Incompatible modes: %d %d\n", requestedOpenMode, fp->mode);
      goto exit;
   }

   DEBUG(VM_DEBUG_LOG, "Compatible handle: type %d mapped %d count %d\n",
         openType, fp->mmapped, fp->handleRefCount);

   /*
    * Do nothing for subsequent mmap/read reference requests.
    * For mmap the OS layer invokes mnomap only once
    * for multiple mmap calls.
    * For read we only need to reference the first real need to open, i.e. ENOENT
    * is returned when there isn't a compatible handle.
    */
   if (OPENREQ_MMAP == openType && fp->mmapped) {
      DEBUG(VM_DEBUG_LOG, "Mmapped: already referenced %d %d\n", requestedOpenMode, fp->mode);
      goto exit;
   }

   if (OPENREQ_READ == openType) {
      DEBUG(VM_DEBUG_LOG, "Open for Read: already referenced %d %d\n", requestedOpenMode, fp->mode);
      goto exit;
   }

   /*
    * Reference the handle for the open.
    * For the regular open and memory map calls we increment the normal
    * count, for all others (e.g. create) it is an internal increment.
    */
   if (OPENREQ_OPEN != openType && OPENREQ_MMAP != openType) {
      fp->intHandleRefCount++;
      DEBUG(VM_DEBUG_LOG, "Internal Handle Ref Cnt %d\n", fp->intHandleRefCount);
   } else {
      fp->handleRefCount++;
      DEBUG(VM_DEBUG_LOG, "Handle Ref Cnt %d\n", fp->handleRefCount);
   }

   if (!(fp->mmapped) && OPENREQ_MMAP == openType) {
      fp->mmapped = TRUE;
   }

exit:
   return ret;
 }


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetOpenFileHandle --
 *
 *      Sets the file handle for the provided vnode if it has reference count
 *      equal to zero. The reference count of the handle must be increased when
 *      the handle is set. This is done with HgfsCheckAndReferenceHandle.
 *      Caller must hold handleLock when invoking the function.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The handle may not be set again until it is cleared.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsSetOpenFileHandle(struct vnode *vp,          // IN: Vnode to set handle for
                      HgfsHandle handle,         // IN: Value of handle
                      HgfsMode openMode,         // IN: Mode assosiated with the handle
                      HgfsOpenType openType)     // IN: type of open for VNOP_ call
{
   HgfsFile *fp;

   ASSERT(vp);

   fp = HGFS_VP_TO_FP(vp);
   ASSERT(fp);

   fp->handle = handle;
   fp->mode = openMode;
   fp->handleRefCount = 1;
   if (OPENREQ_OPEN == openType) {
      fp->handleRefCount = 1;
   } else {
      fp->intHandleRefCount = 1;
   }
   DEBUG(VM_DEBUG_STATE, "File %s handle %d ref Cnt %d Int Ref Cnt %d\n",
         HGFS_VP_TO_FILENAME(vp), fp->handle, fp->handleRefCount, fp->intHandleRefCount);
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
   HgfsFile *fp;
   int ret = 0;

   ASSERT(vp);
   ASSERT(outHandle);

   fp = HGFS_VP_TO_FP(vp);
   ASSERT(fp);

   os_rw_lock_lock_shared(fp->handleLock);

   if (fp->handleRefCount == 0) {
      ret = HGFS_ERR;
   } else {
      *outHandle = fp->handle;
   }

   os_rw_lock_unlock_shared(fp->handleLock);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReleaseOpenFileHandle --
 *
 *      Decrements the reference count of one of the handles for the provided
 *      vnode. If the reference count becomes zero, then the handle is cleared and
 *      the original handle is retruned to the caller.
 *
 * Results:
 *      Returns new handle reference count.
 *      When the returned value is 0 returns the file handle which need to be closed
 *      on the host.
 *      Returns special value -1 if the handle had not been open.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsReleaseOpenFileHandle(struct vnode *vp,            // IN: correspondent vnode
                          HgfsOpenType openType,       // IN: open type to release
                          HgfsHandle *handleToClose)   // OUT: Host handle to close
{
   int ret = -1;
   HgfsFile *fp;

   ASSERT(vp);

   fp = HGFS_VP_TO_FP(vp);
   ASSERT(fp);

   os_rw_lock_lock_exclusive(fp->handleLock);

   /* Make sure the reference count is not going negative! */
   ASSERT(fp->handleRefCount >= 0 || fp->intHandleRefCount > 0);

   if (fp->handleRefCount > 0 || fp->intHandleRefCount > 0) {
      if (fp->handleRefCount > 0) {
         --fp->handleRefCount;
      }
      /*
       * We don't issue explicit closes for internal opens (read/create), so
       * always decrement the internal count here.
       */
      if (fp->intHandleRefCount > 0) {
         --fp->intHandleRefCount;
      }
      /* Return the real not internal count. */
      ret = fp->handleRefCount;
      /* If unmapping clear our flag. */
      if (OPENREQ_MMAP == openType) {
         fp->mmapped = FALSE;
      }

      /* If the reference count has gone to zero, clear the handle. */
      if (ret == 0) {
         DEBUG(VM_DEBUG_LOG, "Last open closing handle %d\n", fp->handle);
         *handleToClose = fp->handle;
         fp->handle = 0;
         fp->intHandleRefCount = 0;
      } else {
         DEBUG(VM_DEBUG_LOG, "ReleaseOpenFileHandle: refCount: %d intRefCount %d\n",
               fp->handleRefCount, fp->intHandleRefCount);
      }
   }
   os_rw_lock_unlock_exclusive(fp->handleLock);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsLookupExistingVnode --
 *
 *      Locates existing vnode in the hash table that matches given file name.
 *      If a vnode that corresponds the given name does not exists then the function
 *      returns ENOENT.
 *      If the vnode exists the function behavior depends on failIfExist parameter.
 *      When failIfExist is true then the function return EEXIST, otherwise 
 *      function references the vnode, assigns vnode pointer to vpp and return 0.
 *
 * Results:
 *      
 *      Returns 0 if existing vnode is found and its address is returned in vpp or
 *      an error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsLookupExistingVnode(const char* fileName,
                        HgfsFileHashTable *htp,
                        Bool failIfExist,
                        struct vnode **vpp)
{
   HgfsFile* existingFp;
   int err = ENOENT;
   os_mutex_lock(htp->mutex);
   /* First verify if a vnode for the filename is already allocated. */
   existingFp = HgfsFindFile(fileName, htp);
   if (existingFp != NULL) {
      DEBUG(VM_DEBUG_LOG, "Found existing vnode for %s\n", fileName);
      if (failIfExist) {
         err = EEXIST;
      } else {
         err = vnode_get(existingFp->vnodep);
         if (err == 0) {
            *vpp = existingFp->vnodep;
         } else {
            /* vnode exists but unusable, remove HGFS context assosiated with it. */
            DEBUG(VM_DEBUG_FAIL, "Removing HgfsFile assosiated with an unusable vnode\n");
            DblLnkLst_Unlink1(&existingFp->listNode);
            err = ENOENT;
         }
      }
   }
   os_mutex_unlock(htp->mutex);
   return err;
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
 *      If a HgfsFile already exists for this filename then it is used and the associated
 *      vnode is referenced and returned.
 *      if a HgfsFile doesn't exist, a new vnode and HgfsFile structure is created.
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

#if defined __FreeBSD__
static int
HgfsVnodeGetInt(struct vnode **vpp,        // OUT:  Filled with address of created vnode
                struct vnode *dvp,         // IN:   Parent directory vnode
                HgfsSuperInfo *sip,        // IN:   Superinfo
                struct mount *vfsp,        // IN:   Filesystem structure
                const char *fileName,      // IN:   Name of this file
                HgfsFileType fileType,     // IN:   Tyoe of file
                HgfsFileHashTable *htp,    // IN:   File hash
                Bool rootVnode,            // IN:   Is this a root vnode?
                Bool fileCreate,           // IN:   Is it a new file creation?
                int permissions,           // IN:   Permissions for new files
                off_t fileSize)            // IN:   Size of the file
{
   struct vnode *vp;
   int ret;

   HgfsFile *fp;
   HgfsFile *existingFp;

   ASSERT(vpp);
   ASSERT(sip);
   ASSERT(vfsp);
   ASSERT(fileName);
   ASSERT(htp);
   ASSERT(dvp != NULL || rootVnode);

   /* First verify if a vnode for the filename is already allocated. */
   ret = HgfsLookupExistingVnode(fileName, htp, fileCreate, vpp);
   if (ret != ENOENT) {
      return ret;
   }

   /*
    * Here we need to construct the vnode for the kernel as well as our
    * internal file system state.  Our internal state described by
    * HgfsFile structure which is kept per-file. There is no state information assosiated
    * with file descriptor. The reason is that when OS invokes vnode methods
    * it does not provide information about file descriptor that was used to initiate the
    * IO. We have a one-to-one mapping between vnodes and HgfsFiles.
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

   /* We now allocate our private open file structure.    */
   fp = (void *)HgfsAllocFile(fileName, fileType, dvp, htp, permissions, fileSize);
   if (fp == NULL) {
      ret = ENOMEM;
      goto destroyOut;
   }

   fp->vnodep = vp;
   vp->v_data = fp;
   /* If this is going to be the root vnode, we have to mark it as such. */
   if (rootVnode) {
      vp->v_vflag |= VV_ROOT;
   }

   existingFp = HgfsInsertFile(fileName, fp, htp);

   if (existingFp != NULL) { // Race occured, another thread inserted a node ahead of us
      if (fileCreate) {
         ret = EEXIST;
         goto destroyOut;
      }
      compat_lockmgr(vp->v_vnlock, LK_RELEASE, NULL, curthread);
      vput(vp);
      vp = existingFp->vnodep;
      /*
       * Return a locked vnode to the caller.
       */
      ret = compat_lockmgr(vp->v_vnlock, LK_EXCLUSIVE, NULL, curthread);
      if (ret) {
         DEBUG(VM_DEBUG_FAIL, "Fatal: could not acquire lock on vnode\n");
         goto destroyVnode;
      }
      HgfsFreeFile(fp);
   }

   /* Fill in the provided address with the new vnode. */
   *vpp = vp;

   /* Return success */
   return 0;

   /* Cleanup points for errors. */
destroyOut:
   compat_lockmgr(vp->v_vnlock, LK_RELEASE, NULL, curthread);
destroyVnode:
   vput(vp);
   return ret;
}

#elif defined __APPLE__
static int
HgfsVnodeGetInt(struct vnode **vpp,        // OUT
                struct vnode *dvp,         // IN
                HgfsSuperInfo *sip,        // IN
                struct mount *vfsp,        // IN
                const char *fileName,      // IN
                HgfsFileType fileType,     // IN
                HgfsFileHashTable *htp,    // IN
                Bool rootVnode,            // IN
                Bool fileCreate,           // IN
                int permissions,           // IN
                off_t fileSize)            // IN
{
   struct vnode *vp;
   struct vnode_fsparam params;
   int ret;
   HgfsFile *fp = NULL;
   HgfsFile *existingFp;

   ASSERT(vpp);
   ASSERT(sip);
   ASSERT(vfsp);
   ASSERT(fileName);
   ASSERT(htp);

   /* First verify if a vnode for the filename is already allocated. */
   ret = HgfsLookupExistingVnode(fileName, htp, fileCreate, vpp);
   if (ret != ENOENT) {
      return ret;
   }

   params.vnfs_mp         = vfsp;
   params.vnfs_str        = "hgfs";
   params.vnfs_dvp        = dvp;
   params.vnfs_fsnode     = NULL;
   params.vnfs_vops       = HgfsVnodeOps;
   params.vnfs_marksystem = FALSE;
   params.vnfs_rdev       = 0;
   params.vnfs_filesize   = fileSize;
   params.vnfs_cnp        = NULL;
   /* Do not let Mac OS cache vnodes for us. */
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

   fp = HgfsAllocFile(fileName, fileType, dvp, htp, permissions, fileSize);

   params.vnfs_fsnode = (void *)fp;
   if (params.vnfs_fsnode == NULL) {
      ret = ENOMEM;
      goto out;
   }

   ret = vnode_create(VNCREATE_FLAVOR, sizeof(params), &params, &vp);
   if (ret != 0) {
      DEBUG(VM_DEBUG_FAIL, "Failed to create vnode");
      goto out;
   }

   fp->vnodep = vp;

   existingFp = HgfsInsertFile(fileName, fp, htp);

   if (existingFp != NULL) { // Race occured, another thread inserted a node ahead of us
      vnode_put(vp);
      if (fileCreate) {
         ret = EEXIST;
         goto out;
      }
      vp = existingFp->vnodep;
      HgfsFreeFile(fp);
   } else {
      /* Get a soft FS reference to the vnode. This tells the system that the vnode
       * has data associated with it. It is considered a weak reference though, in that
       * it does not prevent the system from reusing the vnode.
       */
      vnode_addfsref(vp);
   }

   /* Fill in the provided address with the new vnode. */
   *vpp = vp;

   /* Return success */
   return 0;

out:
   if (fp) {
      HgfsFreeFile(fp);
   }
   return ret;
}
#else
   NOT_IMPLEMENTED();
#endif

/* Allocation/initialization/free of open file state */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAllocFile --
 *
 *      Allocates and initializes a file structure.
 *
 * Results:
 *      Returns a pointer to the open file on success, NULL on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static HgfsFile *
HgfsAllocFile(const char *fileName,         // IN: Name of file
              HgfsFileType fileType,        // IN: Type of file
              struct vnode *dvp,            // IN: Parent directory vnode
              HgfsFileHashTable *htp,       // IN: Hash table
              int permissions,              // IN: permissions for creating new files
              off_t fileSize)               // IN: file size
{
   HgfsFile *fp;
   fp = os_malloc(sizeof *fp, M_ZERO | M_WAITOK);

   if (fp != NULL) {
      DEBUG(VM_DEBUG_INFO, "HgfsGetFile: allocated HgfsFile for %s.\n", fileName);

      if (HgfsInitFile(fp, dvp, fileName, fileType, permissions, fileSize) != 0) {
         DEBUG(VM_DEBUG_FAIL, "Failed to initialize HgfsFile");
         os_free(fp, sizeof(*fp));
         fp = NULL;
      }
   } else {
      DEBUG(VM_DEBUG_FAIL, "Failed to allocate memory");
   }
   return fp;
}

/* Acquiring/releasing file state */

/*
 *----------------------------------------------------------------------------
 *
 * HgfsInsertFile --
 *
 *      Inserts a HgfsFile object into the hash table if the table does not
 *      contain an object with the same name. 
 *      If an object with the same name already exists in the hash table
 *      then does nothing and just returns pointer to the existing object.
 *
 * Results:
 *      Returns a pointer to the file if there is a name collision, NULL otherwise.
 *
 * Side effects:
 *      If there is a name collision adds reference to vnode IOrefcount.
 *
 *----------------------------------------------------------------------------
 */

static HgfsFile *
HgfsInsertFile(const char *fileName,       // IN: Filename to get file for
               HgfsFile *fp,               // IN: HgfsFile object to insert
               HgfsFileHashTable *htp)     // IN: Hash table to look in
{
   HgfsFile *existingFp = NULL;

   ASSERT(fileName);
   ASSERT(htp);

   /*
    * We try to find the file in the hash table.  If it exists we increment its
    * reference count and return it.
    */
   os_mutex_lock(htp->mutex);

   existingFp = HgfsFindFile(fileName, htp);
   if (existingFp) { // HgfsFile with this name already exists
      int ret = vnode_get(existingFp->vnodep);
      if (ret != 0) {
         /*
          * It is not clear why vnode_get may fail while there is HgfsFile in
          * our hash table. Most likely it will never happen.
          * However if this ever occur the safest approach is to remove
          * the HgfsFile structure from the hash table but do dont free it.
          * It should be freed later on when the vnode is recycled.
          */
         DblLnkLst_Unlink1(&existingFp->listNode);
         existingFp = NULL;
      }
   }
   if (existingFp == NULL) {
      HgfsAddFile(fp, htp);
   }

   os_mutex_unlock(htp->mutex);
   return existingFp;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReleaseFile --
 *
 * Removes HgfsFile structure from the hash table and releases it.
 *
 * Results:
 *      None.
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

   DEBUG(VM_DEBUG_INFO, "HgfsReleaseFile: freeing HgfsFile for %s.\n",
         fp->fileName);
   /* Take this file off its list */
   os_mutex_lock(htp->mutex);
   DblLnkLst_Unlink1(&fp->listNode);
   os_mutex_unlock(htp->mutex);

   HgfsFreeFile(fp);
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
             struct vnode *dvp,         // IN: Paretn directory vnode
             const char *fileName,      // IN: Name of file
             HgfsFileType fileType,     // IN: Type of file
             int permissions,           // IN: Permissions for new files
             off_t fileSize)            // IN: File size
{
   int len;

   ASSERT(fp);
   ASSERT(fileName);

   /* Make sure the filename will fit. */
   len = strlen(fileName);
   if (len > sizeof fp->fileName - 1) {
      return HGFS_ERR;
   }

   fp->fileNameLength = len;
   memcpy(fp->fileName, fileName, len + 1);
   fp->fileName[fp->fileNameLength] = '\0';

   /*
    * We save the file type so we can recreate a vnode for the HgfsFile without
    * sending a request to the Hgfs Server.
    */
   fp->permissions = permissions;

   /* Initialize the links to place this file in our hash table. */
   DblLnkLst_Init(&fp->listNode);

   /*
    * Fill in the node id.  This serves as the inode number in directory
    * entries and the node id in vnode attributes.
    */
   HgfsNodeIdHash(fp->fileName, fp->fileNameLength, &fp->nodeId);

   fp->mode = 0;
   fp->modeIsSet = FALSE;

   fp->handleRefCount = 0;
   fp->intHandleRefCount = 0;
   fp->handle = 0;
   fp->mmapped = FALSE;
   fp->fileSize = fileSize;

   fp->handleLock = os_rw_lock_alloc_init("hgfs_rw_handle_lock");
   if (!fp->handleLock) {
      goto destroyOut;
   }

   fp->modeMutex = os_mutex_alloc_init("hgfs_mtx_mode");
   if (!fp->modeMutex) {
      goto destroyOut;
   }

#if defined __APPLE__
   fp->rwFileLock = os_rw_lock_alloc_init("hgfs_rw_file_lock");
   if (!fp->rwFileLock) {
      goto destroyOut;
   }
   DEBUG(VM_DEBUG_LOG, "fp = %p, Lock = %p .\n", fp, fp->rwFileLock);

#endif

   fp->parent = dvp;
   if (dvp != NULL) {
      vnode_ref(dvp);
   }

   /* Success */
   return 0;

destroyOut:
   ASSERT(fp);

   if (fp->handleLock) {
      os_rw_lock_free(fp->handleLock);
   }

   if (fp->modeMutex) {
      os_mutex_free(fp->modeMutex);
   }

#if defined __APPLE__
   if (fp->rwFileLock) {
      os_rw_lock_free(fp->rwFileLock);
   }
   DEBUG(VM_DEBUG_LOG, "Destroying fp = %p, Lock = %p .\n", fp, fp->rwFileLock);
#endif

   os_free(fp, sizeof *fp);
   return HGFS_ERR;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFreeFile --
 *
 *      Preforms necessary cleanup and frees the memory allocated for HgfsFile.
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
HgfsFreeFile(HgfsFile *fp)   // IN: HgfsFile structure to free
{
   ASSERT(fp);
   os_rw_lock_free(fp->handleLock);
   os_mutex_free(fp->modeMutex);
#if defined __APPLE__
   DEBUG(VM_DEBUG_LOG, "Trace enter, fp = %p, Lock = %p .\n", fp, fp->rwFileLock);
   os_rw_lock_free(fp->rwFileLock);
#endif
   if (fp->parent != NULL) {
      vnode_rele(fp->parent);
   }
   os_free(fp, sizeof *fp);
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

   LCK_MTX_ASSERT(htp->mutex);
   index = HgfsFileNameHash(fp->fileName);

   /* Add this file to the end of the bucket's list */
   DblLnkLst_LinkLast(HGFS_FILE_HT_HEAD(htp, index), &fp->listNode);
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
   LCK_MTX_ASSERT(htp->mutex);

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


/*
 *----------------------------------------------------------------------------
 *
 * HgfsIsModeCompatible --
 *
 *      Verifies if the requested open mode for the file is compatible
 *      with already assigned open mode.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
HgfsIsModeCompatible(HgfsMode requestedMode,   // IN: Requested open mode
                     HgfsMode existingMode)    // IN: Existing open mode
{
   DEBUG(VM_DEBUG_LOG, "Compare mode %d with %d.\n", requestedMode, existingMode);
   return (existingMode == HGFS_OPEN_MODE_READ_WRITE ||
           requestedMode == existingMode);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetFileSize --
 *
 *      Notifies virtual memory system that file size has changed.
 *      Required for memory mapped files to work properly.
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
HgfsSetFileSize(struct vnode *vp,  // IN: vnode which file size has changed
                off_t newSize)     // IN: new value for file size
{
   HgfsFile *fp;

   ASSERT(vp);
   fp = HGFS_VP_TO_FP(vp);
   if (fp->fileSize != newSize) {
      fp->fileSize = newSize;
      os_SetSize(vp, newSize);
   }
}
