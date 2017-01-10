/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
#include "vm_basic_defs.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
#define HGFS_SM_MB_BEFORE                   smp_mb__before_atomic
#define HGFS_SM_MB_AFTER                    smp_mb__after_atomic
#else
/*
 * Fedora 21 backported some of the atomic primitives so
 * we test if they are defined and use them otherwise fallback
 * to the older variants.
 */
#ifdef smp_mb__before_atomic
#define HGFS_SM_MB_BEFORE                   smp_mb__before_atomic
#else
#define HGFS_SM_MB_BEFORE                   smp_mb__before_clear_bit
#endif
#ifdef smp_mb__after_atomic
#define HGFS_SM_MB_AFTER                    smp_mb__after_atomic
#else
#define HGFS_SM_MB_AFTER                    smp_mb__after_clear_bit
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
#define HGFS_PAGE_FILE_INDEX(page)          page_file_index(page)
#else
#define HGFS_PAGE_FILE_INDEX(page)          ((page)->index)
#endif

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
static int HgfsDoWriteBegin(struct file *file,
                            struct page *page,
                            unsigned pageFrom,
                            unsigned pageTo,
                            Bool canRetry,
                            Bool *doRetry);
static int HgfsDoWriteEnd(struct file *file,
                          struct page *page,
                          unsigned pageFrom,
                          unsigned pageTo,
                          loff_t writeTo,
                          unsigned copied);
static void HgfsDoExtendFile(struct inode *inode,
                             loff_t writeTo);

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

enum {
   PG_BUSY = 0,
};

typedef struct HgfsWbPage {
   struct list_head        wb_list;        /* Defines state of page: */
   struct page             *wb_page;       /* page to read in/write out */
   pgoff_t                 wb_index;       /* Offset >> PAGE_CACHE_SHIFT */
   struct kref             wb_kref;        /* reference count */
   unsigned long           wb_flags;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 13)
   wait_queue_head_t       wb_queue;
#endif
} HgfsWbPage;

static void HgfsInodePageWbAdd(struct inode *inode,
                                struct page *page);
static void HgfsInodePageWbRemove(struct inode *inode,
                                  struct page *page);
static Bool HgfsInodePageWbFind(struct inode *inode,
                                struct page *page);
static void HgfsWbRequestDestroy(HgfsWbPage *req);
static Bool HgfsCheckReadModifyWrite(struct file *file,
                                     struct page *page,
                                     unsigned int pageFrom,
                                     unsigned int pageTo);


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

         result = actualSize;
         if (actualSize == 0) {
            /* We got no bytes. */
            LOG(6, (KERN_WARNING "VMware hgfs: HgfsDoRead: server returned "
                   "zero\n"));
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

      LOG(4, (KERN_WARNING "VMware hgfs: HgfsDoWrite: res %u\n", result));
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
   loff_t curOffset = ((loff_t)HGFS_PAGE_FILE_INDEX(page) << PAGE_CACHE_SHIFT) + pageFrom;
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
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: zeroing last %Zu bytes\n",
              __func__, remainingCount));
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
   compat_unlock_page(page);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoWritepageInt --
 *
 *    Writes out a single page, using the specified handle and page offsets.
 *    At the time of writing, HGFS_IO_MAX == PAGE_CACHE_SIZE, so we could
 *    avoid the do {} while() and just write the page as is, but in case the
 *    above assumption is ever broken, it's nice that this will continue to
 *    "just work".
 *
 * Results:
 *    Number of bytes copied on success, negative error on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsDoWritepageInt(HgfsHandle handle,  // IN: Handle to use for writing
                   struct page *page,  // IN: Page containing data to write
                   unsigned pageFrom,  // IN: Beginning page offset
                   unsigned pageTo)    // IN: Ending page offset
{
   int result = 0;
   loff_t curOffset = ((loff_t)HGFS_PAGE_FILE_INDEX(page) << PAGE_CACHE_SHIFT) + pageFrom;
   size_t nextCount;
   size_t remainingCount = pageTo - pageFrom;
   struct inode *inode;
   HgfsDataPacket dataPacket[1];

   ASSERT(page->mapping);
   ASSERT(page->mapping->host);
   inode = page->mapping->host;

   LOG(4, (KERN_WARNING "VMware hgfs: %s: start writes at %Lu\n",
           __func__, curOffset));
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
         LOG(4, (KERN_WARNING "VMware hgfs: %s: write error %d\n",
                 __func__, result));
         goto exit;
      }
      remainingCount -= result;
      curOffset += result;
      pageFrom += result;

      /* Update the inode's size now rather than waiting for a revalidate. */
      HgfsDoExtendFile(inode, curOffset);
   } while ((result > 0) && (remainingCount > 0));

exit:
   if (result >= 0) {
      result = pageTo - pageFrom - remainingCount;
   }
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

   LOG(4, (KERN_WARNING "VMware hgfs: %s: start writes at %u to %u\n",
           __func__, pageFrom, pageTo));

   result = HgfsDoWritepageInt(handle, page, pageFrom, pageTo);
   if (result < 0) {
      goto exit;
   }

   HgfsInodePageWbRemove(page->mapping->host, page);

   result = 0;
   SetPageUptodate(page);

exit:
   LOG(4, (KERN_WARNING "VMware hgfs: %s: return %d\n", __func__, result));
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
   LOG(6, (KERN_WARNING "VMware hgfs: %s: reading from handle %u\n",
           __func__, handle));

   page_cache_get(page);
   result = HgfsDoReadpage(handle, page, 0, PAGE_CACHE_SIZE);
   page_cache_release(page);
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
   pgoff_t pageIndex;
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
   pageIndex = HGFS_PAGE_FILE_INDEX(page);
   LOG(4, (KERN_WARNING "VMware hgfs: %s: file size lpi %lu pi %lu\n",
           __func__, lastPageIndex, pageIndex));
   if (pageIndex > lastPageIndex) {
      goto exit;
   } else if (pageIndex == lastPageIndex) {
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
 *    Zero on success, always.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsDoWriteBegin(struct file *file,         // IN: File to be written
                 struct page *page,         // IN: Page to be written
                 unsigned pageFrom,         // IN: Starting page offset
                 unsigned pageTo,           // IN: Ending page offset
                 Bool canRetry,             // IN: can we retry write
                 Bool *doRetry)             // OUT: set to retry if necessary
{
   ASSERT(page);

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: off %Lu: %u to %u\n", __func__,
           (loff_t)HGFS_PAGE_FILE_INDEX(page) << PAGE_CACHE_SHIFT, pageFrom, pageTo));

   if (canRetry && HgfsCheckReadModifyWrite(file, page, pageFrom, pageTo)) {
      HgfsHandle readHandle;
      int result;
      result = HgfsGetHandle(page->mapping->host,
                             HGFS_OPEN_MODE_READ_ONLY + 1,
                             &readHandle);
      if (result == 0) {
         /*
          * We have a partial page write and thus require non-written part if the page
          * is to contain valid data.
          * A read of the page of the valid file data will set the page up to date.
          * If it fails the page will not be set up to date and the write end will write
          * the data out immediately (synchronously effectively).
          */
         result = HgfsDoReadpage(readHandle, page, 0, PAGE_CACHE_SIZE);
         *doRetry = TRUE;
      }
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: HgfsReadpage result %d\n", __func__, result));
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: returns 0\n", __func__));
   return 0;
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
 *      On success zero, always.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsPrepareWrite(struct file *file,  // IN: File to be written
                 struct page *page,  // IN: Page to prepare
                 unsigned pageFrom,  // IN: Beginning page offset
                 unsigned pageTo)    // IN: Ending page offset
{
   Bool dummyCanRetry = FALSE;
   return HgfsDoWriteBegin(file, page, pageFrom, pageTo, FALSE, &dummyCanRetry);
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
   unsigned pageFrom = pos & (PAGE_CACHE_SIZE - 1);
   unsigned pageTo = pageFrom + len;
   struct page *page;
   int result;
   Bool canRetry = TRUE;
   Bool doRetry;

   LOG(6, (KERN_WARNING "VMware hgfs: %s: (%s/%s(%ld), %u@%lld)\n",
           __func__, file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name,
           mapping->host->i_ino, len, (long long) pos));

   do {
      doRetry = FALSE;
      page = compat_grab_cache_page_write_begin(mapping, index, flags);
      if (page == NULL) {
         result = -ENOMEM;
         goto exit;
      }

      LOG(6, (KERN_DEBUG "VMware hgfs: %s: file size %Lu @ %Lu page %u to %u\n", __func__,
            (loff_t)compat_i_size_read(page->mapping->host),
            (loff_t)HGFS_PAGE_FILE_INDEX(page) << PAGE_CACHE_SHIFT,
            pageFrom, pageTo));

      result = HgfsDoWriteBegin(file, page, pageFrom, pageTo, canRetry, &doRetry);
      ASSERT(result == 0);
      canRetry = FALSE;
      if (doRetry) {
         page_cache_release(page);
      }
   } while (doRetry);

exit:
   *pagePtr = page;
   LOG(6, (KERN_DEBUG "VMware hgfs: %s: return %d\n", __func__, result));
   return result;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoExtendFile --
 *
 *      Helper function for extending a file size.
 *
 *      This function updates the inode->i_size, under the inode lock.
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
HgfsDoExtendFile(struct inode *inode, // IN: File we're writing to
                 loff_t writeTo)      // IN: Offset we're written to
{
   loff_t currentFileSize;

   spin_lock(&inode->i_lock);
   currentFileSize = compat_i_size_read(inode);

   if (writeTo > currentFileSize) {
      compat_i_size_write(inode, writeTo);
   }
   spin_unlock(&inode->i_lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsZeroUserSegments --
 *
 *      Wrapper function for setting a page's segments.
 *
 *      This function updates the inode->i_size, under the inode lock.
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
HgfsZeroUserSegments(struct page *page,      // IN: Page we're writing to
                     unsigned int start1,    // IN: segment 1 start
                     unsigned int end1,      // IN: segment 1 end
                     unsigned int start2,    // IN: segment 2 start
                     unsigned int end2)      // IN: segment 2 end
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
   zero_user_segments(page, start1, end1, start2, end2);
#else
   void *kaddr = compat_kmap_atomic(page);
   if (end1 > start1) {
      memset(kaddr + start1, 0, end1 - start1);
   }
   if (end2 > start2) {
      memset(kaddr + start2, 0, end2 - start2);
   }
   compat_kunmap_atomic(kaddr);
   flush_dcache_page(page);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsZeroUserSegments --
 *
 *      Wrapper function for zeroing a page's segments.
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
HgfsZeroUserSegment(struct page *page,      // IN: Page we're writing to
                    unsigned int start,     // IN: segment 1 start
                    unsigned int end)       // IN: segment 1 end
{
   HgfsZeroUserSegments(page, start, end, 0, 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetPageLength --
 *
 *      Helper function for finding the extent of valid file data in a page.
 *
 * Results:
 *      The page valid data length.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static unsigned int
HgfsGetPageLength(struct page *page) // IN: Page we're writing to
{
   loff_t currentFileSize;
   unsigned int pageLength = 0;

   currentFileSize = compat_i_size_read(page->mapping->host);
   if (currentFileSize > 0) {
      pgoff_t pageIndex = HGFS_PAGE_FILE_INDEX(page);
      pgoff_t fileSizeIndex = (currentFileSize - 1) >> PAGE_CACHE_SHIFT;

      if (pageIndex < fileSizeIndex) {
         pageLength = PAGE_CACHE_SIZE;
      } else if (pageIndex == fileSizeIndex) {
         pageLength = ((currentFileSize - 1) & ~PAGE_CACHE_MASK) + 1;
      }
   }

   return pageLength;
}


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
   struct inode *inode;

   ASSERT(file);
   ASSERT(page);
   inode = page->mapping->host;

   LOG(6, (KERN_WARNING "VMware hgfs: %s: (%s/%s(%ld), from %u to %u@%lld => %u)\n",
           __func__, file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name,
           page->mapping->host->i_ino, pageFrom, pageTo, (long long) writeTo, copied));

   /*
    * Zero any uninitialised parts of the page, and then mark the page
    * as up to date if it turns out that we're extending the file.
    */
   if (!PageUptodate(page)) {
      unsigned int pageLength = HgfsGetPageLength(page);

      if (pageLength == 0) {
         /* No file valid data in this page. Zero unwritten segments only. */
         HgfsZeroUserSegments(page, 0, pageFrom, pageTo, PAGE_CACHE_SIZE);
         SetPageUptodate(page);
      } else if (pageTo >= pageLength) {
         /* Some file valid data in this page. Zero unwritten segments only. */
         HgfsZeroUserSegment(page, pageTo, PAGE_CACHE_SIZE);
         if (pageTo == 0) {
            /* Overwritten all file valid data in this page. So the page is uptodate. */
            SetPageUptodate(page);
         }
      } else {
         /* Overwriting part of the valid file data. */
         HgfsZeroUserSegment(page, pageLength, PAGE_CACHE_SIZE);
      }
   }


   if (!PageUptodate(page)) {
      HgfsHandle handle = FILE_GET_FI_P(file)->handle;
      int result;

      /* Do a synchronous write since we have a partial page write of data. */
      result = HgfsDoWritepageInt(handle, page, pageFrom, pageTo);
      if (result == 0) {
         LOG(6, (KERN_WARNING "VMware hgfs: %s: sync write return %d\n", __func__, result));
      }
   } else {
      /* Page to write contains all valid data. */
      set_page_dirty(page);
      /*
       * Track the pages being written.
       */
      HgfsInodePageWbAdd(inode, page);
   }

   HgfsDoExtendFile(inode, writeTo);

   LOG(6, (KERN_WARNING "VMware hgfs: %s: return 0\n", __func__));
   return 0;
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

   offset = (loff_t)HGFS_PAGE_FILE_INDEX(page) << PAGE_CACHE_SHIFT;
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
   unsigned pageTo = pageFrom + len;
   loff_t writeTo = pos + copied;
   int ret;

   ASSERT(file);
   ASSERT(mapping);
   ASSERT(page);


   LOG(6, (KERN_WARNING "VMware hgfs: %s: (%s/%s(%ld), %u@%lld,=>%u)\n",
           __func__, file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name,
           mapping->host->i_ino, len, (long long) pos, copied));

   if (copied < len) {
      HgfsZeroUserSegment(page, pageFrom + copied, pageFrom + len);
   }

   ret = HgfsDoWriteEnd(file, page, pageFrom, pageTo, writeTo, copied);
   if (ret == 0) {
      ret = copied;
   }

   compat_unlock_page(page);
   page_cache_release(page);
   LOG(6, (KERN_WARNING "VMware hgfs: %s: return %d\n", __func__, ret));
   return ret;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbPageAlloc --
 *
 *    Allocates a write-back page object.
 *
 * Results:
 *    The write-back page object
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static inline HgfsWbPage *
HgfsWbPageAlloc(void)
{
   return kmalloc(sizeof (HgfsWbPage), GFP_KERNEL);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbPageAlloc --
 *
 *    Frees a write-back page object.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */


static inline void
HgfsWbPageFree(HgfsWbPage *page)  // IN: request of page data to write
{
   ASSERT(page);
   kfree(page);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestFree --
 *
 *    Frees the resources for a write-back page request.
 *    Calls the request destroy and then frees the object memory.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsWbRequestFree(struct kref *kref)  // IN: ref field request of page data to write
{
   HgfsWbPage *req = container_of(kref, HgfsWbPage, wb_kref);

   /* Release write back request page and free it. */
   HgfsWbRequestDestroy(req);
   HgfsWbPageFree(req);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestGet --
 *
 *    Reference the write-back page request.
 *    Calls the request destroy and then frees the object memory.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
HgfsWbRequestGet(HgfsWbPage *req)   // IN: request of page data to write
{
   kref_get(&req->wb_kref);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestPut --
 *
 *    Remove a reference the write-back page request.
 *    Calls the request free to tear down the object memory if it was the
 *    final one.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Destroys the request if last one.
 *
 *----------------------------------------------------------------------
 */

void
HgfsWbRequestPut(HgfsWbPage *req)  // IN: request of page data to write
{
   kref_put(&req->wb_kref, HgfsWbRequestFree);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestWaitUninterruptible --
 *
 *    Sleep function while waiting for requests to complete.
 *
 * Results:
 *    Always zero.
 *
 * Side effects:
*    None
 *
 *----------------------------------------------------------------------
 */

#if !defined VMW_WAITONBIT_317 && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13) && \
    LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
static int
HgfsWbRequestWaitUninterruptible(void *word) // IN:unused
{
   io_schedule();
   return 0;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestWait --
 *
 *    Wait for a write-back page request to complete.
 *    Interruptible by fatal signals only.
 *    The user is responsible for holding a count on the request.
 *
 * Results:
 *    Returned value will be zero if the bit was cleared,
 *    non-zero if the process received a signal and the mode
 *    permitted wakeup on that signal.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */


int
HgfsWbRequestWait(HgfsWbPage *req)  // IN: request of page data to write
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
   return wait_on_bit_io(&req->wb_flags,
                         PG_BUSY,
                         TASK_UNINTERRUPTIBLE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
   return wait_on_bit(&req->wb_flags,
                      PG_BUSY,
#if !defined VMW_WAITONBIT_317 && LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
                      HgfsWbRequestWaitUninterruptible,
#endif
                      TASK_UNINTERRUPTIBLE);
#else
   wait_event(req->wb_queue,
              !test_bit(PG_BUSY, &req->wb_flags));
   return 0;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestLock --
 *
 *    Lock the write-back page request.
 *
 * Results:
 *    Non-zero if the lock was not already locked
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static inline int
HgfsWbRequestLock(HgfsWbPage *req)  // IN: request of page data to write
{
   return !test_and_set_bit(PG_BUSY, &req->wb_flags);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestUnlock --
 *
 *    Unlock the write-back page request.
 *    Wakes up any waiting threads on the lock.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsWbRequestUnlock(HgfsWbPage *req)  // IN: request of page data to write
{
   if (!test_bit(PG_BUSY,&req->wb_flags)) {
      LOG(6, (KERN_WARNING "VMware Hgfs: %s: Invalid unlock attempted\n", __func__));
      return;
   }
   HGFS_SM_MB_BEFORE();
   clear_bit(PG_BUSY, &req->wb_flags);
   HGFS_SM_MB_AFTER();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
   wake_up_bit(&req->wb_flags, PG_BUSY);
#else
   wake_up(&req->wb_queue);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestUnlockAndPut --
 *
 *    Unlock the write-back page request and removes a reference.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsWbRequestUnlockAndPut(HgfsWbPage *req)  // IN: request of page data to write
{
   HgfsWbRequestUnlock(req);
   HgfsWbRequestPut(req);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestListAdd --
 *
 *    Add the write-back page request into the list.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static inline void
HgfsWbRequestListAdd(HgfsWbPage *req,         // IN: request of page data to write
                     struct list_head *head)  // IN: list of requests
{
   list_add_tail(&req->wb_list, head);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestListRemove --
 *
 *    Remove the write-back page request from the list.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static inline void
HgfsWbRequestListRemove(HgfsWbPage *req)  // IN: request of page data to write
{
   if (!list_empty(&req->wb_list)) {
      list_del_init(&req->wb_list);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestCreate --
 *
 *    Create the write-back page request.
 *
 * Results:
 *    The new write-back page request.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

HgfsWbPage *
HgfsWbRequestCreate(struct page *page)   // IN: page of data to write
{
   HgfsWbPage *wbReq;
   /* try to allocate the request struct */
   wbReq = HgfsWbPageAlloc();
   if (wbReq == NULL) {
      wbReq = ERR_PTR(-ENOMEM);
      goto exit;
   }

   /*
    * Initialize the request struct. Initially, we assume a
    * long write-back delay. This will be adjusted in
    * update_nfs_request below if the region is not locked.
    */
   wbReq->wb_flags   = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 13)
   init_waitqueue_head(&wbReq->wb_queue);
#endif
   INIT_LIST_HEAD(&wbReq->wb_list);
   wbReq->wb_page    = page;
   wbReq->wb_index   = HGFS_PAGE_FILE_INDEX(page);
   page_cache_get(page);
   kref_init(&wbReq->wb_kref);

exit:
   LOG(6, (KERN_WARNING "VMware hgfs: %s: (%p, %p)\n",
          __func__, wbReq, page));
   return wbReq;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWbRequestDestroy --
 *
 *    Destroys by freeing up all resources allocated to the request.
 *    Release page associated with a write-back request after it has completed.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsWbRequestDestroy(HgfsWbPage *req) // IN: write page request
{
   struct page *page = req->wb_page;

   LOG(6, (KERN_WARNING"VMware hgfs: %s: (%p, %p)\n",
          __func__, req, req->wb_page));

   if (page != NULL) {
      page_cache_release(page);
      req->wb_page = NULL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInodeFindWbRequest --
 *
 *    Finds if there is a write-back page request on this inode and returns it.
 *
 * Results:
 *    NULL or the write-back request for the page.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static HgfsWbPage *
HgfsInodeFindWbRequest(struct inode *inode, // IN: inode of file to write to
                       struct page *page)   // IN: page of data to write
{
   HgfsInodeInfo *iinfo;
   HgfsWbPage *req = NULL;
   HgfsWbPage *cur;

   iinfo = INODE_GET_II_P(inode);

   /* Linearly search the write back list for the correct req */
   list_for_each_entry(cur, &iinfo->listWbPages, wb_list) {
      if (cur->wb_page == page) {
         req = cur;
         break;
      }
   }

   if (req != NULL) {
      HgfsWbRequestGet(req);
   }

   return req;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInodeFindExistingWbRequest --
 *
 *    Finds if there is a write-back page request on this inode and returns
 *    locked.
 *    If the request is busy (locked) then it drops the lock and waits for it
 *    be not locked and searches the list again.
 *
 * Results:
 *    NULL or the write-back request for the page.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static HgfsWbPage *
HgfsInodeFindExistingWbRequest(struct inode *inode, // IN: inode of file to write to
                               struct page *page)   // IN: page of data to write
{
   HgfsWbPage *req;
   int error;

   spin_lock(&inode->i_lock);

   for (;;) {
      req = HgfsInodeFindWbRequest(inode, page);
      if (req == NULL) {
         goto out_exit;
      }

      /*
       * Try and lock the request if not already locked.
       * If we find it is already locked, busy, then we drop
       * the reference and wait to try again. Otherwise,
       * once newly locked we break out and return to the caller.
       */
      if (HgfsWbRequestLock(req)) {
         break;
      }

      /* The request was in use, so wait and then retry */
      spin_unlock(&inode->i_lock);
      error = HgfsWbRequestWait(req);
      HgfsWbRequestPut(req);
      if (error != 0) {
         goto out_nolock;
      }

      spin_lock(&inode->i_lock);
   }

out_exit:
   spin_unlock(&inode->i_lock);
   return req;

out_nolock:
   return ERR_PTR(error);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInodeAddWbRequest --
 *
 *    Add a write-back page request to an inode.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsInodeAddWbRequest(struct inode *inode, // IN: inode of file to write to
                      HgfsWbPage *req)     // IN: page write request
{
   HgfsInodeInfo *iinfo = INODE_GET_II_P(inode);

   LOG(6, (KERN_WARNING "VMware hgfs: %s: (%p, %p, %lu)\n",
          __func__, inode, req->wb_page, iinfo->numWbPages));

   /* Lock the request! */
   HgfsWbRequestLock(req);

   HgfsWbRequestListAdd(req, &iinfo->listWbPages);
   iinfo->numWbPages++;
   HgfsWbRequestGet(req);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInodeRemoveWbRequest --
 *
 *    Remove a write-back page request from an inode.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsInodeRemoveWbRequest(struct inode *inode, // IN: inode of file written to
                         HgfsWbPage *req)     // IN: page write request
{
   HgfsInodeInfo *iinfo = INODE_GET_II_P(inode);

   LOG(6, (KERN_CRIT "VMware hgfs: %s: (%p, %p, %lu)\n",
          __func__, inode, req->wb_page, iinfo->numWbPages));

   iinfo->numWbPages--;
   HgfsWbRequestListRemove(req);
   HgfsWbRequestPut(req);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInodePageWbAdd --
 *
 *    Add a write-back page request to an inode.
 *    If the page is already exists in the list for this inode nothing is
 *    done, otherwise a new object is created for the page and added to the
 *    inode list.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsInodePageWbAdd(struct inode *inode, // IN: inode of file to write to
                   struct page *page)   // IN: page of data to write
{
   HgfsWbPage *req;

   LOG(6, (KERN_WARNING "VMware hgfs: %s: (%p, %p)\n",
           __func__, inode, page));

   req = HgfsInodeFindExistingWbRequest(inode, page);
   if (req != NULL) {
      goto exit;
   }

   /*
    * We didn't find an existing write back request for that page so
    * we create one.
    */
   req = HgfsWbRequestCreate(page);
   if (IS_ERR(req)) {
      goto exit;
   }

   spin_lock(&inode->i_lock);
   /*
    * Add the new write request for the page into our inode list to track.
    */
   HgfsInodeAddWbRequest(inode, req);
   spin_unlock(&inode->i_lock);

exit:
   if (!IS_ERR(req)) {
      HgfsWbRequestUnlockAndPut(req);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInodePageWbRemove --
 *
 *    Remove a write-back page request from an inode.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsInodePageWbRemove(struct inode *inode, // IN: inode of file written to
                      struct page *page)   // IN: page of data written
{
   HgfsWbPage *req;

   LOG(6, (KERN_WARNING "VMware hgfs: %s: (%p, %p)\n",
           __func__, inode, page));

   req = HgfsInodeFindExistingWbRequest(inode, page);
   if (req == NULL) {
      goto exit;
   }
   spin_lock(&inode->i_lock);
   /*
    * Add the new write request for the page into our inode list to track.
    */
   HgfsInodeRemoveWbRequest(inode, req);
   HgfsWbRequestUnlockAndPut(req);
   spin_unlock(&inode->i_lock);

exit:
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInodePageWbFind --
 *
 *    Find a write-back page request from an inode.
 *
 * Results:
 *    TRUE if found an existing write for the page, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsInodePageWbFind(struct inode *inode, // IN: inode of file written to
                    struct page *page)   // IN: page of data written
{
   HgfsWbPage *req;
   Bool found = TRUE;

   LOG(6, (KERN_WARNING "VMware hgfs: %s: (%p, %p)\n",
           __func__, inode, page));

   req = HgfsInodeFindExistingWbRequest(inode, page);
   if (req == NULL) {
      found = FALSE;
      goto exit;
   }
   spin_lock(&inode->i_lock);
   /*
    * Remove the write request lock and reference we just grabbed.
    */
   HgfsWbRequestUnlockAndPut(req);
   spin_unlock(&inode->i_lock);

exit:
   LOG(6, (KERN_WARNING "VMware hgfs: %s: (%p, %p) return %d\n",
           __func__, inode, page, found));
   return found;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsCheckReadModifyWrite --
 *
 *    Check if we can read the page from the server to get the valid data
 *    for a page that we are in process of partially modifying and then
 *    writing.
 *
 *    We maybe required to read the page first if the file is open for
 *    reading in addition to writing, the page is not marked as uptodate,
 *    it is not dirty or waiting to be committed, indicating that it was
 *    previously allocated and then modified, that there were valid bytes
 *    of data in that range of the file, and that the new data won't completely
 *    replace the old data in that range of the file.
 *
 * Results:
 *    TRUE if we need to read valid data and can do so for the page,
 *    FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsCheckReadModifyWrite(struct file *file,     // IN: File to be written
                         struct page *page,     // IN: page of data written
                         unsigned int pageFrom, // IN: position
                         unsigned int pageTo)   // IN: len
{
   unsigned int pageLength = HgfsGetPageLength(page);
   struct inode *inode = page->mapping->host;
   Bool readPage = FALSE;

  LOG(6, (KERN_WARNING "VMware hgfs: %s: (%p, %u, %u)\n",
           __func__, page, pageFrom, pageTo));

   if ((file->f_mode & FMODE_READ) &&             // opened for read?
       !HgfsInodePageWbFind(inode, page) &&       // I/O request already ?
       !PageUptodate(page) &&                     // Up to date?
       pageLength > 0 &&                          // valid bytes of file?
       (pageTo < pageLength || pageFrom != 0)) {  // replace all valid bytes?
      readPage = TRUE;
   }

  LOG(6, (KERN_WARNING "VMware hgfs: %s: (%p, %u, %u) return %d\n",
           __func__, page, pageFrom, pageTo, readPage));
   return readPage;
}
