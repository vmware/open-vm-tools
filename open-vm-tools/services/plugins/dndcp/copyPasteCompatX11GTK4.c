/*********************************************************
 * Copyright (c) 2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
 * copyPasteCompatX11GTK4.c --
 *
 *    Like copyPasteCompatX11.c, this is the GTK4 version implementation for
 *    Linux OS for guest<-->host text copy/paste. We need separate
 *    implementation for GTK4 as its API changed dramaticly compared to GTK3.
 *
 *    Set of functions in this file are for guest side text copy/paste between
 *    host and guest. Currently there are 4 versions copy/paste.
 *    ** Version 1 is based on backdoor, supports only text copy/paste,
 *    available with GTK3/GTK4.
 *    ** Version 2/3/4 are all based on tools guestRPC.
 *    *** Version 2 only existed for a certain period for compatibility with
 *    pre-2000 windows guest and is deprecated.
 *    *** Version 3 also existed for compatibility with some old tools with
 *    Fusion 3, WS 7 etc and is considered deprecated now.
 *    *** Version 4 is the current version, which supports both text and file
 *    copy/paste.
 *    Refer to "bora/vmx/tools/dndController" for more details.
 *
 *    ### G->H Text Copy/Paste (version 1) ###
 *    --------------------
 *    When Ungrab, CopyPaste_RequestSelection got called, which try to get
 *    selection text and send to backdoor.
 *
 *    ### H->G Text Copy/Paste (version 1) ###
 *    --------------------
 *    When grab, CopyPaste_GetBackdoorSelections got called, which first
 *    get host selection text and set its content in gHostClipboardBuf.
 *    Then the gGdkDisplay default clipboard is set with its content.
 */
#ifndef GTK4
#error "This should only build with GTK4"
#endif
#define G_LOG_DOMAIN "dndcp"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "copyPasteCompat.h"
#include "dndPluginIntX11.h"
#include "util.h"
#include "vm_assert.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/plugin.h"

/*
 * Currently there are 2 active versions copy/paste: version 1 and version 4
 * (version 2/3  are considered deprecated):
 * Key points in copy/paste version 1:
 * 1. Only text copy/paste
 * 2. copy/paste is based on backdoor directly
 *
 * Key points in copy/paste version 4:
 * 1. Support both file/text copy/paste
 * 2. Both file/text copy/paste are based on guestRPC
 */
static int32 gVmxCopyPasteVersion = 1;

/*
 * Here gHostClipboardBuf is one byte lager than MAX_SELECTION_BUFFER_LENGTH,
 * As the backdoor bit copy is 4-byte aligned, we could copy H->G with exactly
 * MAX_SELECTION_BUFFER_LENGTH(65536 - 100) bytes at most as its value is 4-byte
 * aligned, we need additional 1 termination char '\0'.
 * While for the gGuestClipboardBuf, as we are copy G->H also with 4-byte
 * aligned, we need to include the termination '\0' at the last char, thus it
 * could be MAX_SELECTION_BUFFER_LENGTH at most.
 */
static char gGuestClipboardBuf[MAX_SELECTION_BUFFER_LENGTH];
static char gHostClipboardBuf[MAX_SELECTION_BUFFER_LENGTH + 1];
static unsigned int gGuestClipGenNum;
static unsigned int gHostClipGenNum;
static gint64 gGuestDefaultClipTime = 0;
static gint64 gGuestPrimaryClipTime = 0;

static ToolsAppCtx *gCtx = NULL;
static GdkClipboard *gDefaultClipBd = NULL;
static GdkClipboard *gPrimaryClipBd = NULL;

/*
 * Forward Declarations
 */
static void CopyPasteSetBackdoorSelections(void);
static void CopyPasteRequestTextCb(GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data);
static void GuestDefaultClipbdChangedCb(GdkClipboard *clipboard,
                                        gpointer user_data);
static void GuestPrimaryClipbdChangedCb(GdkClipboard *clipboard,
                                        gpointer user_data);


/*
 *-----------------------------------------------------------------------------
 *
 * GuestDefaultClipbdChangedCb --
 *
 *      Callback when the guest Default clipboard changed.
 *      Either guest own app or the H-G copy could change the guest clipboard.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If clipboard text changed, gGuestCpGenNum is increased by 1 and
 *      gGuestDefaultClipTime is updated.
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestDefaultClipbdChangedCb(GdkClipboard *clipboard,   // UNUSED
                            gpointer user_data)        // UNUSED
{
   GdkContentFormats *clipFormats = gdk_clipboard_get_formats(gDefaultClipBd);

   /*
    * Only update when clipboard text content changed,
    * as v1 copy/paste only supports plain text
    */
   if (gdk_content_formats_contain_gtype(clipFormats, G_TYPE_STRING)) {
      ++gGuestClipGenNum;
      gGuestDefaultClipTime = g_get_real_time();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestPrimaryClipbdChangedCb --
 *
 *      Callback when the guest Primary clipboard changed.
 *      Either guest own app or the H-G copy could change the guest clipboard.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If clipboard text changed, gGuestCpGenNum is increased by 1 and
 *      gGuestPrimaryClipTime is updated.
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestPrimaryClipbdChangedCb(GdkClipboard *clipboard,   // UNUSED
                            gpointer user_data)        // UNUSED
{
   GdkContentFormats *clipFormats = gdk_clipboard_get_formats(gPrimaryClipBd);

   /*
    * Only update when clipboard text content changed,
    * as v1 copy/paste only supports plain text
    */
   if (gdk_content_formats_contain_gtype(clipFormats, G_TYPE_STRING)) {
      ++gGuestClipGenNum;
      gGuestPrimaryClipTime = g_get_real_time();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 *  CopyPasteRequestTextCb--
 *
 *      Callback when finish reading the guest's text clipboard, and then send
 *      the content buffer to host.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      On Success:
 *      ** gGuestClipboardBuf is set with the guest clipboard text
 *      ** gGuestClipboardBuf content is sent to host with backdoor.
 *      ** gHostClipGenNum is set to gGuestClipGenNum.
 *-----------------------------------------------------------------------------
 */

static void
CopyPasteRequestTextCb(GObject *source_object,  // IN/OUT: clipboard
                       GAsyncResult *result,    // UNUSED
                       gpointer user_data)      // UNUSED
{
   GdkClipboard *clipboard = GDK_CLIPBOARD(source_object);
   GError *err = NULL;
   size_t len;
   /* gdk_clipboard_read_text_finish return NUL terminated UTF-8 string */
   char *cpStr = gdk_clipboard_read_text_finish(clipboard, result, &err);

   if (cpStr != NULL) {
      len = strlen(cpStr);
      if (len >= MAX_SELECTION_BUFFER_LENGTH) {
          /*
           * At most MAX_SELECTION_BUFFER_LENGTH-1 size string could be copied
           * G->H clipboard, as backdoor copy is 4-byte aligned and we need to
           * keep the last char as '\0' terminator
           */
          len = MAX_SELECTION_BUFFER_LENGTH - 1;
          g_warning("%s: Guest clipboard selection exceeds [%zu] bytes, text truncated\n",
                    __FUNCTION__, len);
      }
      memcpy(gGuestClipboardBuf, cpStr, len);
      gGuestClipboardBuf[len] = '\0';
      g_free(cpStr);
      g_debug("%s: Guest clipboard GenNum=%d, text is [%s]\n", __FUNCTION__,
              gGuestClipGenNum, gGuestClipboardBuf);
      CopyPasteSetBackdoorSelections();
      gHostClipGenNum = gGuestClipGenNum;
   }

   if (err != NULL) {
      /* clipboard failure is not critical one,  warn but do not error exit */
      g_warning("%s: Error to copy from guest clipboard: %s\n", __FUNCTION__,
                err->message);
      g_error_free(err);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_RequestSelection --
 *
 *      If gHostClipGenNum is different from gGuestClipGenNum, request the
 *      guest's text clipboard (asynchronously). Once the clipboard read request
 *      is completed the Callback function is invoked to send the buffer to
 *      the host.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CopyPaste_RequestSelection(void)
{
   if (gVmxCopyPasteVersion > 1) {
      return FALSE;
   }

   if (gHostClipGenNum == gGuestClipGenNum) {
      g_debug("%s: Guest clipboard in sync with host with the same GenNum=%d, "
              "skip send", __FUNCTION__, gGuestClipGenNum);
   } else {
      gGuestClipboardBuf[0] = '\0';
      GdkClipboard *cpClipboard = (gGuestPrimaryClipTime > gGuestDefaultClipTime)
                                 ? gPrimaryClipBd
                                 : gDefaultClipBd;
      gdk_clipboard_read_text_async(cpClipboard, NULL, CopyPasteRequestTextCb, NULL);
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteSetBackdoorSelections --
 *
 *      Set the clipboard uses CopyPaste_SetSelLength and set backdoor selection
 *      with clipboard. If unavailable, set backdoor selection length to be 0.
 *      Unlike its GTK3 implementation in copyPasteCompatX11.c, we could
 *      not compare with the timestamps so only the default clipboard is used.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The VMX probably changes some string buffers.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteSetBackdoorSelections(void)
{
   uint32 const *p;
   size_t len = strlen(gGuestClipboardBuf);

   if (len > 0) {
      p = (uint32 const *)gGuestClipboardBuf;
      size_t alignedLen;
      unsigned int i;

      /* Here long string should already be truncated in caller function. */
      alignedLen = (len + 4) & ~3;
      ASSERT(alignedLen <= MAX_SELECTION_BUFFER_LENGTH);

      CopyPaste_SetSelLength(len);
      g_debug("%s: Set host clipboard with [%zu] text.\n", __FUNCTION__, len);
      for (i = 0; i < len; i += 4, p++) {
         CopyPaste_SetNextPiece(*p);
      }
   } else {
      CopyPaste_SetSelLength(0);
      g_debug("%s: Set host clipboard with empty text.\n", __FUNCTION__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_GetBackdoorSelections --
 *
 *      Get the clipboard "the old way".
 *      The old way uses CopyPaste_GetHostSelectionLen and there's only one
 *      selection and its content is in gHostClipboardBuf.
 *
 *      XXX: the "new way" isn't availble yet because the vmx doesn't
 *           implement separate clipboards. Even when it does this
 *           function will still exist for backward compatibility
 *
 * Results:
 *      TRUE if 0 <= selLength <= MAX_SELECTION_BUFFER_LENGTH, FALSE otherwise.
 *
 * Side effects:
 *      * guest clipboard is set with the host's clipboard content.
 *      * gHostClipGenNum is increased by 1.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CopyPaste_GetBackdoorSelections(void)
{
   int selLength;

   if (gVmxCopyPasteVersion > 1) {
      return TRUE;
   }

   selLength = CopyPaste_GetHostSelectionLen();
   if (selLength < 0 || selLength > MAX_SELECTION_BUFFER_LENGTH) {
      return FALSE;
   } else if (selLength > 0) {
      CopyPaste_GetHostSelection(selLength, gHostClipboardBuf);
      gHostClipboardBuf[selLength] = 0;
      ++gHostClipGenNum;
      g_debug("%s: Host clipboard GenNum=%d, text is [%s].\n", __FUNCTION__,
              gHostClipGenNum, gHostClipboardBuf);
      /* Initialize a GValue with the contents of the gHostClipboardBuf */
      GValue value = G_VALUE_INIT;

      g_value_init(&value, G_TYPE_STRING);
      g_value_set_string(&value, gHostClipboardBuf);
      /* H->G copy paste, store the value in guest Default and Primary clipboard */
      gdk_clipboard_set_value(gDefaultClipBd, &value);
      gdk_clipboard_set_value(gPrimaryClipBd, &value);
      g_value_unset(&value);
   }
   /* finally return TRUE if setLength>=0 */
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_Register --
 *
 *      Setup callbacks, initialize.
 *
 * Results:
 *      Always TRUE.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CopyPaste_Register(GtkWidget* mainWnd,   // IN
                   ToolsAppCtx *ctx)     // IN
{
   g_debug("%s: enter\n", __FUNCTION__);
   ASSERT(mainWnd);
   ASSERT(ctx);

   gCtx = ctx;
   gHostClipboardBuf[0] = '\0';
   gGuestClipboardBuf[0] = '\0';
   gGuestClipGenNum = 0;
   gHostClipGenNum = 0;
   gDefaultClipBd = gdk_display_get_clipboard(gGdkDisplay);
   gPrimaryClipBd = gdk_display_get_primary_clipboard(gGdkDisplay);

   g_signal_connect(gDefaultClipBd, "changed",
                    G_CALLBACK(GuestDefaultClipbdChangedCb), NULL);
   g_signal_connect(gPrimaryClipBd, "changed",
                    G_CALLBACK(GuestPrimaryClipbdChangedCb), NULL);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_Unregister --
 *
 *      Currently do nothing. Dummy function in GTK4.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPaste_Unregister(GtkWidget* mainWnd)
{
   return;
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPaste_IsRpcCPSupported --
 *
 *    Check if RPC copy/paste is supported by vmx or not.
 *
 * Results:
 *    FALSE always.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CopyPaste_IsRpcCPSupported(void)
{
   return gVmxCopyPasteVersion > 1;
}


/**
 * Set the copy paste version.
 *
 * @param[in] version version to set.
 */

void
CopyPaste_SetVersion(int version)
{
   g_debug("%s: enter version %d\n", __FUNCTION__, version);
   gVmxCopyPasteVersion = version;
}
