/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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


/*
 * dndClipboard.c
 *
 *      Implements dndClipboard.h
 */

#include <stdlib.h>
#include <string.h>

#include "vm_assert.h"

#include "dndClipboard.h"
#include "dndInt.h"
#include "dndCPMsgV4.h"
#include "unicode.h"

#define CPFormatToIndex(x) ((unsigned int)(x) - 1)

/*
 *----------------------------------------------------------------------------
 *
 * CPClipItemInit --
 *
 *      CPClipboardItem constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
CPClipItemInit(CPClipItem *item)        // IN/OUT: the clipboard item
{
   ASSERT(item);

   item->buf = NULL;
   item->size = 0;
   item->exists = FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipItemDestroy --
 *
 *      CPCilpboardItem destructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
CPClipItemDestroy(CPClipItem *item)     // IN/OUT: the clipboard item
{
   ASSERT(item);

   free(item->buf);
   item->buf = NULL;
   item->size = 0;
   item->exists = FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipItemCopy --
 *
 *      Copy clipboard item from src to dest.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipItemCopy(CPClipItem *dest,        // IN: dest clipboard item
               const CPClipItem *src)   // IN: source clipboard item
{
   ASSERT(dest);
   ASSERT(src);

   if (src->buf) {
      void *tmp = dest->buf;
      dest->buf = realloc(dest->buf, src->size + 1);
      if (!dest->buf) {
         dest->buf = tmp;
         return FALSE;
      }
      ((uint8 *)dest->buf)[src->size] = 0;
      memcpy(dest->buf, src->buf, src->size);
   }

   dest->size = src->size;
   dest->exists = src->exists;

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_Init --
 *
 *      Constructor for CPClipboard.
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
CPClipboard_Init(CPClipboard *clip)     // IN/OUT: the clipboard
{
   unsigned int i;

   ASSERT(clip);

   clip->changed = TRUE;
   clip->maxSize = CPCLIPITEM_MAX_SIZE_V3;
   for (i = CPFORMAT_MIN; i < CPFORMAT_MAX; ++i) {
      CPClipItemInit(&clip->items[CPFormatToIndex(i)]);
   }
   clip->isInitialized = TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_InitWithSize --
 *
 *      Call CPClipboard_Init and set the clipboard size.
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
CPClipboard_InitWithSize(CPClipboard *clip,  // IN/OUT: the clipboard
                         uint32 size)        // IN: clipboard size
{
   ASSERT(clip);

   CPClipboard_Init(clip);
   clip->maxSize = size;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_Destroy --
 *
 *      Destroys a clipboard.
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
CPClipboard_Destroy(CPClipboard *clip)  // IN/OUT: the clipboard
{
   unsigned int i;

   ASSERT(clip);

   for (i = CPFORMAT_MIN; i < CPFORMAT_MAX; ++i) {
      CPClipItemDestroy(&clip->items[CPFormatToIndex(i)]);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_Clear --
 *
 *      Clears the items in CPClipboard.
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
CPClipboard_Clear(CPClipboard *clip)    // IN/OUT: the clipboard
{
   unsigned int i;

   ASSERT(clip);

   clip->changed = TRUE;
   for (i = CPFORMAT_MIN; i < CPFORMAT_MAX; ++i) {
      CPClipboard_ClearItem(clip, i);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 *  CPClipboard_SetItem --
 *
 *      Makes a copy of the item adds it to the clipboard. If something
 *      already exists for the format it is overwritten. To set a promised
 *      type, pass in NULL for buffer and 0 for the size.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_SetItem(CPClipboard *clip,          // IN/OUT: the clipboard
                    const DND_CPFORMAT fmt,     // IN: the item format
                    const void *clipitem,       // IN: the item
                    const size_t size)          // IN: the item size
{
   CPClipItem *item;
   uint8 *newBuf = NULL;
   /*
    * Microsoft Office Text Effects i.e. HTML Format, BIFF, GVML, image, rtf
    * and text may be put into a clipboard at same time, and total size may be
    * more than limit. The order in filterList determines the order in which
    * the format will be fiiltered, e.g. HTML format will be first dropped,
    * then GVML,..., at last TEXT
    * File content won't appear tegother with other formats, if exceeding size
    * limit, the format will be dropped
    */
   DND_CPFORMAT filterList[] = {CPFORMAT_FILECONTENTS,
                                CPFORMAT_ART_GVML_CLIPFORMAT,
                                CPFORMAT_BIFF12,
                                CPFORMAT_HTML_FORMAT,
                                CPFORMAT_IMG_PNG,
                                CPFORMAT_RTF,
                                CPFORMAT_TEXT};
   int filterIndex = 0;

   ASSERT(clip);

   if (!(CPFORMAT_UNKNOWN < fmt && fmt < CPFORMAT_MAX)) {
      return FALSE;
   }

   if (!CPClipboard_ClearItem(clip, fmt)) {
      return FALSE;
   }

   if (size >= (size_t) clip->maxSize) {
      return FALSE;
   }

   item = &clip->items[CPFormatToIndex(fmt)];

   if (clipitem) {
      /* It has to be valid utf8 for plain text format. */
      if (CPFORMAT_TEXT == fmt) {
         char *str = (char *)clipitem;
         if (!Unicode_IsBufferValid(str,
                                    size,
                                    STRING_ENCODING_UTF8)) {
            return FALSE;
         }
      }

      newBuf = malloc(size + 1);
      if (!newBuf) {
         return FALSE;
      }
      memcpy(newBuf, clipitem, size);
      newBuf[size] = 0;
   }

   item->buf = newBuf;
   item->size = size;
   item->exists = TRUE;

   /* Drop some data if total size is more than limit. */
   while (CPClipboard_GetTotalSize(clip) >= (size_t) clip->maxSize &&
          filterIndex < ARRAYSIZE(filterList)) {
      if (!CPClipboard_ClearItem(clip, filterList[filterIndex])) {
         return FALSE;
      }
      filterIndex++;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_ClearItem --
 *
 *      Clears the item in the clipboard.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_ClearItem(CPClipboard *clip,        // IN: the clipboard
                      DND_CPFORMAT fmt)         // IN: item to be cleared
{
   CPClipItem *item;

   ASSERT(clip);

   if (!(CPFORMAT_UNKNOWN < fmt && fmt < CPFORMAT_MAX)) {
      return FALSE;
   }

   item = &clip->items[CPFormatToIndex(fmt)];

   free(item->buf);
   item->buf = NULL;
   item->size = 0;
   item->exists = FALSE;

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_GetItem  --
 *
 *      Get the clipboard item of format fmt from the clipboard. The clipboard
 *      maintains ownership of the data. If the item is promised, the buffer
 *      will contain NULL and the size will be 0.
 *
 * Results:
 *      TRUE if item exists, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_GetItem(const CPClipboard *clip,    // IN: the clipboard
                    DND_CPFORMAT fmt,           // IN: the format
                    void **buf,                 // OUT
                    size_t *size)               // OUT
{
   ASSERT(clip);
   ASSERT(size);
   ASSERT(buf);

   if(!(CPFORMAT_UNKNOWN < fmt && fmt < CPFORMAT_MAX)) {
      return FALSE;
   }

   if (clip->items[CPFormatToIndex(fmt)].exists) {
      *buf = clip->items[CPFormatToIndex(fmt)].buf;
      *size = clip->items[CPFormatToIndex(fmt)].size;
      ASSERT(*buf);
      ASSERT((*size > 0) && (*size < clip->maxSize));
      return TRUE;
   } else {
      ASSERT(!clip->items[CPFormatToIndex(fmt)].size);
      return FALSE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_ItemExists  --
 *
 *      Check if the clipboard item of format fmt exists or not.
 *
 * Results:
 *      TRUE if item exists, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_ItemExists(const CPClipboard *clip,    // IN: the clipboard
                       DND_CPFORMAT fmt)           // IN: the format
{
   ASSERT(clip);

   if(!(CPFORMAT_UNKNOWN < fmt && fmt < CPFORMAT_MAX)) {
      return FALSE;
   }

   return (clip->items[CPFormatToIndex(fmt)].exists &&
           clip->items[CPFormatToIndex(fmt)].size > 0);
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_IsEmpty  --
 *
 *      Check if the clipboard item of format fmt exists or not.
 *
 * Results:
 *      TRUE if item exists, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_IsEmpty(const CPClipboard *clip)    // IN: the clipboard
{
   unsigned int i;

   ASSERT(clip);

   for (i = CPFORMAT_MIN; i < CPFORMAT_MAX; ++i) {
      if (clip->items[CPFormatToIndex(i)].exists &&
          clip->items[CPFormatToIndex(i)].size > 0) {
         return FALSE;
      }
   }
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_GetTotalSize --
 *
 *      Get total buffer size of the clipboard.
 *
 * Results:
 *      Total buffer size of the clipboard.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

size_t
CPClipboard_GetTotalSize(const CPClipboard *clip) // IN: the clipboard
{
   unsigned int i = 0;
   size_t totalSize = 0;

   ASSERT(clip);

   for (i = CPFORMAT_MIN; i < CPFORMAT_MAX; ++i) {
      if (clip->items[CPFormatToIndex(i)].exists &&
          clip->items[CPFormatToIndex(i)].size > 0) {
         totalSize += clip->items[CPFormatToIndex(i)].size;
      }
   }
   return totalSize;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_SetChanged  --
 *
 *      Set clip->changed.
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
CPClipboard_SetChanged(CPClipboard *clip, // IN/OUT: the clipboard
                       Bool changed)      // IN
{
   clip->changed = changed;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_Changed  --
 *
 *      Return clip->changed.
 *
 * Results:
 *      Return clip->changed.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_Changed(const CPClipboard *clip)    // IN: the clipboard
{
   return clip->changed;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_Copy --
 *
 *      Copies the clipboard contents from src to dest. Assumes that dest has
 *      been initialized and contains no data.
 *
 * Results:
 *      TRUE on success, FALSE on failure. On failure the caller should
 *      destroy the object to ensure memory is cleaned up.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_Copy(CPClipboard *dest,             // IN: the desination clipboard
                 const CPClipboard *src)        // IN: the source clipboard
{
   unsigned int i;

   ASSERT(dest);
   ASSERT(src);

   for (i = CPFORMAT_MIN; i < CPFORMAT_MAX; ++i) {
      if (!CPClipItemCopy(&dest->items[CPFormatToIndex(i)],
                         &src->items[CPFormatToIndex(i)])) {
         return FALSE;
      }
   }
   dest->changed = src->changed;
   dest->maxSize = src->maxSize;
   dest->isInitialized = TRUE;

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_Serialize --
 *
 *      Serialize the contents of the CPClipboard out to the provided dynbuf.
 *
 * Results:
 *      TRUE on success.
 *      FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_Serialize(const CPClipboard *clip, // IN
                      DynBuf *buf)             // OUT: the output buffer
{
   DND_CPFORMAT fmt;
   uint32 maxFmt = CPFORMAT_MAX;

   ASSERT(clip);
   ASSERT(buf);
   ASSERT(clip->isInitialized);

   /* Return FALSE if not initialized. */
   if (!clip->isInitialized) {
      return FALSE;
   }

   /* First append number of formats in clip. */
   if (!DynBuf_Append(buf, &maxFmt, sizeof maxFmt)) {
      return FALSE;
   }

   /* Append format data one by one. */
   for (fmt = CPFORMAT_MIN; fmt < CPFORMAT_MAX; ++fmt) {
      CPClipItem *item = (CPClipItem *)&(clip->items[CPFormatToIndex(fmt)]);
      if (!DynBuf_Append(buf, &item->exists, sizeof item->exists) ||
          !DynBuf_Append(buf, &item->size, sizeof item->size)) {
         return FALSE;
      }
      if (item->exists && (item->size > 0) &&
          !DynBuf_Append(buf, item->buf, item->size)) {
         return FALSE;
      }
   }

   if (!DynBuf_Append(buf, &clip->changed, sizeof clip->changed)) {
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_Unserialize --
 *
 *      Unserialize the arguments of the CPClipboard provided by the buffer.
 *      On failure the clip will be destroyed.
 *
 * Results:
 *      TRUE if success, FALSE otherwise
 *
 * Side effects:
 *      The clip passed in should be empty, otherwise will cause memory leakage.
 *      On success, arguments found in buf are unserialized into clip.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_Unserialize(CPClipboard *clip, // OUT: the clipboard
                        const void *buf,   // IN: input buffer
                        size_t len)        // IN: buffer length
{
   DND_CPFORMAT fmt;
   BufRead r;
   uint32 maxFmt;

   ASSERT(clip);
   ASSERT(buf);
   ASSERT(clip->isInitialized);

   /* Return FALSE if not initialized. */
   if (!clip->isInitialized) {
      goto error;
   }

   r.pos = buf;
   r.unreadLen = len;

   /* First get number of formats in the buf. */
   if (!DnDReadBuffer(&r, &maxFmt, sizeof maxFmt)) {
      goto error;
   }

   /* This version only supports number of formats up to CPFORMAT_MAX. */
   maxFmt = MIN(CPFORMAT_MAX, maxFmt);

   for (fmt = CPFORMAT_MIN; fmt < maxFmt; ++fmt) {
      Bool exists = FALSE;
      uint32 size = 0;

      if (!DnDReadBuffer(&r, &exists, sizeof exists) ||
          !DnDReadBuffer(&r, &size, sizeof size)) {
         Log("%s: Error: exists:%d, size:%d, format:%d.\n", __FUNCTION__,
             exists, size, (int)fmt);
         goto error;
      }

      if (exists && size) {
         if (size > r.unreadLen) {
            Log("%s: Error: size:%d, unreadLen:%d, format:%d.\n", __FUNCTION__,
                size, (int)r.unreadLen, (int)fmt);
            goto error;
         }

         if (!CPClipboard_SetItem(clip, fmt, r.pos, size)) {
            goto error;
         }
         if (!DnDSlideBuffer(&r, size)) {
            goto error;
         }
      }
   }

   /* It is possible that clip->changed is missing in some beta products. */
   if (r.unreadLen == sizeof clip->changed &&
       !DnDReadBuffer(&r, &clip->changed, sizeof clip->changed)) {
      goto error;
   }

   return TRUE;

error:
   CPClipboard_Destroy(clip);
   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPClipboard_Strip --
 *
 *      Remove clipboard items based on the passed in capabilities mask.
 *      Introduced in DnDV4.
 *
 *      XXX This function assumes that the bits in mask are such that if the
 *      check is being made for copy paste, that the corresponding bit for
 *      DnD is set to zero. Otherwise, the format cleared by copy paste will
 *      not be removed. Similar for the other case. A way to make this clearer
 *      would be to pass a flag to this function that tells it which bits
 *      to check, with no dependencies on the other bits being in proper
 *      state.
 *
 * Results:
 *      TRUE if clipboard is empty as a result, else FALSE.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPClipboard_Strip(CPClipboard *clip,    // IN/OUT: the clipboard
                  uint32 mask)          // IN: if TRUE, DnD.
{
   if (!(mask & DND_CP_CAP_PLAIN_TEXT_DND) &&
       !(mask & DND_CP_CAP_PLAIN_TEXT_CP)) {
      CPClipboard_ClearItem(clip, CPFORMAT_TEXT);
   }
   if (!(mask & DND_CP_CAP_RTF_DND) && !(mask & DND_CP_CAP_RTF_CP)) {
      CPClipboard_ClearItem(clip, CPFORMAT_RTF);
   }
   if (!(mask & DND_CP_CAP_IMAGE_DND) && !(mask & DND_CP_CAP_IMAGE_CP)) {
      CPClipboard_ClearItem(clip, CPFORMAT_IMG_PNG);
   }
   if (!(mask & DND_CP_CAP_FILE_DND) && !(mask & DND_CP_CAP_FILE_CP)) {
      CPClipboard_ClearItem(clip, CPFORMAT_FILELIST);
      CPClipboard_ClearItem(clip, CPFORMAT_FILELIST_URI);
   }
   if (!(mask & DND_CP_CAP_FILE_CONTENT_DND) &&
       !(mask & DND_CP_CAP_FILE_CONTENT_CP)) {
      CPClipboard_ClearItem(clip, CPFORMAT_FILECONTENTS);
   }
   return CPClipboard_IsEmpty(clip);
}
