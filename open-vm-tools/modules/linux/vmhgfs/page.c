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
#include "compat_pagemap.h"
#include "compat_highmem.h"
#include <linux/writeback.h>

#include "cpName.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "hgfsUtil.h"
#include "fsutil.h"
#include "inode.h"
#include "vm_assert.h"
#include "vm_basic_types.h"
#include "hgfsTransport.h"


/* Private functions. */
static int HgfsDoWrite(HgfsHandle handle,
                       HgfsDataPacket dataPacket[],
                       uint32 numEntries,
                       loff_t offset);
static int HgfsDoRead(HgfsHandle handle,
                      HgfsDataPacket dataPacket[],
                      uint32 numEntries,
                      loff_t offset);
static int HgfsDoReadpage(HgfsHandle handle,
                          struct page *page,
                          unsigned pageFrom,
                          unsigned pageTo);
static int HgfsDoWritepage(HgfsHandle handle,
                           struct page *page,
                           unsigned pageFrom,
                           unsigned pageTo);
static void HgfsDoWriteBegin(struct page *page,
                             unsigned pageFrom,
                             unsigned pageTo);
static int HgfsDoWriteEnd(struct file *file,
                          struct page *page,
                          unsigned pageFrom,
                          unsigned pageTo,
                          loff_t writeTo,
                          unsigned copied);

/* HGFS address space operations. */
static int HgfsReadpage(struct file *file,
                        struct page *page);
static int HgfsWritepage(struct page *page,
                         struct writeback_control *wbc);

/*
 * Write aop interface has changed in 2.6.28. Specifically,
 * the page locking semantics and requirement to handle
 * short writes. We already handle short writes, so no major
 * changes needed. write_begin is expected to return a locked
 * page and write_end is expected to unlock the page and drop
 * the reference before returning.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28)
static int HgfsWriteBegin(struct file *file,
                          struct address_space *mapping,
                          loff_t pos,
                          unsigned len,
                          unsigned flags,
                          struct page **page,
                          void **clientData);
static int HgfsWriteEnd(struct file *file,
                        struct address_space *mapping,
                        loff_t pos,
                        unsigned len,
                        unsigned copied,
                        struct page *page,
                        void *clientData);
#else
static int HgfsPrepareWrite(struct file *file,
                            struct page *page,
                            unsigned pageFrom,
                            unsigned pageTo);
static int HgfsCommitWrite(struct file *file,
                           struct page *page,
                           unsigned pageFrom,
                           unsigned pageTo);
#endif

/* HGFS address space operations structure. */
struct address_space_operations HgfsAddressSpaceOperations = {
   .readpage      = HgfsReadpage,
   .writepage     = HgfsWritepage,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28)
   .write_begin   = HgfsWriteBegin,
   .write_end     = HgfsWriteEnd,
#else
   .prepare_write = HgfsPrepareWrite,
   .commit_write  = HgfsCommitWrite,
#endif
   .set_page_dirty = __set_page_dirty_nobuffers,
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
 *    HgfsDataPacket is an array of pages into which data will be read.
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
HgfsDoRead(HgfsHandle handle,             // IN:  Handle for this file
           HgfsDataPacket dataPacket[],   // IN/OUT: Data description
           uint32 numEntries,             // IN: Number of entries in dataPacket
           loff_t offset)                 // IN:  Offset at which to read
{
   HgfsReq *req;
   HgfsOp opUsed;
   int result = 0;
   uint32 actualSize = 0;
   char *payload = NULL;
   HgfsStatus replyStatus;
   char *buf;
   uint32 count;
   ASSERT(numEntries == 1);

   count = dataPacket[0].len;

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoRead: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

 retry:
   opUsed = hgfsVersionRead;
   if (opUsed == HGFS_OP_READ_FAST_V4) {
      HgfsRequest *header;
      HgfsRequestReadV3 *request;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->id = req->id;
      header->op = opUsed;

      request = (HgfsRequestReadV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->file = handle;
      request->offset = offset;
      request->requiredSize = count;
      request->reserved = 0;
      req->dataPacket = kmalloc(numEntries * sizeof req->dataPacket[0],
                                GFP_KERNEL);
      if (!req->dataPacket) {
         LOG(4, (KERN_WARNING "%s: Failed to allocate mem\n", __func__));
         result = -ENOMEM;
         goto out;
      }
      memcpy(req->dataPacket, dataPacket, numEntries * sizeof req->dataPacket[0]);
      req->numEntries = numEntries;

      LOG(4, (KERN_WARNING "VMware hgfs: Fast Read V4\n"));
   } else if (opUsed == HGFS_OP_READ_V3) {
      HgfsRequest *header;
      HgfsRequestReadV3 *request;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->id = req->id;
      header->op = opUsed;

      request = (HgfsRequestReadV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->file = handle;
      request->offset = offset;
      request->requiredSize = MIN(req->bufferSize - sizeof *request -
                                  sizeof *header, count);
      request->reserved = 0;
      req->dataPacket = NULL;
      req->numEntries = 0;
      req->payloadSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   } else {
      HgfsRequestRead *request;

      request = (HgfsRequestRead *)(HGFS_REQ_PAYLOAD(req));
      request->header.id = req->id;
      request->header.op = opUsed;
      request->file = handle;
      request->offset = offset;
      request->requiredSize = MIN(req->bufferSize - sizeof *request, count);
      req->dataPacket = NULL;
      req->numEntries = 0;
      req->payloadSize = sizeof *request;
   }

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         if (opUsed == HGFS_OP_READ_FAST_V4) {
            actualSize = ((HgfsReplyReadV3 *)HGFS_REP_PAYLOAD_V3(req))->actualSize;
         } else if (opUsed == HGFS_OP_READ_V3) {
            actualSize = ((HgfsReplyReadV3 *)HGFS_REP_PAYLOAD_V3(req))->actualSize;
            payload = ((HgfsReplyReadV3 *)HGFS_REP_PAYLOAD_V3(req))->payload;
         } else {
            actualSize = ((HgfsReplyRead *)HGFS_REQ_PAYLOAD(req))->actualSize;
            payload = ((HgfsReplyRead *)HGFS_REQ_PAYLOAD(req))->payload;
         }

         /* Sanity check on read size. */
         if (actualSize > count) {
            LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoRead: read too big!\n"));
            result = -EPROTO;
            goto out;
         }

         if (!actualSize) {
            /* We got no bytes. */
            LOG(6, (KERN_WARNING "VMware hgfs: HgfsDoRead: server returned "
                   "zero\n"));
            result = actualSize;
            goto out;
         }

         /* Return result. */
         if (opUsed == HGFS_OP_READ_V3 || opUsed == HGFS_OP_READ) {
            buf = kmap(dataPacket[0].page) + dataPacket[0].offset;
            ASSERT(buf);
            memcpy(buf, payload, actualSize);
            LOG(6, (KERN_WARNING "VMware hgfs: HgfsDoRead: copied %u\n",
                    actualSize));
            kunmap(dataPacket[0].page);
         }
         result = actualSize;
	      break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         switch (opUsed) {
         case HGFS_OP_READ_FAST_V4:
            LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoRead: Fast Read V4 not "
                    "supported. Falling back to V3 Read.\n"));
            if (req->dataPacket) {
               kfree(req->dataPacket);
               req->dataPacket = NULL;
            }
            hgfsVersionRead = HGFS_OP_READ_V3;
            goto retry;

         case HGFS_OP_READ_V3:
            LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoRead: Version 3 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionRead = HGFS_OP_READ;
            goto retry;

         default:
            break;
         }
	      break;

      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoRead: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoRead: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoRead: unknown error: "
              "%d\n", result));
   }

out:
   if (req->dataPacket) {
      kfree(req->dataPacket);
   }
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
 *    HgfsDataPacket is an array of pages from which data will be written
 *    to file.
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
HgfsDoWrite(HgfsHandle handle,             // IN: Handle for this file
            HgfsDataPacket dataPacket[],   // IN: Data description
            uint32 numEntries,             // IN: Number of entries in dataPacket
            loff_t offset)                 // IN: Offset to begin writing at
{
   HgfsReq *req;
   int result = 0;
   HgfsOp opUsed;
   uint32 requiredSize = 0;
   uint32 actualSize = 0;
   char *payload = NULL;
   uint32 reqSize;
   HgfsStatus replyStatus;
   char *buf;
   uint32 count;
   ASSERT(numEntries == 1);

   count = dataPacket[0].len;

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoWrite: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

 retry:
   opUsed = hgfsVersionWrite;
   if (opUsed == HGFS_OP_WRITE_FAST_V4) {
      HgfsRequest *header;
      HgfsRequestWriteV3 *request;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->id = req->id;
      header->op = opUsed;

      request = (HgfsRequestWriteV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->file = handle;
      request->flags = 0;
      request->offset = offset;
      request->requiredSize = count;
      request->reserved = 0;
      payload = request->payload;
      requiredSize = request->requiredSize;

      req->dataPacket = kmalloc(numEntries * sizeof req->dataPacket[0],
                                GFP_KERNEL);
      if (!req->dataPacket) {
         LOG(4, (KERN_WARNING "%s: Failed to allocate mem\n", __func__));
         result = -ENOMEM;
         goto out;
      }
      memcpy(req->dataPacket, dataPacket, numEntries * sizeof req->dataPacket[0]);
      req->numEntries = numEntries;
      reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
      req->payloadSize = reqSize;
      LOG(4, (KERN_WARNING "VMware hgfs: Fast Write V4\n"));
   } else if (opUsed == HGFS_OP_WRITE_V3) {
      HgfsRequest *header;
      HgfsRequestWriteV3 *request;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->id = req->id;
      header->op = opUsed;

      request = (HgfsRequestWriteV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->file = handle;
      request->flags = 0;
      request->offset = offset;
      request->requiredSize = MIN(req->bufferSize - sizeof *header -
                                  sizeof *request, count);
      LOG(4, (KERN_WARNING "VMware hgfs: Using write V3\n"));
      request->reserved = 0;
      payload = request->payload;
      requiredSize = request->requiredSize;
      reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
      req->dataPacket = NULL;
      req->numEntries = 0;
      buf = kmap(dataPacket[0].page) + dataPacket[0].offset;
      memcpy(payload, buf, requiredSize);
      kunmap(dataPacket[0].page);

      req->payloadSize = reqSize + requiredSize - 1;
   } else {
      HgfsRequestWrite *request;

      request = (HgfsRequestWrite *)(HGFS_REQ_PAYLOAD(req));
      request->header.id = req->id;
      request->header.op = opUsed;
      request->file = handle;
      request->flags = 0;
      request->offset = offset;
      request->requiredSize = MIN(req->bufferSize - sizeof *request, count);
      payload = request->payload;
      requiredSize = request->requiredSize;
      reqSize = sizeof *request;
      req->dataPacket = NULL;
      req->numEntries = 0;
      buf = kmap(dataPacket[0].page) + dataPacket[0].offset;
      memcpy(payload, buf, requiredSize);
      kunmap(dataPacket[0].page);

      req->payloadSize = reqSize + requiredSize - 1;
   }

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         if (opUsed == HGFS_OP_WRITE_V3 || opUsed == HGFS_OP_WRITE_FAST_V4) {
            actualSize = ((HgfsReplyWriteV3 *)HGFS_REP_PAYLOAD_V3(req))->actualSize;
         } else {
            actualSize = ((HgfsReplyWrite *)HGFS_REQ_PAYLOAD(req))->actualSize;
         }

         /* Return result. */
         LOG(6, (KERN_WARNING "VMware hgfs: HgfsDoWrite: wrote %u bytes\n",
                 actualSize));
         result = actualSize;
         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         switch (opUsed) {
         case HGFS_OP_WRITE_FAST_V4:
            LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoWrite: Fast Write V4 not "
                    "supported. Falling back to V3 write.\n"));
            if (req->dataPacket) {
               kfree(req->dataPacket);
               req->dataPacket = NULL;
            }
            hgfsVersionWrite = HGFS_OP_WRITE_V3;
            goto retry;

         case HGFS_OP_WRITE_V3:
            LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoWrite: Version 3 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionWrite = HGFS_OP_WRITE;
            goto retry;

         default:
            break;
         }
         break;

      default:
         LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoWrite: server "
                 "returned error: %d\n", result));
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoWrite: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoWrite: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoWrite: unknown error: "
              "%d\n", result));
   }

out:
   if (req->dataPacket) {
      kfree(req->dataPacket);
   }
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
   loff_t curOffset = ((loff_t)page->index << PAGE_CACHE_SHIFT) + pageFrom;
   size_t nextCount, remainingCount = pageTo - pageFrom;
   HgfsDataPacket dataPacket[1];

   LOG(6, (KERN_WARNING "VMware hgfs: HgfsDoReadpage: read %Zu bytes from fh %u "
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
      dataPacket[0].page = page;
      dataPacket[0].offset = pageFrom;
      dataPacket[0].len = nextCount;
      result = HgfsDoRead(handle, dataPacket, 1, curOffset);
      if (result < 0) {
         LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoReadpage: read error %d\n",
                 result));
         goto out;
      }
      remainingCount -= result;
      curOffset += result;
      pageFrom += result;
   } while ((result > 0) && (remainingCount > 0));

   /*
    * It's possible that despite being asked to read a full page, there is less
    * than a page in the file from this offset, so we should zero the rest of
    * the page's memory.
    */
   if (remainingCount) {
      char *buffer = kmap(page) + pageTo;
      memset(buffer - remainingCount, 0, remainingCount);
      kunmap(page);
   }

   /*
    * We read a full page (or all of the page that actually belongs to the
    * file), so mark it up to date. Also, flush the old page data from the data
    * cache.
    */
   flush_dcache_page(page);
   SetPageUptodate(page);
   result = 0;

  out:
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
   loff_t curOffset = ((loff_t)page->index << PAGE_CACHE_SHIFT) + pageFrom;
   size_t nextCount;
   size_t remainingCount = pageTo - pageFrom;
   struct inode *inode;
   HgfsDataPacket dataPacket[1];

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
      dataPacket[0].page = page;
      dataPacket[0].offset = pageFrom;
      dataPacket[0].len = nextCount;
      result = HgfsDoWrite(handle, dataPacket, 1, curOffset);
      if (result < 0) {
         LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoWritepage: write error %d\n",
                 result));
         goto out;
      }
      remainingCount -= result;
      curOffset += result;
      pageFrom += result;

      /* Update the inode's size now rather than waiting for a revalidate. */
      if (curOffset > compat_i_size_read(inode)) {
         compat_i_size_write(inode, curOffset);
      }
   } while ((result > 0) && (remainingCount > 0));

   result = 0;

  out:
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
   LOG(6, (KERN_WARNING "VMware hgfs: HgfsReadPage: reading from handle %u\n",
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

static int
HgfsWritepage(struct page *page,             // IN: Page to write from
              struct writeback_control *wbc) // IN: Ignored
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
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsWritepage: could not get writable "
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
 * HgfsDoWriteBegin --
 *
 *      Helper function for HgfsWriteBegin / HgfsPrepareWrite.
 *
 *      Initialize the page if the file is to be appended.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsDoWriteBegin(struct page *page,         // IN: Page to be written
                 unsigned pageFrom,         // IN: Starting page offset
                 unsigned pageTo)           // IN: Ending page offset
{
   loff_t offset;
   loff_t currentFileSize;

   ASSERT(page);

   offset = (loff_t)page->index << PAGE_CACHE_SHIFT;
   currentFileSize = compat_i_size_read(page->mapping->host);

   /*
    * If we are doing a partial write into a new page (beyond end of
    * file), then intialize it. This allows other writes to this page
    * to accumulate before we need to write it to the server.
    */
   if ((offset >= currentFileSize) ||
       ((pageFrom == 0) && (offset + pageTo) >= currentFileSize)) {
      void *kaddr = compat_kmap_atomic(page);

      if (pageFrom) {
         memset(kaddr, 0, pageFrom);
      }
      if (pageTo < PAGE_CACHE_SIZE) {
         memset(kaddr + pageTo, 0, PAGE_CACHE_SIZE - pageTo);
      }
      compat_kunmap_atomic(kaddr);
      flush_dcache_page(page);
   }
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
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
   HgfsDoWriteBegin(page, pageFrom, pageTo);

   return 0;
}

#else


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsWriteBegin --
 *
 *      Called by the generic write path to set up a write request for a page.
 *      We're expected to do any pre-allocation and housekeeping prior to
 *      receiving the write.
 *
 *      This function is expected to return a locked page.
 *
 * Results:
 *      Zero on success, non-zero error otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsWriteBegin(struct file *file,             // IN: File to be written
               struct address_space *mapping, // IN: Mapping
               loff_t pos,                    // IN: File position
               unsigned len,                  // IN: Bytes to be written
               unsigned flags,                // IN: Write flags
               struct page **pagePtr,         // OUT: Locked page
               void **clientData)             // OUT: Opaque to pass to write_end, unused
{
   pgoff_t index = pos >> PAGE_CACHE_SHIFT;
   unsigned pageFrom = pos & (PAGE_CACHE_SHIFT - 1);
   unsigned pageTo = pos + len;
   struct page *page;

   page = compat_grab_cache_page_write_begin(mapping, index, flags);
   if (page == NULL) {
      return -ENOMEM;
   }
   *pagePtr = page;

   HgfsDoWriteBegin(page, pageFrom, pageTo);
   return 0;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoWriteEnd --
 *
 *      Helper function for HgfsWriteEnd.
 *
 *      This function updates the inode->i_size, conditionally marks the page
 *      updated and carries out the actual write in case of partial page writes.
 *
 * Results:
 *      Zero on succes, non-zero on error.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsDoWriteEnd(struct file *file, // IN: File we're writing to
               struct page *page, // IN: Page we're writing from
               unsigned pageFrom, // IN: Starting page offset
               unsigned pageTo,   // IN: Ending page offset
               loff_t writeTo,    // IN: File position to write to
               unsigned copied)   // IN: Number of bytes copied to the page
{
   HgfsHandle handle;
   struct inode *inode;
   loff_t currentFileSize;
   loff_t offset;

   ASSERT(file);
   ASSERT(page);
   inode = page->mapping->host;
   currentFileSize = compat_i_size_read(inode);
   offset = (loff_t)page->index << PAGE_CACHE_SHIFT;

   if (writeTo > currentFileSize) {
      compat_i_size_write(inode, writeTo);
   }

   /* We wrote a complete page, so it is up to date. */
   if (copied == PAGE_CACHE_SIZE) {
      SetPageUptodate(page);
   }

   /*
    * Check if this is a partial write to a new page, which was
    * initialized in HgfsDoWriteBegin.
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

   /*
    * We've recieved a partial write to page that is not uptodate, so
    * do the write now while the page is still locked.  Another
    * alternative would be to read the page in HgfsDoWriteBegin, which
    * would make it uptodate (ie a complete cached page).
    */
   handle = FILE_GET_FI_P(file)->handle;
   LOG(6, (KERN_WARNING "VMware hgfs: %s: writing to handle %u\n", __func__,
           handle));
   return HgfsDoWritepage(handle, page, pageFrom, pageTo);
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCommitWrite --
 *
 *      This function is the more common write path for HGFS, called from
 *      generic_file_buffered_write. It is much simpler for us than
 *      HgfsWritepage above: the caller has obtained a reference to the page
 *      and will unlock it when we're done. And we don't need to worry about
 *      properly marking the writeback bit, either. See mm/filemap.c in the
 *      kernel for details about how we are called.
 *
 * Results:
 *      Zero on succes, non-zero on error.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsCommitWrite(struct file *file,    // IN: File to write
                struct page *page,    // IN: Page to write from
                unsigned pageFrom,    // IN: Starting page offset
                unsigned pageTo)      // IN: Ending page offset
{
   loff_t offset;
   loff_t writeTo;
   unsigned copied;

   ASSERT(page);
   ASSERT(file);

   offset = (loff_t)page->index << PAGE_CACHE_SHIFT;
   writeTo = offset + pageTo;
   copied = pageTo - pageFrom;

   return HgfsDoWriteEnd(file, page, pageFrom, pageTo, writeTo, copied);
}

#else


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsWriteEnd --
 *
 *      This function is the more common write path for HGFS, called from
 *      generic_file_buffered_write. It is much simpler for us than
 *      HgfsWritepage above: write_begin has obtained a reference to the page
 *      and we will unlock it when we're done. And we don't need to worry about
 *      properly marking the writeback bit, either. See mm/filemap.c in the
 *      kernel for details about how we are called.
 *
 *      This function should unlock the page and reduce the refcount.
 *
 * Results:
 *      Number of bytes written or negative error
 *
 * Side effects:
 *      Unlocks the page and drops the reference.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsWriteEnd(struct file *file,              // IN: File to write
             struct address_space *mapping,  // IN: Mapping
             loff_t pos,                     // IN: File position
             unsigned len,                   // IN: len passed from write_begin
             unsigned copied,                // IN: Number of actually copied bytes
             struct page *page,              // IN: Page to write from
             void *clientData)               // IN: From write_begin, unused.
{
   unsigned pageFrom = pos & (PAGE_CACHE_SIZE - 1);
   unsigned pageTo = pageFrom + copied;
   loff_t writeTo = pos + copied;
   int ret;

   ASSERT(file);
   ASSERT(mapping);
   ASSERT(page);

   ret = HgfsDoWriteEnd(file, page, pageFrom, pageTo, writeTo, copied);
   if (ret == 0) {
      ret = copied;
   }

   compat_unlock_page(page);
   page_cache_release(page);
   return ret;
}
#endif
