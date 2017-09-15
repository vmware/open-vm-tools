/*********************************************************
 * Copyright (C) 2004-2016 VMware, Inc. All rights reserved.
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
 * hgfsState.c --
 *
 * This implements the functions that provide all the filesystem specific state
 * that is placed underneath the vnodes.
 *
 */


/*
 * Includes
 */

#include "vnode.h"
#include "hgfsState.h"
#include "debug.h"

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "sha1.h"               /* SHA-1 for Node ID calculation */


/*
 * Macros
 */
#define HGFS_VNODE_INIT_FLAG    VNOMAP  /* So we don't have to implement mmap() */
#define HGFS_VNODE_INIT_COUNT   1
#define HGFS_VNODE_INIT_RDEV    VBLK    /* We pretend to be a block device */

#define HGFS_FILE_HT_HEAD(ht, index)    (ht->hashTable[index]).next
#define HGFS_FILE_HT_BUCKET(ht, index)  (&ht->hashTable[index])

#define HGFS_IS_ROOT_FILE(sip, file)    (HGFS_VP_TO_FP(sip->rootVnode) == file)

/*
 * Prototypes for internal functions
 */

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
static inline void HgfsAddFile(HgfsFile *fp, HgfsFileHashTable *htp);
static inline void HgfsRemoveFile(HgfsFile *fp, HgfsFileHashTable *htp);
static HgfsFile *HgfsFindFile(const char *fileName, HgfsFileHashTable *htp);

/* Other utility functions */
static unsigned int HgfsFileNameHash(const char *fileName);
static void HgfsNodeIdHash(const char *fileName, uint32_t fileNameLength,
                           ino64_t *outHash);


/*
 * Public functions
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVnodeGet --
 *
 *    Creates a vnode for the provided filename.
 *
 *    This will always allocate a vnode and HgfsOpenFile.  If a HgfsFile
 *    already exists for this filename then that is used, if a HgfsFile doesn't
 *    exist, one is created.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    If the HgfsFile already exists, its reference count is incremented;
 *    otherwise a HgfsFile is created.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsVnodeGet(struct vnode **vpp,        // OUT: Filled with address of created vnode
             HgfsSuperInfo *sip,        // IN:  Superinfo
             struct vfs *vfsp,          // IN:  Filesystem structure
             const char *fileName,      // IN:  Name of this file
             HgfsFileType fileType,     // IN:  Type of file
             HgfsFileHashTable *htp)    // IN:  File hash table
{
   struct vnode *vp;

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
   /*
    * Note that we no longer embed the vnode in our private structure in
    * Solaris 9.  This was done to simplify this code path and decrease the
    * differences between Solaris 9 and 10.
    */
#  ifdef SOL9
   vp = kmem_zalloc(sizeof *vp, HGFS_ALLOC_FLAG);
#  else
   vp = vn_alloc(HGFS_ALLOC_FLAG);
#  endif
   if (!vp) {
      return HGFS_ERR;
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

   default:
      /* Hgfs only supports directories and regular files */
      goto vnode_error;
   }

   /*
    * Now set the vnode operations.  This is handled differently on Solaris
    * 9 and 10, and we call HgfsSetVnodeOps() to take care of this for us.
    */
   if (HgfsSetVnodeOps(vp) != 0) {
      goto vnode_error;
   }

   /*
    * The vnode cache constructor will have initialized the mutex for us on
    * Solaris 10, so we only do it ourselves on Solaris 9.
    */
#  ifdef SOL9
   mutex_init(&vp->v_lock, NULL, MUTEX_DRIVER, NULL);
#  endif

   vp->v_flag  = HGFS_VNODE_INIT_FLAG;
   vp->v_count = HGFS_VNODE_INIT_COUNT;
   vp->v_vfsp  = vfsp;
   vp->v_rdev  = HGFS_VNODE_INIT_RDEV;

   /*
    * We now allocate our private open file structure.  This will correctly
    * initialize the per-open-file state, as well as locate (or create if
    * necessary) the per-file state.
    */
   vp->v_data = (void *)HgfsAllocOpenFile(fileName, fileType, htp);
   if (!vp->v_data) {
      goto openfile_error;
   }

   /* Fill in the provided address with the new vnode. */
   *vpp = vp;

   /* Return success */
   return 0;

   /* Cleanup points for errors. */
openfile_error:
#  ifdef SOL9
   mutex_destroy(&vp->v_lock);
#  endif
vnode_error:
#  ifdef SOL9
   kmem_free(vp, sizeof *vp);
#  else
   vn_free(vp);
#  endif
   return HGFS_ERR;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVnodePut --
 *
 *    Releases the provided vnode.
 *
 *    This will always free both the vnode and its associated HgfsOpenFile.
 *    The HgfsFile's reference count is decremented and, if 0, freed.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
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

   /* Get our private open-file state. */
   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {          // XXX Maybe ASSERT() this?
      return HGFS_ERR;
   }

   /*
    * We need to free the open file structure.  This takes care of releasing
    * our reference on the underlying file structure (and freeing it if
    * necessary).
    */
   HgfsFreeOpenFile(ofp, htp);

   /*
    * Now we clean up the vnode.
    */
#  ifdef SOL9
   mutex_destroy(&vp->v_lock);
#  endif

#  ifdef SOL9
   kmem_free(vp, sizeof *vp);
#  else
   vn_free(vp);
#  endif

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVnodeDup --
 *
 *    Duplicates the vnode and HgfsOpenFile (per-open state) of a file and
 *    increments the reference count of the underlying HgfsFile.  This function
 *    just calls HgfsVnodeGet with the right arguments.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.  On success
 *    the address of the duplicated vnode is written to newVpp.
 *
 * Side effects:
 *    The HgfsFile for origVp will have an additional reference.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsVnodeDup(struct vnode **newVpp,             // OUT: Given address of new vnode
             struct vnode *origVp,              // IN:  Vnode to duplicate
             struct HgfsSuperInfo *sip,         // IN:  Superinfo pointer
             HgfsFileHashTable *htp)            // IN:  File hash table
{
   ASSERT(newVpp);
   ASSERT(origVp);
   ASSERT(sip);
   ASSERT(htp);

   DEBUG(VM_DEBUG_ALWAYS, "HgfsVnodeDup: duping %s\n", HGFS_VP_TO_FILENAME(origVp));

   return HgfsVnodeGet(newVpp, sip, origVp->v_vfsp, HGFS_VP_TO_FILENAME(origVp),
                       HGFS_VP_TO_HGFSFILETYPE(origVp), &sip->fileHashTable);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileNameToVnode --
 *
 *    Allocates new per-open-file state if a HgfsFile for fileName exists in
 *    the provided file hash table.
 *
 * Results:
 *    Returns 0 on success or a non-zero error code on failure.  On success,
 *    vpp is filled with the address of the new per-open state.
 *
 * Side effects:
 *    The reference count of the HgfsFile for fileName is incremented if it
 *    exists.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsFileNameToVnode(const char *fileName,
                    struct vnode **vpp,
                    struct HgfsSuperInfo *sip,
                    struct vfs *vfsp,
                    HgfsFileHashTable *htp)
{
   HgfsFile *fp;
   HgfsFileType fileType;

   ASSERT(vpp);
   ASSERT(sip);
   ASSERT(vfsp);
   ASSERT(fileName);
   ASSERT(htp);

   /*
    * XXX: This locking here is not totally correct.  Because we are calling
    * HgfsVnodeGet(), which does its own locking on the hash table, we must
    * make finding out the file is in the hash table and then creating our
    * internal state that increments that file's reference count non-atomic.
    *
    * Because of this, it is possible for the file to be in the hash table when
    * we look, then be removed by the time we look for it again.  As
    * a consquence, we will add the file to the hash table then.  This is
    * partially correct in the fact that the file was in the hash table when we
    * looked, but it is partially incorrect since it wasn't in the hash table
    * when we looked again.  In practice this shouldn't cause any problems, but
    * it is possible for a file that is deleted on the host to remain in our
    * hash table longer than it should.
    *
    * A more correct locking scheme was not used because the complexity of
    * doing so outweighed the problems that can occur from this more simple
    * approach.  This approach was also left as is because it provides an
    * optimization to the filesystem by decreasing the number of requests that
    * must be sent significantly.  This optimization can be easily turned off
    * by commenting out the single call to this function in HgfsLookup() in
    * vnode.c.
    *
    * Possible solutions to this are: 1) adding new locks to the top-level
    * public functions (HgfsVnodeGet(), FileNameToVnode(), and NodeIdGet()), 2)
    * bringing the hash table locking up to those same functions, or 3)
    * reimplementing much of the call sequence down to the calls to FindFile()
    * and AddFile() in HgfsGetFile() that is specific to this function.  1 is
    * likely the best option as 2 uses hash table locks for an incorrect
    * purpose and 3 will create a significant amount of repeated code.
    */

   DEBUG(VM_DEBUG_ALWAYS, "HgfsFileNameToVnode: looking for %s\n", fileName);

   mutex_enter(&htp->mutex);

   fp = HgfsFindFile(fileName, htp);
   if (!fp) {
      mutex_exit(&htp->mutex);
      return HGFS_ERR;
   }

   /* Guaranteed by HgfsFindFile(). */
   ASSERT(strcmp(fileName, fp->fileName) == 0);

   /*
    * We save the type of this file with the lock held in case it goes away:
    * see the above comment about locking.
    */
   fileType = fp->fileType;

   mutex_exit(&htp->mutex);

   DEBUG(VM_DEBUG_ALWAYS, "HgfsFileNameToVnode: found %s\n", fileName);

   return HgfsVnodeGet(vpp, sip, vfsp, fileName, fileType, htp);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsNodeIdGet --
 *
 *    Gets the node id for the provided file.  This will only calculate the
 *    node id again if a per-file state structure doesn't yet exist for this
 *    file.  (This situation exists on a readdir since dentries are filled in
 *    rather than creating vnodes.)
 *
 *    In Solaris, node ids are provided in vnodes and inode numbers are
 *    provided in dentries.  For applications to work correctly, we must make
 *    sure that the inode number of a file's dentry and the node id in a file's
 *    vnode match one another.  This poses a problem since vnodes typically do
 *    not exist when dentries need to be created, and once a dentry is created
 *    we have no reference to it since it is copied to the user and freed from
 *    kernel space.  An example of a program that breaks when these values
 *    don't match is /usr/bin/pwd.  This program first acquires the node id of
 *    "." from its vnode, then traverses backwards to ".." and looks for the
 *    dentry in that directory with the inode number matching the node id.
 *    (This is how it obtains the name of the directory it was just in.)
 *    /usr/bin/pwd repeats this until it reaches the root directory, at which
 *    point it concatenates the filenames it acquired along the way and
 *    displays them to the user.  When inode numbers don't match the node id,
 *    /usr/bin/pwd displays an error saying it cannot determine the directory.
 *
 *    The Hgfs protocol does not provide us with unique identifiers for files
 *    since it must support filesystems that do not have the concept of inode
 *    numbers.  Therefore, we must maintain a mapping from filename to node id/
 *    inode numbers.  This is done in a stateless manner by calculating the
 *    SHA-1 hash of the filename.  All points in the Hgfs code that need a node
 *    id/inode number obtain it by either calling this function or directly
 *    referencing the saved node id value in the vnode, if one is available.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsNodeIdGet(HgfsFileHashTable *htp,   // IN:  File hash table
              const char *fileName,     // IN:  Filename to get node id for
              uint32_t fileNameLength,  // IN:  Length of filename
              ino64_t *outNodeId)       // OUT: Destination for nodeid
{
   HgfsFile *fp;

   ASSERT(htp);
   ASSERT(fileName);
   ASSERT(outNodeId);

   mutex_enter(&htp->mutex);

   fp = HgfsFindFile(fileName, htp);
   if (fp) {
      *outNodeId = fp->nodeId;
   } else {
      HgfsNodeIdHash(fileName, fileNameLength, outNodeId);
   }

   mutex_exit(&htp->mutex);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsInitFileHashTable --
 *
 *    Initializes the hash table used to track per-file state.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsInitFileHashTable(HgfsFileHashTable *htp)   // IN: Hash table to initialize
{
   int i;

   ASSERT(htp);

   mutex_init(&htp->mutex, NULL, MUTEX_DRIVER, NULL);

   for (i = 0; i < ARRAYSIZE(htp->hashTable); i++) {
      DblLnkLst_Init(&htp->hashTable[i]);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileHashTableIsEmpty --
 *
 *    Determines whether the hash table is in an acceptable state to unmount
 *    the file system.
 *
 *    Note that this is not strictly empty: if the only file in the table is
 *    the root of the filesystem and its reference count is 1, this is
 *    considered empty since this is part of the operation of unmounting the
 *    filesystem.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    None.
 *
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

   mutex_enter(&htp->mutex);

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
            mutex_enter(&HGFS_ROOT_VNODE(sip)->v_lock);
            if (HGFS_ROOT_VNODE(sip)->v_count <= 1) {
               mutex_exit(&HGFS_ROOT_VNODE(sip)->v_lock);

               /* This file is okay; skip to the next one. */
               currNode = currNode->next;
               continue;
            }

            DEBUG(VM_DEBUG_FAIL, "HgfsFileHashTableIsEmpty: %s has count of %d.\n",
                  currFile->fileName, HGFS_ROOT_VNODE(sip)->v_count);

            mutex_exit(&HGFS_ROOT_VNODE(sip)->v_lock);
            /* Fall through to failure case */
         }

         /* Fail if a file is found. */
         mutex_exit(&htp->mutex);
         DEBUG(VM_DEBUG_FAIL, "HgfsFileHashTableIsEmpty: %s "
               "still in use (file count=%d).\n",
               currFile->fileName, currFile->refCount);
         return FALSE;
      }
   }

   mutex_exit(&htp->mutex);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintFileHashTable --
 *
 *    This will print out all the HgfsFiles that we have in the hash table, as
 *    well as the HgfsFile reference count.  This should help finding places
 *    where there may be loose references on files that prevent an unmount
 *    (EBUSY) when it should be allowed.
 *
 * Results:
 *    Void.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsDebugPrintFileHashTable(HgfsFileHashTable *htp,     // IN: Hash table to print
                            int level)                  // IN: Debugging level
{
   int bucket;

   ASSERT(htp);

   mutex_enter(&htp->mutex);

   for (bucket = 0; bucket < ARRAYSIZE(htp->hashTable); bucket++) {
      DblLnkLst_Links *currNode = HGFS_FILE_HT_HEAD(htp, bucket);

      while (currNode != HGFS_FILE_HT_BUCKET(htp, bucket)) {
         HgfsFile *currFile = DblLnkLst_Container(currNode, HgfsFile, listNode);

         mutex_enter(&currFile->mutex);
         DEBUG(level, "HgfsDebugPrintFileHashTable: "
               "file: %s, count: %d (bucket %d)\n",
               currFile->fileName, currFile->refCount, bucket);
         mutex_exit(&currFile->mutex);

         currNode = currNode->next;
      }
   }

   mutex_exit(&htp->mutex);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsHandleIsSet --
 *
 *    Determines whether handle of the vnode's open file is currently set.
 *
 * Results:
 *    Returns TRUE if the handle is set, FALSE if the handle is not set.
 *    HGFS_ERR is returned on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsHandleIsSet(struct vnode *vp)       // IN: Vnode to check handle of
{
   HgfsOpenFile *ofp;
   Bool isSet;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   mutex_enter(&ofp->handleMutex);
   isSet = ofp->handleIsSet;
   mutex_exit(&ofp->handleMutex);

   return isSet;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetOpenFileHandle --
 *
 *    Sets the open file handle for the provided vnode.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    The handle may not be set again until it is cleared.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSetOpenFileHandle(struct vnode *vp,         // IN: Vnode to set handle for
                      HgfsHandle handle)        // IN: Value of handle
{
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   mutex_enter(&ofp->handleMutex);

   if (ofp->handleIsSet) {
      DEBUG(VM_DEBUG_FAIL, "**HgfsSetOpenFileHandle: handle for %s already set to %d; "
            "cannot set to %d\n", HGFS_VP_TO_FILENAME(vp), ofp->handle, handle);
      mutex_exit(&ofp->handleMutex);
      return HGFS_ERR;
   }

   ofp->handle = handle;
   ofp->handleIsSet = TRUE;

   DEBUG(VM_DEBUG_STATE, "HgfsSetOpenFileHandle: set handle for %s to %d\n",
         HGFS_VP_TO_FILENAME(vp), ofp->handle);

   mutex_exit(&ofp->handleMutex);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetOpenFileHandle --
 *
 *    Gets the open file handle for the provided vnode.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.  On success,
 *    the value of the vnode's handle is placed in outHandle.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsGetOpenFileHandle(struct vnode *vp,         // IN:  Vnode to get handle for
                      HgfsHandle *outHandle)    // OUT: Filled with value of handle
{
   HgfsOpenFile *ofp;

   ASSERT(vp);
   ASSERT(outHandle);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   mutex_enter(&ofp->handleMutex);

   if (!ofp->handleIsSet) {
      DEBUG(VM_DEBUG_FAIL, "**HgfsGetOpenFileHandle: handle for %s is not set.\n",
            HGFS_VP_TO_FILENAME(vp));
      mutex_exit(&ofp->handleMutex);
      return HGFS_ERR;
   }

   *outHandle = ofp->handle;

   mutex_exit(&ofp->handleMutex);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsClearOpenFileHandle --
 *
 *    Clears the open file handle for the provided vnode.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    The handle may be set.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsClearOpenFileHandle(struct vnode *vp)       // IN: Vnode to clear handle for
{
   HgfsOpenFile *ofp;

   ASSERT(vp);

   ofp = HGFS_VP_TO_OFP(vp);
   if (!ofp) {
      return HGFS_ERR;
   }

   mutex_enter(&ofp->handleMutex);

   ofp->handle = 0;
   ofp->handleIsSet = FALSE;

   DEBUG(VM_DEBUG_STATE, "HgfsClearOpenFileHandle: cleared %s's handle\n",
         HGFS_VP_TO_FILENAME(vp));

   mutex_exit(&ofp->handleMutex);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetOpenFileMode --
 *
 *    Sets the mode of the open file for the provided vnode.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    The mode may not be set again until cleared.
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

   mutex_enter(&ofp->modeMutex);

   if (ofp->modeIsSet) {
      DEBUG(VM_DEBUG_FAIL, "**HgfsSetOpenFileMode: mode for %s already set to %d; "
            "cannot set to %d\n", HGFS_VP_TO_FILENAME(vp), ofp->mode, mode);
      mutex_exit(&ofp->modeMutex);
      return HGFS_ERR;
   }

   ofp->mode = mode;
   ofp->modeIsSet = TRUE;

   DEBUG(VM_DEBUG_STATE, "HgfsSetOpenFileMode: set mode for %s to %d\n",
         HGFS_VP_TO_FILENAME(vp), ofp->mode);

   mutex_exit(&ofp->modeMutex);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetOpenFileMode --
 *
 *    Gets the mode of the open file for the provided vnode.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    None.
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

   mutex_enter(&ofp->modeMutex);

   if (!ofp->modeIsSet) {
//      DEBUG(VM_DEBUG_FAIL, "**HgfsGetOpenFileMode: mode for %s is not set.\n",
//            HGFS_VP_TO_FILENAME(vp));
      mutex_exit(&ofp->modeMutex);
      return HGFS_ERR;
   }

   *outMode = ofp->mode;

   mutex_exit(&ofp->modeMutex);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsClearOpenFileMode --
 *
 *    Clears the mode of the open file for the provided vnode.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    The mode may be set again.
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

   mutex_enter(&ofp->modeMutex);

   ofp->mode = 0;
   ofp->modeIsSet = FALSE;
   
   DEBUG(VM_DEBUG_STATE, "HgfsClearOpenFileMode: cleared %s's mode\n",
         HGFS_VP_TO_FILENAME(vp));

   mutex_exit(&ofp->modeMutex);

   return 0;
}


/*
 * Internal functions
 */


/* Allocation/initialization/free of open file state */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAllocOpenFile --
 *
 *    Allocates and initializes an open file structure.  Also finds or, if
 *    necessary, creates the underlying HgfsFile per-file state.
 *
 * Results:
 *    Returns a pointer to the open file on success, NULL on error.
 *
 * Side effects:
 *    None.
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
   ofp = (HgfsOpenFile *)kmem_zalloc(sizeof *ofp, HGFS_ALLOC_FLAG);
   /* kmem_zalloc() cannot fail if given KM_SLEEP; check otherwise */
#  if HGFS_ALLOC_FLAG != KM_SLEEP
   if (!ofp) {
      return NULL;
   }
#  endif

   /* Manually set these since the public functions need the lock. */
   ofp->handle = 0;
   ofp->handleIsSet = FALSE;

   ofp->mode = 0;
   ofp->modeIsSet = FALSE;

   mutex_init(&ofp->handleMutex, NULL, MUTEX_DRIVER, NULL);
   mutex_init(&ofp->handleMutex, NULL, MUTEX_DRIVER, NULL);

   /*
    * Now we get a reference to the underlying per-file state.
    */
   ofp->hgfsFile = HgfsGetFile(fileName, fileType, htp);
   if (!ofp->hgfsFile) {
      kmem_free(ofp, sizeof *ofp);
      return NULL;
   }

   return ofp;

}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFreeOpenFile --
 *
 *    Frees the provided open file.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    None.
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
   mutex_destroy(&ofp->handleMutex);
   mutex_destroy(&ofp->modeMutex);

   kmem_free(ofp, sizeof *ofp);
}


/* Acquiring/releasing file state */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetFile --
 *
 *    Gets the file for the provided filename.
 *
 *    If no file structure exists for this filename, one is created and added
 *    to the hash table.
 *
 * Results:
 *    Returns a pointer to the file on success, NULL on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static HgfsFile *
HgfsGetFile(const char *fileName,       // IN: Filename to get file for
            HgfsFileType fileType,      // IN: Type of file
            HgfsFileHashTable *htp)     // IN: Hash table to look in
{
   HgfsFile *fp;
   int err;

   ASSERT(fileName);
   ASSERT(htp);

   /*
    * We try to find the file in the hash table.  If it exists we increment its
    * reference count and return it.
    */
   mutex_enter(&htp->mutex);

   fp = HgfsFindFile(fileName, htp);
   if (fp) {
      /* Signify our reference to this file. */
      mutex_enter(&fp->mutex);
      fp->refCount++;
      mutex_exit(&fp->mutex);

      mutex_exit(&htp->mutex);
      return fp;
   }

   DEBUG(VM_DEBUG_ALWAYS, "HgfsGetFile: allocated HgfsFile for %s.\n", fileName);

   /*
    * If it doesn't exist we create one, initialize it, and add it to the hash
    * table.
    */
   fp = (HgfsFile *)kmem_zalloc(sizeof *fp, HGFS_ALLOC_FLAG);
#  if HGFS_ALLOC_FLAG != KM_SLEEP
   if (!fp) {
      /* fp is NULL already */
      goto out;
   }
#  endif

   err = HgfsInitFile(fp, fileName, fileType);
   if (err) {
      kmem_free(fp, sizeof *fp);
      fp = NULL;
      goto out;
   }

   /*
    * This is guaranteed to not add a duplicate since we checked above and have
    * held the lock until now.
    */
   HgfsAddFile(fp, htp);

out:
   mutex_exit(&htp->mutex);
   DEBUG(VM_DEBUG_DONE, "HgfsGetFile: done\n");
   return fp;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReleaseFile --
 *
 *    Releases a reference to the provided file.  If the reference count of
 *    this file becomes zero, the file structure is removed from the hash table
 *    and freed.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    None.
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
   mutex_enter(&fp->mutex);

   if ( !(--fp->refCount) ) {
      mutex_exit(&fp->mutex);

      /* Remove file from hash table, then clean up. */
      HgfsRemoveFile(fp, htp);

      DEBUG(VM_DEBUG_ALWAYS, "HgfsReleaseFile: freeing HgfsFile for %s.\n",
            fp->fileName);

      rw_destroy(&fp->rwlock);
      mutex_destroy(&fp->mutex);
      kmem_free(fp, sizeof *fp);
      return;
   }

   DEBUG(VM_DEBUG_ALWAYS, "HgfsReleaseFile: %s has %d references.\n",
         fp->fileName, fp->refCount);

   mutex_exit(&fp->mutex);
}


/* Allocation/initialization/free of file state */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsInitFile --
 *
 *    Initializes a file structure.
 *
 *    This sets the filename of the file and initializes other structure
 *    elements.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    None.
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

   /*
    * The reader/write lock is for the rwlock/rwunlock vnode entry points and
    * the mutex is to protect the reference count on this structure.
    */
   rw_init(&fp->rwlock, NULL, RW_DRIVER, NULL);
   mutex_init(&fp->mutex, NULL, MUTEX_DRIVER, NULL);

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
 *    Adds the file to the hash table.
 *
 *    This function must be called with the hash table lock held.  This is done
 *    so adding the file in the hash table can be made with any other
 *    operations (such as previously finding out that this file wasn't in the
 *    hash table).
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static inline void
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
 *    Removes file from the hash table.
 *
 *    Note that unlike the other two hash functions, this one performs its own
 *    locking since the removal doesn't need to be atomic with other
 *    operations.  (This could change in the future if the functions that use
 *    this one are reorganized.)
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static inline void
HgfsRemoveFile(HgfsFile *fp,            // IN: File to remove
               HgfsFileHashTable *htp)  // IN: Hash table to remove from
{
   ASSERT(fp);
   ASSERT(htp);

   mutex_enter(&htp->mutex);

   /* Take this file off its list */
   DblLnkLst_Unlink1(&fp->listNode);

   mutex_exit(&htp->mutex);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFindFile --
 *
 *    Looks for a filename in the hash table.
 *
 *    This function must be called with the hash table lock held.  This is done
 *    so finding the file in the hash table and using it (after this function
 *    returns) can be atomic.
 *
 * Results:
 *    Returns a pointer to the file if found, NULL otherwise.
 *
 * Side effects:
 *    None.
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
 *    Hashes the filename to get an index into the hash table.  This is known
 *    as the PJW string hash function and it was taken from "Mastering
 *    Algorithms in C".
 *
 * Results:
 *    Returns an index between 0 and HGFS_HT_NR_BUCKETS.
 *
 * Side effects:
 *    None.
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
 *    Hashes the provided filename to generate a node id.
 *
 * Results:
 *    None.  The value of the hash is filled into outHash.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsNodeIdHash(const char *fileName,    // IN:  Filename to hash
               uint32_t fileNameLength, // IN:  Length of the filename
               ino64_t *outHash)   // OUT: Location to write hash to
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
   SHA1Update(&hashContext, (unsigned char *)fileName, fileNameLength);
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
            uint8 *inByte = (uint8 *)((u_longlong_t *)(digest + i)) + j;
            *outByte ^= *inByte;
         }
         break;
      }

      /* Block xor */
      *outHash ^= *((u_longlong_t *)(digest + i));
   }

   /*
    * Clear the most significant byte so that user space apps depending on
    * a node id/inode number that's only 32 bits won't break.  (For example,
    * gedit's call to stat(2) returns EOVERFLOW if we don't do this.)
    */
#  ifndef HGFS_BREAK_32BIT_USER_APPS
   *((uint32_t *)outHash) ^= *((uint32_t *)outHash + 1);
   *((uint32_t *)outHash + 1) = 0;
#  endif

   DEBUG(VM_DEBUG_INFO, "Hash of: %s (%d) is %"FMT64"u\n", fileName, fileNameLength, *outHash);

   return;
}
