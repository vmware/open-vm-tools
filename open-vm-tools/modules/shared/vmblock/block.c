/*********************************************************
 * Copyright (C) 2006,2017-2018 VMware, Inc. All rights reserved.
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

/*********************************************************
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *********************************************************/

/*********************************************************
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
 * block.c --
 *
 *      Blocking operation implementions for the vmblock driver.
 */

/* os.h includes necessary OS-specific headers. */
#include "os.h"

#if defined(vmblock_fuse)
#elif defined(__linux__)
# include "vmblockInt.h"
#elif defined(sun)
# include "module.h"
#elif defined(__FreeBSD__)
# include "vmblock_k.h"
#endif
#include "block.h"
#include "dbllnklst.h"

typedef struct BlockInfo {
   DblLnkLst_Links links;
   os_atomic_t refcount;
   os_blocker_id_t blocker;
   os_completion_t completion;
   os_completion_t notification;
   char filename[OS_PATH_MAX];
} BlockInfo;


/* XXX: Is it worth turning this into a hash table? */
static DblLnkLst_Links blockedFiles;
static os_rwlock_t blockedFilesLock;
static os_kmem_cache_t *blockInfoCache;


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

static BlockInfo *
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
   os_completion_init(&block->notification);
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

   if (DblLnkLst_IsLinked(&block->links)) {
      Warning("Block on file [%s] is still in the list, "
              "not freeing, leaking memory\n", block->filename);
      return;
   }

   os_completion_destroy(&block->completion);
   os_completion_destroy(&block->notification);
   os_kmem_cache_free(cache, block);
}


/*
 *----------------------------------------------------------------------------
 *
 * BlockGrabReference --
 *
 *    Increments reference count in the provided block structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static BlockInfo *
BlockGrabReference(BlockInfo *block)
{
   ASSERT(block);

   os_atomic_inc(&block->refcount);

   return block;
}


/*
 *----------------------------------------------------------------------------
 *
 * BlockDropReference --
 *
 *    Increments reference count in the provided block structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    When reference count reaches 0 disposes of the block structure by
 *    calling FreeBlock().
 *
 *----------------------------------------------------------------------------
 */

static void
BlockDropReference(BlockInfo *block)
{
   if (os_atomic_dec_and_test(&block->refcount)) {
      LOG(4, "Dropped last reference for block on [%s]\n", block->filename);
      FreeBlock(blockInfoCache, block);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * GetBlock --
 *
 *    Searches for a block on the provided filename by the provided blocker.
 *    If blocker is NULL, it is ignored and any matching filename is returned.
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
         return currBlock;
      }
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * BlockDoRemoveBlock --
 *
 *    Removes given block from the block list and notifies waiters that block
 *    is gone.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Block structure will be freed if there were no waiters.
 *
 *----------------------------------------------------------------------------
 */

static void
BlockDoRemoveBlock(BlockInfo *block)
{
   ASSERT(block);

   DblLnkLst_Unlink1(&block->links);

   /* Wake up waiters, if any */
   LOG(4, "Completing block on [%s] (%d waiters)\n",
       block->filename, os_atomic_read(&block->refcount) - 1);
   os_complete_all(&block->completion);
   os_complete_all(&block->notification);

   /* Now drop our reference */
   BlockDropReference(block);
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
   int retval;

   ASSERT(filename);

   os_write_lock(&blockedFilesLock);

   if (GetBlock(filename, OS_UNKNOWN_BLOCKER)) {
      retval = OS_EEXIST;
      goto out;
   }

   block = AllocBlock(blockInfoCache, filename, blocker);
   if (!block) {
      Warning("BlockAddFileBlock: out of memory\n");
      retval = OS_ENOMEM;
      goto out;
   }

   DblLnkLst_LinkLast(&blockedFiles, &block->links);
   LOG(4, "added block for [%s]\n", filename);
   retval = 0;

out:
   os_write_unlock(&blockedFilesLock);
   return retval;
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
   int retval;

   ASSERT(filename);

   os_write_lock(&blockedFilesLock);

   block = GetBlock(filename, blocker);
   if (!block) {
      retval = OS_ENOENT;
      goto out;
   }

   BlockDoRemoveBlock(block);
   retval = 0;

out:
   os_write_unlock(&blockedFilesLock);
   return retval;
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

         BlockDoRemoveBlock(currBlock);

         /*
          * We count only entries removed from the -list-, regardless of whether
          * or not other waiters exist.
          */
         ++removed;
      }
   }

   os_write_unlock(&blockedFilesLock);

   return removed;
}


/*
 *----------------------------------------------------------------------------
 *
 * BlockWaitFileBlock --
 *
 *    The caller will be blocked until any other thread accesses the file
 *    specified by the filename or the block on the file is removed.
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
BlockWaitFileBlock(const char *filename,          // IN: block to wait
                   const os_blocker_id_t blocker) // IN: blocker
{
   BlockInfo *block;
   int retval = 0;

   ASSERT(filename);

   os_write_lock(&blockedFilesLock);
   block = GetBlock(filename, blocker);
   os_write_unlock(&blockedFilesLock);

   if (!block) {
      retval = OS_ENOENT;
      return retval;
   }

   os_wait_for_completion(&block->notification);

   return retval;
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
      if (block) {
         BlockGrabReference(block);
      }
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

   // Call the callback
   os_complete_all(&block->notification);

   LOG(4, "(%"OS_FMTTID") Waiting for completion on [%s]\n", os_threadid, filename);
   error = os_wait_for_completion(&block->completion);
   LOG(4, "(%"OS_FMTTID") Wokeup from block on [%s]\n", os_threadid, filename);

   BlockDropReference(block);

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
   if (block) {
      BlockGrabReference(block);
   }

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

