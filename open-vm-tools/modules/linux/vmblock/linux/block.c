/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * block.c --
 *
 *      Blocking operation implementions for the vmblock driver.
 */

/* os.h includes necessary OS-specific headers. */
#include "os.h"

#if defined(vmblock_fuse)
#elif defined(linux)
# include "vmblockInt.h"
#elif defined(sun)
# include "module.h"
#elif defined(__FreeBSD__)
# include "vmblock_k.h"
#endif
#include "block.h"
#include "stubs.h"
#include "dbllnklst.h"

typedef struct BlockInfo {
   DblLnkLst_Links links;
   os_atomic_t refcount;
   os_blocker_id_t blocker;
   os_completion_t completion;
   char filename[OS_PATH_MAX];
} BlockInfo;


/* XXX: Is it worth turning this into a hash table? */
static DblLnkLst_Links blockedFiles;
static os_rwlock_t blockedFilesLock;
static os_kmem_cache_t *blockInfoCache = NULL;

/* Utility functions */
static Bool BlockExists(const char *filename);
static BlockInfo *GetBlock(const char *filename, const os_blocker_id_t blocker);
static BlockInfo *AllocBlock(os_kmem_cache_t *cache,
                             const char *filename, const os_blocker_id_t blocker);
static void FreeBlock(os_kmem_cache_t *cache, BlockInfo *block);


/*
 *----------------------------------------------------------------------------
 *
 * BlockInit --
 *
 *    Initializes blocking portion of module.
 *
 * Results:
 *    Zero on success, error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
BlockInit(void)
{
   ASSERT(!blockInfoCache);

   blockInfoCache = os_kmem_cache_create("blockInfoCache",
                                         sizeof (BlockInfo),
                                         0,
                                         NULL);
   if (!blockInfoCache) {
      return OS_ENOMEM;
   }

   DblLnkLst_Init(&blockedFiles);
   os_rwlock_init(&blockedFilesLock);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * BlockCleanup --
 *
 *    Cleans up the blocking portion of the module.
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
BlockCleanup(void)
{
   ASSERT(blockInfoCache);
   ASSERT(!DblLnkLst_IsLinked(&blockedFiles));

   os_rwlock_destroy(&blockedFilesLock);
   os_kmem_cache_destroy(blockInfoCache);
}


/*
 *----------------------------------------------------------------------------
 *
 * BlockAddFileBlock --
 *
 *    Adds a block for the provided filename.  filename should be the name of
 *    the actual file being blocked, not the name within our namespace.  The
 *    provided blocker ID should uniquely identify this blocker.
 *
 *    All calls to BlockWaitOnFile() with the same filename will not return
 *    until BlockRemoveFileBlock() is called.
 *
 *    Note that this function assumes a block on filename does not already
 *    exist.
 *
 * Results:
 *    Zero on success, error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
BlockAddFileBlock(const char *filename,           // IN: name of file to block
                  const os_blocker_id_t blocker)  // IN: blocker adding the block
{
   BlockInfo *block;

   ASSERT(filename);

   /* Create a new block. */
   block = AllocBlock(blockInfoCache, filename, blocker);
   if (!block) {
      Warning("BlockAddFileBlock: out of memory\n");
      return OS_ENOMEM;
   }
   os_write_lock(&blockedFilesLock);

   /*
    * Prevent duplicate blocks of any filename.  Done under same lock as list
    * addition to ensure check for and adding of file are atomic.
    */
   if (BlockExists(filename)) {
      Warning("BlockAddFileBlock: block already exists for [%s]\n", filename);
      os_write_unlock(&blockedFilesLock);
      FreeBlock(blockInfoCache, block);
      return OS_EEXIST;
   }

   DblLnkLst_LinkLast(&blockedFiles, &block->links);

   os_write_unlock(&blockedFilesLock);

   LOG(4, "added block for [%s]\n", filename);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * BlockRemoveFileBlock --
 *
 *    Removes the provided file block and wakes up any threads waiting within
 *    BlockWaitOnFile().  Note that only the blocker that added a block can
 *    remove it.
 *
 * Results:
 *    Zero on success, error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
BlockRemoveFileBlock(const char *filename,          // IN: block to remove
                     const os_blocker_id_t blocker) // IN: blocker removing this block
{
   BlockInfo *block;

   ASSERT(filename);

   os_write_lock(&blockedFilesLock);

   block = GetBlock(filename, blocker);
   if (!block) {
      os_write_unlock(&blockedFilesLock);
      return OS_ENOENT;
   }

   DblLnkLst_Unlink1(&block->links);
   os_write_unlock(&blockedFilesLock);

   /* Undo GetBlock's refcount increment first. */
   os_atomic_dec(&block->refcount);

   /*
    * Now remove /our/ reference.  (As opposed to references by waiting
    * threads.)
    */
   if (os_atomic_dec_and_test(&block->refcount)) {
      /* No threads are waiting, so clean up ourself. */
      LOG(4, "Freeing block with no waiters on [%s]\n", filename);
      FreeBlock(blockInfoCache, block);
   } else {
      /* Wake up waiters; the last one will free the BlockInfo */
      LOG(4, "Completing block on [%s]\n", filename);
      os_complete_all(&block->completion);
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * BlockRemoveAllBlocks --
 *
 *    Removes all blocks added by the provided blocker.
 *
 * Results:
 *    Returns the number of entries removed from the blocklist.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

unsigned int
BlockRemoveAllBlocks(const os_blocker_id_t blocker)  // IN: blocker to remove blocks for
{
   struct DblLnkLst_Links *curr;
   struct DblLnkLst_Links *tmp;
   unsigned int removed = 0;

   os_write_lock(&blockedFilesLock);

   DblLnkLst_ForEachSafe(curr, tmp, &blockedFiles) {
      BlockInfo *currBlock = DblLnkLst_Container(curr, BlockInfo, links);
      if (currBlock->blocker == blocker || blocker == OS_UNKNOWN_BLOCKER) {

         DblLnkLst_Unlink1(&currBlock->links);

         /*
          * We count only entries removed from the -list-, regardless of whether
          * or not other waiters exist.
          */
         ++removed;

         /*
          * BlockInfos, as the result of placing a block on a file or directory,
          * reference themselves.  When the block is lifted, we need to remove
          * this self-reference and handle the result appropriately.
          */
         if (os_atomic_dec_and_test(&currBlock->refcount)) {
            /* Free blocks without any waiters ... */
            LOG(4, "Freeing block with no waiters for blocker [%p] (%s)\n",
                blocker, currBlock->filename);
            FreeBlock(blockInfoCache, currBlock);
         } else {
            /* ... or wakeup the waiting threads */
            LOG(4, "Completing block for blocker [%p] (%s)\n",
                blocker, currBlock->filename);
            os_complete_all(&currBlock->completion);
         }
      }
   }

   os_write_unlock(&blockedFilesLock);

   return removed;
}


/*
 *----------------------------------------------------------------------------
 *
 * BlockWaitOnFile --
 *
 *    Searches for a block on the provided filename.  If one exists, this
 *    function does not return until that block has been lifted; otherwise, it
 *    returns right away.
 *
 * Results:
 *    Zero on success, otherwise an appropriate system error if our sleep/
 *    block is interrupted.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
BlockWaitOnFile(const char *filename,   // IN: file to block on
                BlockHandle cookie)     // IN: previously found block
{
   BlockInfo *block = NULL;
   int error = 0;

   ASSERT(filename);

   /*
    * Caller may have used BlockLookup to conditionally search for a
    * block before actually going to sleep.  (This allows the caller to
    * do a little housekeeping, such as releasing vnode locks, before
    * blocking here.)
    */
   if (cookie == NULL) {
      os_read_lock(&blockedFilesLock);
      block = GetBlock(filename, OS_UNKNOWN_BLOCKER);
      os_read_unlock(&blockedFilesLock);

      if (!block) {
         /* This file is not blocked, just return */
         return 0;
      }
   } else {
      /*
       * Note that the "cookie's" reference count was incremented when it
       * was fetched via BlockLookup, so this is completely safe.  (We'll
       * decrement it below.)
       */
      block = cookie;
   }

   LOG(4, "(%"OS_FMTTID") Waiting for completion on [%s]\n", os_threadid, filename);
   error = os_wait_for_completion(&block->completion);
   LOG(4, "(%"OS_FMTTID") Wokeup from block on [%s]\n", os_threadid, filename);

   /*
    * The assumptions here are as follows:
    *   1.  The BlockInfo holds a reference to itself.  (BlockInfo's refcount
    *       is initialized to 1.)
    *   2.  BlockInfo's self reference is deleted only when BlockInfo is
    *       /also/ removed removed from the block list.
    *
    * Therefore, if the reference count hits zero, it's because the block is
    * no longer in the list, and there is no chance of another thread finding
    * and referencing this block between our dec_and_test and freeing it.
    */
   if (os_atomic_dec_and_test(&block->refcount)) {
      /* We were the last thread, so clean up */
      LOG(4, "(%"OS_FMTTID") I am the last to wakeup, freeing the block on [%s]\n",
          os_threadid, filename);
      FreeBlock(blockInfoCache, block);
   }

   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * BlockLookup --
 *
 *      VFS-exported function for searching for blocks.
 *
 * Results:
 *      Opaque pointer to a blockInfo if a block is found, NULL otherwise.
 *
 * Side effects:
 *      Located blockInfo, if any, has an incremented reference count.
 *
 *-----------------------------------------------------------------------------
 */

BlockHandle
BlockLookup(const char *filename,               // IN: pathname to test for
                                                //     blocking
            const os_blocker_id_t blocker)      // IN: specific blocker to
                                                //     search for
{
   BlockInfo *block;

   os_read_lock(&blockedFilesLock);

   block = GetBlock(filename, blocker);

   os_read_unlock(&blockedFilesLock);

   return block;
}


#ifdef VMX86_DEVEL
/*
 *----------------------------------------------------------------------------
 *
 * BlockListFileBlocks --
 *
 *    Lists all the current file blocks.
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
BlockListFileBlocks(void)
{
   DblLnkLst_Links *curr;
   int count = 0;

   os_read_lock(&blockedFilesLock);

   DblLnkLst_ForEach(curr, &blockedFiles) {
      BlockInfo *currBlock = DblLnkLst_Container(curr, BlockInfo, links);
      LOG(1, "BlockListFileBlocks: (%d) Filename: [%s], Blocker: [%p]\n",
          count++, currBlock->filename, currBlock->blocker);
   }

   os_read_unlock(&blockedFilesLock);

   if (!count) {
      LOG(1, "BlockListFileBlocks: No blocks currently exist.\n");
   }
}
#endif


/* Utility functions */

/*
 *----------------------------------------------------------------------------
 *
 * BlockExists --
 *
 *    Checks if a block already exists for the provided filename.
 *
 *    Note that this assumes the proper locking has been done on the data
 *    structure holding the blocked files (including ensuring the atomic_dec()
 *    without a kmem_cache_free() is safe).
 *
 * Results:
 *    TRUE if a block exists, FALSE otherwise.
 *
 * Side effects:
 *    If a block exists, its refcount is incremented and decremented.
 *
 *----------------------------------------------------------------------------
 */

static Bool
BlockExists(const char *filename)
{
   BlockInfo *block = GetBlock(filename, OS_UNKNOWN_BLOCKER);

   if (block) {
      os_atomic_dec(&block->refcount);
      return TRUE;
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * GetBlock --
 *
 *    Searches for a block on the provided filename by the provided blocker.
 *    If blocker is NULL, it is ignored and any matching filename is returned.
 *    If a block is found, the refcount is incremented.
 *
 *    Note that this assumes the proper locking has been done on the data
 *    structure holding the blocked files.
 *
 * Results:
 *    A pointer to the corresponding BlockInfo if found, NULL otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static BlockInfo *
GetBlock(const char *filename,          // IN: file to find block for
         const os_blocker_id_t blocker) // IN: blocker associated with this block
{
   struct DblLnkLst_Links *curr;

   /*
    * On FreeBSD we have a mechanism to assert (but not simply check)
    * that a lock is held. Since semantic is different (panic that
    * happens if assertion fails can not be suppressed) we are using
    * different name.
    */
#ifdef os_assert_rwlock_held
   os_assert_rwlock_held(&blockedFilesLock);
#else
   ASSERT(os_rwlock_held(&blockedFilesLock));
#endif

   DblLnkLst_ForEach(curr, &blockedFiles) {
      BlockInfo *currBlock = DblLnkLst_Container(curr, BlockInfo, links);
      if ((blocker == OS_UNKNOWN_BLOCKER || currBlock->blocker == blocker) &&
          strcmp(currBlock->filename, filename) == 0) {
         os_atomic_inc(&currBlock->refcount);
         return currBlock;
      }
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AllocBlock --
 *
 *    Allocates and initializes a new block structure.
 *
 * Results:
 *    Pointer to the struct on success, NULL on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

BlockInfo *
AllocBlock(os_kmem_cache_t *cache,        // IN: cache to allocate from
           const char *filename,          // IN: filname of block
           const os_blocker_id_t blocker) // IN: blocker id
{
   BlockInfo *block;
   size_t ret;

   /* Initialize this file's block structure. */
   block = os_kmem_cache_alloc(blockInfoCache);
   if (!block) {
      return NULL;
   }

   ret = strlcpy(block->filename, filename, sizeof block->filename);
   if (ret >= sizeof block->filename) {
      Warning("BlockAddFileBlock: filename is too large\n");
      os_kmem_cache_free(blockInfoCache, block);
      return NULL;
   }

   DblLnkLst_Init(&block->links);
   os_atomic_set(&block->refcount, 1);
   os_completion_init(&block->completion);
   block->blocker = blocker;

   return block;
}


/*
 *----------------------------------------------------------------------------
 *
 * FreeBlock --
 *
 *    Frees the provided block structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
FreeBlock(os_kmem_cache_t *cache,       // IN: cache block was allocated from
          BlockInfo *block)             // IN: block to free
{
   ASSERT(cache);
   ASSERT(block);

   os_completion_destroy(&block->completion);
   os_kmem_cache_free(cache, block);
}
