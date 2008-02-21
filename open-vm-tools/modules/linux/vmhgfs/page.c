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
 * page.c --
 *
 * Address space operations for the filesystem portion of the vmhgfs driver.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/pagemap.h>

#include "compat_mm.h"
#include "compat_page-flags.h"
#include "compat_fs.h"
#include "compat_kernel.h"
#ifdef HGFS_ENABLE_WRITEBACK
#include <linux/writeback.h>
#endif

#include "cpName.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "fsutil.h"
#include "inode.h"
#include "vm_assert.h"
#include "vm_basic_types.h"

/*
 * Max amount of read/write data per server request. Must be smaller than 
 * HGFS_PACKET_MAX by a large enough margin to allow for headers and 
 * other request fields.
 */
#define HGFS_IO_MAX 4096               

/* Private functions. */
static int HgfsDoWrite(HgfsHandle handle,
                       const char *buf,
                       size_t count,
                       loff_t offset);
static int HgfsDoRead(HgfsHandle handle,
                      char *buf,
                      size_t count,
                      loff_t offset);
static int HgfsDoReadpage(HgfsHandle handle,
                          struct page *page,
                          unsigned pageFrom,
                          unsigned pageTo);
static int HgfsDoWritepage(HgfsHandle handle,
                           struct page *page,
                           unsigned pageFrom,
                           unsigned pageTo);

/* HGFS address space operations. */
static int HgfsReadpage(struct file *file, 
                        struct page *page);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 52)
static int HgfsWritepage(struct page *page, 
                         struct writeback_control *wbc);
#else
static int HgfsWritepage(struct page *page);
#endif
static int HgfsPrepareWrite(struct file *file, 
                            struct page *page,
                            unsigned pageFrom, 
                            unsigned pageTo);
static int HgfsCommitWrite(struct file *file, 
                           struct page *page,
                           unsigned pageFrom, 
                           unsigned pageTo);

/* HGFS address space operations structure. */
struct address_space_operations HgfsAddressSpaceOperations = {
   .readpage      = HgfsReadpage,
   .writepage     = HgfsWritepage,
   .prepare_write = HgfsPrepareWrite,
   .commit_write  = HgfsCommitWrite,
#ifdef HGFS_ENABLE_WRITEBACK
   .set_page_dirty = __set_page_dirty_nobuffers,
#endif
};


/* 
 * Private functions. 
 */

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoRead --
 *
 *    Do one read request. Called by HgfsReadpage, possibly multiple times
 *    if the size of the read is too big to be handled by one server request.
 *
 *    We send a "Read" request to the server with the given handle.
 *
 *    It is assumed that this function is never called with a larger read than 
 *    what can be sent in one request.
 *
 * Results:
 *    Returns the number of bytes read on success, or an error on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDoRead(HgfsHandle handle,  // IN:  Handle for this file
           char *buf,          // OUT: Buffer to copy data into
           size_t count,       // IN:  Number of bytes to read
           loff_t offset)      // IN:  Offset at which to read
{
   HgfsReq *req;
   HgfsRequestRead *request;
   HgfsReplyRead *reply;
   int result = 0;

   ASSERT(buf);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoRead: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

   /* Fill out the request fields. */
   request = (HgfsRequestRead *)(HGFS_REQ_PAYLOAD(req));
   request->header.id = req->id;
   request->header.op = HGFS_OP_READ;
   request->file = handle;
   request->offset = offset;
   request->requiredSize = count;
   req->payloadSize = sizeof *request;

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      reply = (HgfsReplyRead *)(HGFS_REQ_PAYLOAD(req));
      result = HgfsStatusConvertToLinux(reply->header.status);
      
      if (result != 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoRead: read failed\n"));
         goto out;
      }
      
      /* Sanity check on read size. */
      if (reply->actualSize > count) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoRead: read too big!\n"));
         result = -EPROTO;
         goto out;
      }
      
      if (!reply->actualSize) {
         /* We got no bytes, so don't need to copy to user. */
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDoRead: server returned "
                 "zero\n"));
         result = reply->actualSize;
         goto out;
      }
      
      /* Return result. */
      memcpy(buf, reply->payload, reply->actualSize);
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDoRead: copied %u\n",
              reply->actualSize));
      result = reply->actualSize;         
      goto out;
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoRead: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoRead: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoRead: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoWrite --
 *
 *    Do one write request. Called by HgfsDoWritepage, possibly multiple 
 *    times if the size of the write is too big to be handled by one server 
 *    request.
 *
 *    We send a "Write" request to the server with the given handle.
 *
 *    It is assumed that this function is never called with a larger write 
 *    than what can be sent in one request.
 *
 * Results:
 *    Returns the number of bytes written on success, or an error on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsDoWrite(HgfsHandle handle,       // IN: Handle for this file
            const char *buf,         // IN: Buffer containing data
            size_t count,            // IN: Number of bytes to write
            loff_t offset)           // IN: Offset to begin writing at
{
   HgfsReq *req;
   HgfsRequestWrite *request;
   HgfsReplyWrite *reply;
   int result = 0;

   ASSERT(buf);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoWrite: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

   /* Fill out the request fields. */
   request = (HgfsRequestWrite *)(HGFS_REQ_PAYLOAD(req));
   request->header.id = req->id;
   request->header.op = HGFS_OP_WRITE;
   request->file = handle;
   request->flags = 0;
   request->offset = offset;
   request->requiredSize = count;

   memcpy(request->payload, buf, request->requiredSize);
   req->payloadSize = sizeof *request + request->requiredSize - 1;

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      reply = (HgfsReplyWrite *)(HGFS_REQ_PAYLOAD(req));
      result = HgfsStatusConvertToLinux(reply->header.status);

      if (result != 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoWrite: write failed\n"));
         goto out;
      }
      
      /* Return result. */
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDoWrite: wrote %u bytes\n",
              reply->actualSize));
      result = reply->actualSize;
      goto out;
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoWrite: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoWrite: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoWrite: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoReadpage --
 *
 *    Reads in a single page, using the specified handle and page offsets.
 *    At the time of writing, HGFS_IO_MAX == PAGE_CACHE_SIZE, so we could
 *    avoid the do {} while() and just read the page as is, but in case the
 *    above assumption is ever broken, it's nice that this will continue to
 *    "just work".
 *
 * Results:
 *    Zero on success, non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int 
HgfsDoReadpage(HgfsHandle handle,  // IN:     Handle to use for reading
               struct page *page,  // IN/OUT: Page to read into
               unsigned pageFrom,  // IN:     Where to start reading to
               unsigned pageTo)    // IN:     Where to stop reading
{
   int result = 0;
   char *buffer = kmap(page) + pageFrom;
   loff_t curOffset = ((loff_t)page->index << PAGE_CACHE_SHIFT) + pageFrom;
   size_t nextCount, remainingCount = pageTo - pageFrom;

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDoReadpage: read %Zu bytes from fh %u "
           "at offset %Lu\n", remainingCount, handle, curOffset));

   /*
    * Call HgfsDoRead repeatedly until either
    * - HgfsDoRead returns an error, or
    * - HgfsDoRead returns 0 (end of file), or
    * - We have read the requested number of bytes.
    */
   do {
      nextCount = (remainingCount > HGFS_IO_MAX) ?
         HGFS_IO_MAX : remainingCount;
      result = HgfsDoRead(handle, buffer, nextCount, curOffset);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoReadpage: read error %d\n",
                 result));
         goto out;
      }
      remainingCount -= result;
      curOffset += result;
      buffer += result;
   } while ((result > 0) && (remainingCount > 0));

   /* 
    * It's possible that despite being asked to read a full page, there is less
    * than a page in the file from this offset, so we should zero the rest of 
    * the page's memory.
    */
   memset(buffer, 0, remainingCount);

   /* 
    * We read a full page (or all of the page that actually belongs to the 
    * file), so mark it up to date. Also, flush the old page data from the data
    * cache.
    */
   flush_dcache_page(page);
   SetPageUptodate(page);
   result = 0;

  out:
   kunmap(page);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoWritepage --
 *
 *    Writes out a single page, using the specified handle and page offsets.
 *    At the time of writing, HGFS_IO_MAX == PAGE_CACHE_SIZE, so we could
 *    avoid the do {} while() and just write the page as is, but in case the
 *    above assumption is ever broken, it's nice that this will continue to
 *    "just work".
 *
 *    A quick note about appending to files. Before HGFS used the page cache,
 *    an HgfsWrite examined a file's f_flags and added HGFS_WRITE_APPEND to
 *    the write packet if the file was opened with O_APPEND. This causes the
 *    server to reopen the fd with O_APPEND so that writes will append to the
 *    end.
 *
 *    In the page cache world, this won't work because we may have arrived at
 *    this function via writepage(), which doesn't give us a particular file
 *    and thus we don't know if we should be appending or not. In fact, the
 *    generic write path employed by the page cache handles files with O_APPEND
 *    set by moving the file offset to the result of i_size_read(). So we
 *    shouldn't ever need to set HGFS_WRITE_APPEND, as now we will handle all
 *    write appends, instead of telling the server to do it for us.
 *
 * Results:
 *    Zero on success, non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int 
HgfsDoWritepage(HgfsHandle handle,  // IN: Handle to use for writing
                struct page *page,  // IN: Page containing data to write
                unsigned pageFrom,  // IN: Beginning page offset
                unsigned pageTo)    // IN: Ending page offset
{
   int result = 0;
   char *buffer = kmap(page) + pageFrom;
   loff_t curOffset = ((loff_t)page->index << PAGE_CACHE_SHIFT) + pageFrom;
   size_t nextCount;
   size_t remainingCount = pageTo - pageFrom;
   struct inode *inode;

   ASSERT(page->mapping);
   ASSERT(page->mapping->host);
   inode = page->mapping->host;

   /*
    * Call HgfsDoWrite repeatedly until either
    * - HgfsDoWrite returns an error, or
    * - HgfsDoWrite returns 0 (XXX this probably rarely happens), or
    * - We have written the requested number of bytes.
    */
   do {
      nextCount = (remainingCount > HGFS_IO_MAX) ?
         HGFS_IO_MAX : remainingCount;
      result = HgfsDoWrite(handle, buffer, nextCount, curOffset);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDoWritepage: write error %d\n", 
                 result));
         goto out;
      }
      remainingCount -= result;
      curOffset += result;
      buffer += result;

      /* Update the inode's size now rather than waiting for a revalidate. */
      if (curOffset > compat_i_size_read(inode)) {
         compat_i_size_write(inode, curOffset);
      }
   } while ((result > 0) && (remainingCount > 0));

   result = 0;

  out:
   kunmap(page);
   return result;
}


/*
 * HGFS address space operations.
 */

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsReadpage --
 *
 *    Read a page from an open file. Like HgfsWritepage, there are some
 *    complicated locking rules governing this function. The page arrives from
 *    the VFS locked, and we must unlock it before exiting. In addition, we
 *    must acquire a reference to the page before mapping it, and we must
 *    flush the page's data from the data cache (not to be confused with
 *    dcache i.e. the dentry cache).
 *
 * Results:
 *    Zero on success, non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int 
HgfsReadpage(struct file *file, // IN:     File to read from 
             struct page *page) // IN/OUT: Page to write to
{
   int result = 0;
   HgfsHandle handle;

   ASSERT(file);
   ASSERT(file->f_dentry);
   ASSERT(file->f_dentry->d_inode);
   ASSERT(page);

   handle = FILE_GET_FI_P(file)->handle;
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsReadPage: reading from handle %u\n",
           handle));   

   page_cache_get(page);
   result = HgfsDoReadpage(handle, page, 0, PAGE_CACHE_SIZE);
   page_cache_release(page);
   compat_unlock_page(page);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsWritepage --
 *
 *    The "spontaneous" way to write a page, called when the kernel is under
 *    memory pressure or is asked to sync a memory mapped file. Because 
 *    writepage() can be called from so many different places, we don't get a 
 *    filp with which to write, and we have to be very careful about races and 
 *    locking.
 *
 * Results:
 *    Zero on success, non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 52)
static int 
HgfsWritepage(struct page *page,             // IN: Page to write from
              struct writeback_control *wbc) // IN: Ignored
#else
static int 
HgfsWritepage(struct page *page)             // IN: Page to write from
#endif
{
   struct inode *inode;
   HgfsHandle handle;
   int result;
   pgoff_t lastPageIndex;
   loff_t currentFileSize;
   unsigned to = PAGE_CACHE_SIZE;

   ASSERT(page);
   ASSERT(page->mapping);
   ASSERT(page->mapping->host);
   inode = page->mapping->host;

   /* We need a writable file handle. */
   result = HgfsGetHandle(inode, 
                          HGFS_OPEN_MODE_WRITE_ONLY + 1, 
                          &handle);
   if (result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsWritepage: could not get writable "
              "file handle\n"));
      goto exit;
   }

   /* 
    * We were given an entire page to write. In most cases this means "start
    * writing from the beginning of the page (byte 0) to the very end (byte
    * PAGE_CACHE_SIZE). But what if this is the last page of the file? Then
    * we don't want to write a full PAGE_CACHE_SIZE bytes, but just however
    * many bytes may remain in the page.
    *
    * XXX: Other filesystems check the page index to make sure that the page
    * we're being asked to write is within the size of the file. I guess
    * that's because writepage() can race with truncate(), and if we find
    * ourselves here after a truncate(), we can drop the write.
    */
   currentFileSize = compat_i_size_read(inode);
   lastPageIndex = currentFileSize >> PAGE_CACHE_SHIFT;
   if (page->index > lastPageIndex) {
      goto exit;
   } else if (page->index == lastPageIndex) {
      to = currentFileSize & (PAGE_CACHE_SIZE - 1);
      if (to == 0) {
         goto exit;
      }
   }

   /* 
    * This part is fairly intricate, so it deserves some explanation. We're
    * really interested in calling HgfsDoWritepage with our page and
    * handle, without having to then worry about locks or references. See
    * Documentation/filesystems/Locking in the kernel to see what rules we
    * must obey.
    *
    * Firstly, we acquire a reference to the page via page_cache_get() and call
    * compat_set_page_writeback(). The latter does a number of things: it sets 
    * the writeback bit on the page, and if it wasn't already set, it sets the 
    * writeback bit in the radix tree. Then, if the page isn't dirty, it clears
    * the dirty bit in the radix tree. The end result is that the radix tree's 
    * notion of dirty and writeback is fully synced with the page itself.
    *
    * Secondly, we write the page itself. 
    *
    * Thirdly, we end writeback of the page via compat_end_page_writeback(), 
    * and release our reference on the page.
    *
    * Finally, we unlock the page, waking up its waiters and making it 
    * available to anyone else. Note that this step must be performed 
    * regardless of whether we wrote anything, as the VFS locked the page for
    * us.
    */
   page_cache_get(page);
   compat_set_page_writeback(page);
   result = HgfsDoWritepage(handle, page, 0, to);
   compat_end_page_writeback(page);
   page_cache_release(page);

  exit:
   compat_unlock_page(page);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPrepareWrite --
 *
 *      Called by the generic write path to set up a write request for a page.
 *      We're expected to do any pre-allocation and housekeeping prior to
 *      receiving the write. 
 *
 * Results:
 *      Always zero.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int 
HgfsPrepareWrite(struct file *file,  // IN: Ignored
                 struct page *page,  // IN: Page to prepare
                 unsigned pageFrom,  // IN: Beginning page offset
                 unsigned pageTo)    // IN: Ending page offset
{
#ifdef HGFS_ENABLE_WRITEBACK
   loff_t offset = (loff_t)page->index << PAGE_CACHE_SHIFT;
   loff_t currentFileSize = compat_i_size_read(page->mapping->host);

   /*
    * If we are doing a partial write into a new page (beyond end of
    * file), then intialize it. This allows other writes to this page
    * to accumulate before we need to write it to the server.
    */
   if ((offset >= currentFileSize) ||
       ((pageFrom == 0) && (offset + pageTo) >= currentFileSize)) {
      void *kaddr = kmap_atomic(page, KM_USER0);
	
      if (pageFrom) {
         memset(kaddr, 0, pageFrom);
      }
      if (pageTo < PAGE_CACHE_SIZE) {
         memset(kaddr + pageTo, 0, PAGE_CACHE_SIZE - pageTo);
      }
      kunmap_atomic(kaddr, KM_USER0);
      flush_dcache_page(page);
   }
#endif

   /* 
    * Prior to 2.4.10, our caller expected to call page_address(page) between 
    * the calls to prepare_write() and commit_write(). This meant filesystems
    * had to kmap() the page in prepare_write() and kunmap() it in 
    * commit_write(). In 2.4.10, the call to page_address() was replaced with 
    * __copy_to_user(), and while its not clear to me why this is safer,
    * nfs_prepare_write() dropped the kmap()/kunmap() calls in the same patch,
    * so the two events must be related.
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 10)
   kmap(page);
#endif
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCommitWrite --
 *
 *    This function is the more common write path for HGFS, called from
 *    generic_file_buffered_write. It is much simpler for us than 
 *    HgfsWritepage above: the caller has obtained a reference to the page 
 *    and will unlock it when we're done. And we don't need to worry about 
 *    properly marking the writeback bit, either. See mm/filemap.c in the
 *    kernel for details about how we are called.
 *
 * Results:
 *    Zero on succes, non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int 
HgfsCommitWrite(struct file *file, // IN: File we're writing to
                struct page *page, // IN: Page we're writing from
                unsigned pageFrom, // IN: Beginning page offset
                unsigned pageTo)   // IN: Ending page offset
{
   HgfsHandle handle;
   struct inode *inode;
   loff_t currentFileSize;
   loff_t offset;
   loff_t writeTo;

   ASSERT(file);
   ASSERT(page);
   inode = page->mapping->host;
   currentFileSize = compat_i_size_read(inode);
   offset = (loff_t)page->index << PAGE_CACHE_SHIFT;
   writeTo = offset + pageTo;

   /* See coment in HgfsPrepareWrite. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 10)
   kunmap(page);
#endif

   if (writeTo > currentFileSize) {
      compat_i_size_write(inode, writeTo);
   }
   
   /* We wrote a complete page, so it is up to date. */
   if ((pageTo - pageFrom) == PAGE_CACHE_SIZE) {
      SetPageUptodate(page);
   }

#ifdef HGFS_ENABLE_WRITEBACK
   /* 
    * Check if this is a partial write to a new page, which was 
    * initialized in HgfsPrepareWrite.
    */
   if ((offset >= currentFileSize) ||
       ((pageFrom == 0) && (writeTo >= currentFileSize))) {
      SetPageUptodate(page);
   }

   /*
    * If the page is uptodate, then just mark it dirty and let
    * the page cache write it when it wants to.
    */
   if (PageUptodate(page)) {
      set_page_dirty(page);
      return 0;
   }
#endif
   /*
    * We've recieved a partial write to page that is not uptodate, so
    * do the write now while the page is still locked.  Another
    * alternative would be to read the page in HgfsPrepareWrite, which
    * would make it uptodate (ie a complete cached page).
    */
   handle = FILE_GET_FI_P(file)->handle;
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsCommitWrite: writing to handle %u\n", 
           handle));   
   return HgfsDoWritepage(handle, page, pageFrom, pageTo);
}


