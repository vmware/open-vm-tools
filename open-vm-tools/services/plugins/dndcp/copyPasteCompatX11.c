/*********************************************************
 * Copyright (C) 2005-2019 VMware, Inc. All rights reserved.
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
 * copyPasteCompatX11.c --
 *
 *    Set of functions in guest side for copy/paste (both file and text).
 *    Currently there are 2 versions copy/paste. Version 1 only supports
 *    text copy/paste, and based on backdoor cmd. Version 2 supports both
 *    text and file copy/paste, and based on guestRPC.
 *
 *    G->H Text Copy/Paste (version 1)
 *    --------------------
 *    When Ungrab, CopyPaste_RequestSelection got called, which try to get
 *    selection text and send to backdoor.
 *
 *    H->G Text Copy/Paste (version 1)
 *    --------------------
 *    When grab, CopyPaste_GetBackdoorSelections got called, which first
 *    get host selection text, then claim as selection owner. If some app
 *    asks for selection, CopyPasteSelectionGetCB will reply with host
 *    selection text.
 */

#define G_LOG_DOMAIN "dndcp"

#include "dndPluginIntX11.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#include "vm_assert.h"
#include "copyPasteCompat.h"
#include "str.h"
#include "strutil.h"
#include "dnd.h"
#include "util.h"
#include "cpName.h"
#include "cpNameUtil.h"
#include "vmblock.h"
#include "file.h"
#include "codeset.h"
#include "escape.h"
#include "hostinfo.h"
#include "wiper.h"
#include "vmware/guestrpc/tclodefs.h"

#include "vmware/tools/plugin.h"

/*
 * Gtk 1.2 doesn't know about the CLIPBOARD selection, but that doesn't matter, we
 * just create the atom we need directly in main().
 */
#ifndef GDK_SELECTION_CLIPBOARD
GdkAtom GDK_SELECTION_CLIPBOARD;
#endif

#ifndef GDK_SELECTION_TYPE_TIMESTAMP
GdkAtom GDK_SELECTION_TYPE_TIMESTAMP;
#endif

#ifndef GDK_SELECTION_TYPE_UTF8_STRING
GdkAtom GDK_SELECTION_TYPE_UTF8_STRING;
#endif

/*
 * Currently there are 2 versions copy/paste.
 * Key points in copy/paste version 1:
 * 1. Only text copy/paste
 * 2. copy/paste is based on backdoor directly
 *
 * Key points in copy/paste version 2:
 * 1. Support both file/text copy/paste
 * 2. Both file/text copy/paste are based on guestRPC
 */
static int32 gVmxCopyPasteVersion = 1;

/*
 * Getting a selection is an asyncronous event, so we have to keep track of both
 * selections globablly in order to decide which one to use.
 */
static Bool gWaitingOnGuestSelection = FALSE;
static char gGuestSelPrimaryBuf[MAX_SELECTION_BUFFER_LENGTH];
static char gGuestSelClipboardBuf[MAX_SELECTION_BUFFER_LENGTH];
static uint64 gGuestSelPrimaryTime = 0;
static uint64 gGuestSelClipboardTime = 0;
static char gHostClipboardBuf[MAX_SELECTION_BUFFER_LENGTH + 1];

static Bool gIsOwner;
static ToolsAppCtx *gCtx = NULL;

/*
 * Forward Declarations
 */
static gboolean IsCtxMainLoopActive(void);
static INLINE void CopyPasteStateInit(void);
static void CopyPasteSelectionReceivedCB(GtkWidget *widget,
                                         GtkSelectionData *selection_data,
                                         gpointer data);
static void CopyPasteSelectionGetCB(GtkWidget *widget,
                                    GtkSelectionData *selection_data,
                                    guint info,
                                    guint time_stamp,
                                    gpointer data);
static gint CopyPasteSelectionClearCB(GtkWidget *widget,
                                      GdkEventSelection *event,
                                      gpointer data);

static void CopyPasteSetBackdoorSelections(void);

/* This struct is only used by CopyPasteSelectionRemoveTarget. */
struct SelectionTargetList {
  GdkAtom selection;
  GtkTargetList *list;
};


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteSelectionRemoveTarget --
 *
 *      To remove a target from a selection target list. The reason to develop
 *      this function is that in gtk there is only gtk_selection_add_target to
 *      add supported target to selection list, but no function to remove one.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If no more target, the selection list will be removed too.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteSelectionRemoveTarget(GtkWidget *widget,
                               GdkAtom selection,
                               GdkAtom target)
{
   const char *selection_handler_key = "gtk-selection-handlers";
   struct SelectionTargetList *targetList;
   GList *tempList;
   GList *selectionLists;

   /* Get selection list. */
#ifndef GTK3
   selectionLists = gtk_object_get_data(GTK_OBJECT (widget), selection_handler_key);
#else
   selectionLists = g_object_get_data(G_OBJECT (widget), selection_handler_key);
#endif
   tempList = selectionLists;
   while (tempList) {
      /* Enumerate the list to find the selection. */
      targetList = tempList->data;
      if (targetList->selection == selection) {
         /* Remove target. */
         gtk_target_list_remove(targetList->list, target);
         /* If no more target, remove selection from list. */
#ifndef GTK3
         /* the following code does not build with gtk3 - it may not be
            necessary and gtk_target_list_remove() takes care of it,
            or we create a memory leak. */
         if (!targetList->list->list) {
            /* Free target list. */
            gtk_target_list_unref(targetList->list);
            g_free(targetList);
            /* Remove and free selection node. */
            selectionLists = g_list_remove_link(selectionLists, tempList);
            g_list_free_1(tempList);
         }
#endif
         break;
      }
      tempList = tempList->next;
   }
   /* Put new selection list back. */
#ifndef GTK3
   gtk_object_set_data (GTK_OBJECT (widget), selection_handler_key, selectionLists);
#else
   g_object_set_data (G_OBJECT (widget), selection_handler_key, selectionLists);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_RequestSelection --
 *
 *      Request the guest's text clipboard (asynchronously), we'll give it to 
 *      the host when the request completes. For version 1 guest->host text
 *      copy/paste.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      The owner of the clipboard will get a request from our application.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CopyPaste_RequestSelection(void)
{
   if (gVmxCopyPasteVersion > 1) {
      return FALSE;
   }

   /*
    * Ask for both the PRIMARY and CLIPBOARD selections.
    */
   gGuestSelPrimaryBuf[0] = '\0';
   gGuestSelClipboardBuf[0] = '\0';

   /* Only send out request if we are not the owner. */
   if (!gIsOwner) {
      /* Try to get timestamp for primary and clipboard. */
      gWaitingOnGuestSelection = TRUE;
      gtk_selection_convert(gUserMainWidget,
                            GDK_SELECTION_PRIMARY,
                            GDK_SELECTION_TYPE_TIMESTAMP,
                            GDK_CURRENT_TIME);
      while (IsCtxMainLoopActive() && gWaitingOnGuestSelection) {
         gtk_main_iteration();
      }

      gWaitingOnGuestSelection = TRUE;
      gtk_selection_convert(gUserMainWidget,
                            GDK_SELECTION_CLIPBOARD,
                            GDK_SELECTION_TYPE_TIMESTAMP,
                            GDK_CURRENT_TIME);
      while (IsCtxMainLoopActive() && gWaitingOnGuestSelection) {
         gtk_main_iteration();
      }

      /* Try to get utf8 text from primary and clipboard. */
      gWaitingOnGuestSelection = TRUE;
      gtk_selection_convert(gUserMainWidget,
                            GDK_SELECTION_PRIMARY,
                            GDK_SELECTION_TYPE_UTF8_STRING,
                            GDK_CURRENT_TIME);
      while (IsCtxMainLoopActive() && gWaitingOnGuestSelection) {
         gtk_main_iteration();
      }

      gWaitingOnGuestSelection = TRUE;
      gtk_selection_convert(gUserMainWidget,
                            GDK_SELECTION_CLIPBOARD,
                            GDK_SELECTION_TYPE_UTF8_STRING,
                            GDK_CURRENT_TIME);
      while (IsCtxMainLoopActive() && gWaitingOnGuestSelection) {
         gtk_main_iteration();
      }

      if (gGuestSelPrimaryBuf[0] == '\0' && gGuestSelClipboardBuf[0] == '\0') {
         /*
          * If we cannot get utf8 text, try to get localized text from primary
          * and clipboard.
          */
         gWaitingOnGuestSelection = TRUE;
         gtk_selection_convert(gUserMainWidget,
                               GDK_SELECTION_PRIMARY,
                               GDK_SELECTION_TYPE_STRING,
                               GDK_CURRENT_TIME);
         while (IsCtxMainLoopActive() && gWaitingOnGuestSelection) {
            gtk_main_iteration();
         }

         gWaitingOnGuestSelection = TRUE;
         gtk_selection_convert(gUserMainWidget,
                               GDK_SELECTION_CLIPBOARD,
                               GDK_SELECTION_TYPE_STRING,
                               GDK_CURRENT_TIME);
         while (IsCtxMainLoopActive() && gWaitingOnGuestSelection) {
            gtk_main_iteration();
         }
      }
   }
   /* Send text to host. */
   g_debug("CopyPaste_RequestSelection: Prim is [%s], Clip is [%s]\n",
         gGuestSelPrimaryBuf, gGuestSelClipboardBuf);
   CopyPasteSetBackdoorSelections();
   return TRUE;
}


/**
 * Check to see if we are running in tools main loop.
 *
 * @return TRUE if we are, FALSE if we are not running in main loop.
 */

static gboolean
IsCtxMainLoopActive(void)
{
   ASSERT(gCtx);
   return g_main_loop_is_running(gCtx->mainLoop);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteSelectionReceivedCB --
 *
 *      Callback for the gtk signal "selection_received".
 *      Called because we previously requested a copy/paste selection and
 *      finally got results of that asynchronous operation. After some basic
 *      sanity checks, send the result (in selection_data) thru the backdoor
 *      (version 1) or guestRPC (version 2) so the vmx can copy it to host
 *      clipboard.
 *
 *      We made several requests for selections, the string (actual data) and
 *      file list for each of PRIMARY and CLIPBOARD selections. So this funtion
 *      will get called several times, once for each request.
 *
 *      For guest->host copy/paste (both text and file).
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
CopyPasteSelectionReceivedCB(GtkWidget *widget,                // IN: unused
                             GtkSelectionData *selection_data, // IN: requested data
                             gpointer data)                    // IN: unused
{
   char *target;
#ifndef GTK3
   char *utf8Str = NULL;
#else
   const char *utf8Str = NULL;
   char *utf8Str_cvt = NULL;
#endif
   size_t len;
   size_t aligned_len;

   if ((widget == NULL) || (selection_data == NULL)) {
      g_debug("CopyPasteSelectionReceivedCB: Error, widget or selection_data is invalid\n");
      goto exit;
   }

#ifndef GTK3
   if (selection_data->length < 0) {
#else
   if (gtk_selection_data_get_length(selection_data) < 0) {
#endif
      g_debug("CopyPasteSelectionReceivedCB: Error, length less than 0\n");
      goto exit;
   }

   /* Try to get clipboard or selection timestamp. */
#ifndef GTK3
   if (selection_data->target == GDK_SELECTION_TYPE_TIMESTAMP) {
      if (selection_data->selection == GDK_SELECTION_PRIMARY) {
         if (selection_data->length == 4) {
            gGuestSelPrimaryTime = *(uint32 *)selection_data->data;
#else
   if (gtk_selection_data_get_target(selection_data) == GDK_SELECTION_TYPE_TIMESTAMP) {
      if (gtk_selection_data_get_selection(selection_data) == GDK_SELECTION_PRIMARY) {
         if (gtk_selection_data_get_length(selection_data) == 4) {
            gGuestSelPrimaryTime = *(uint32 *)gtk_selection_data_get_data(selection_data);
#endif
            g_debug("CopyPasteSelectionReceivedCB: Got pri time [%"FMT64"u]\n",
                  gGuestSelPrimaryTime);
#ifndef GTK3
         } else if (selection_data->length == 8) {
            gGuestSelPrimaryTime = *(uint64 *)selection_data->data;
#else
         } else if (gtk_selection_data_get_length(selection_data) == 8) {
            gGuestSelPrimaryTime = *(uint64 *)gtk_selection_data_get_data(selection_data);
#endif
            g_debug("CopyPasteSelectionReceivedCB: Got pri time [%"FMT64"u]\n",
                  gGuestSelPrimaryTime);
         } else {
            g_debug("CopyPasteSelectionReceivedCB: Unknown pri time. Size %d\n",
#ifndef GTK3
                  selection_data->length);
#else
                  gtk_selection_data_get_length(selection_data));
#endif
         }
      }
#ifndef GTK3
      if (selection_data->selection == GDK_SELECTION_CLIPBOARD) {
         if (selection_data->length == 4) {
            gGuestSelClipboardTime = *(uint32 *)selection_data->data;
#else
      if (gtk_selection_data_get_selection(selection_data) == GDK_SELECTION_CLIPBOARD) {
         if (gtk_selection_data_get_length(selection_data) == 4) {
            gGuestSelClipboardTime = *(uint32 *)gtk_selection_data_get_data(selection_data);
#endif
            g_debug("CopyPasteSelectionReceivedCB: Got clip time [%"FMT64"u]\n",
                  gGuestSelClipboardTime);
#ifndef GTK3
         } else if (selection_data->length == 8) {
            gGuestSelClipboardTime = *(uint64 *)selection_data->data;
#else
         } else if (gtk_selection_data_get_length(selection_data) == 8) {
            gGuestSelClipboardTime = *(uint64 *)gtk_selection_data_get_data(selection_data);
#endif
            g_debug("CopyPasteSelectionReceivedCB: Got clip time [%"FMT64"u]\n",
                  gGuestSelClipboardTime);
         } else {
            g_debug("CopyPasteSelectionReceivedCB: Unknown clip time. Size %d\n",
#ifndef GTK3
                  selection_data->length);
#else
                  gtk_selection_data_get_length(selection_data));
#endif
         }
      }
      goto exit;
   }

#ifndef GTK3
   if (selection_data->selection == GDK_SELECTION_PRIMARY) {
#else
   if (gtk_selection_data_get_selection(selection_data) == GDK_SELECTION_PRIMARY) {
#endif
      target = gGuestSelPrimaryBuf;
#ifndef GTK3
   } else if (selection_data->selection == GDK_SELECTION_CLIPBOARD) {
#else
   } else if (gtk_selection_data_get_selection(selection_data) == GDK_SELECTION_CLIPBOARD) {
#endif
      target = gGuestSelClipboardBuf;
   } else {
      goto exit;
   }

#ifndef GTK3
   utf8Str = selection_data->data;
   len = strlen(selection_data->data);

   if (selection_data->target != GDK_SELECTION_TYPE_STRING &&
       selection_data->target != GDK_SELECTION_TYPE_UTF8_STRING) {
      /* It is a file list. */
      if (len >= MAX_SELECTION_BUFFER_LENGTH - 1) {
         Warning("CopyPasteSelectionReceivedCB file list too long\n");
      } else {
         memcpy(target, selection_data->data, len + 1);
      }
      goto exit;
   }
#else
   utf8Str = gtk_selection_data_get_data(selection_data);
   len = strlen(gtk_selection_data_get_data(selection_data));

   if (gtk_selection_data_get_target(selection_data) != GDK_SELECTION_TYPE_STRING &&
       gtk_selection_data_get_target(selection_data) != GDK_SELECTION_TYPE_UTF8_STRING) {
      /* It is a file list. */
      if (len >= MAX_SELECTION_BUFFER_LENGTH - 1) {
         Warning("CopyPasteSelectionReceivedCB file list too long\n");
      } else {
         memcpy(target, gtk_selection_data_get_data(selection_data), len + 1);
      }
      goto exit;
   }
#endif

   /*
    * If target is GDK_SELECTION_TYPE_STRING, assume encoding is local code
    * set. Convert to utf8 before send to vmx.
    */
#ifndef GTK3
   if (selection_data->target == GDK_SELECTION_TYPE_STRING &&
       !CodeSet_CurrentToUtf8(selection_data->data,
                              selection_data->length,
                              &utf8Str,
                              &len)) {
      g_debug("CopyPasteSelectionReceivedCB: Couldn't convert to utf8 code set\n");
      gWaitingOnGuestSelection = FALSE;
      return;
   }
#else
   if (gtk_selection_data_get_target(selection_data) == GDK_SELECTION_TYPE_STRING) {
      if (!CodeSet_CurrentToUtf8(gtk_selection_data_get_data(selection_data),
                              gtk_selection_data_get_length(selection_data),
                              &utf8Str_cvt,
                              &len)) {
         g_debug("CopyPasteSelectionReceivedCB: Couldn't convert to utf8 code set\n");
         gWaitingOnGuestSelection = FALSE;
         return;
      } else {
         utf8Str = utf8Str_cvt;
      }
   }
#endif

   /*
    * String in backdoor communication is 4 bytes by 4 bytes, so the len
    * should be aligned to 4;
    */
   aligned_len = (len + 4) & ~3;
   if (aligned_len >= MAX_SELECTION_BUFFER_LENGTH) {
      /* With alignment, len is still possible to be less than max. */
      if (len < (MAX_SELECTION_BUFFER_LENGTH - 1)) {
         memcpy(target, utf8Str, len + 1);
      } else {
         memcpy(target, utf8Str, MAX_SELECTION_BUFFER_LENGTH - 1);
         target[MAX_SELECTION_BUFFER_LENGTH - 1] ='\0';
      }
   } else {
      memcpy(target, utf8Str, len + 1);
   }

exit:
#ifndef GTK3
   if (selection_data->target == GDK_SELECTION_TYPE_STRING) {
      free(utf8Str);
#else
   if (gtk_selection_data_get_target(selection_data) == GDK_SELECTION_TYPE_STRING) {
      free(utf8Str_cvt);
#endif
   }
   gWaitingOnGuestSelection = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteSelectionGetCB --
 *
 *      Callback for the gtk signal "selection_get".
 *      This is called when some other app requests the copy/paste selection,
 *      probably because we declare oursleves the selection owner on mouse
 *      grab. In text copy/paste case, we simply respond with contents of
 *      gHostClipboardBuf, which should have been set on mouse grab. In file
 *      copy/paste case, send file transfer request to host vmx, then return
 *      file list with right format according to different request.
 *      For host->guest copy/paste (both text and file).
 *
 * Results:
 *      None
 *
 * Side effects:
 *      An X message is sent to the requesting app containing the data, it
 *      will likely act on it in some way. In FCP case, may first start a
 *      host->guest file transfer. Add block if blocking driver is available,
 *      otherwise wait till file copy done.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteSelectionGetCB(GtkWidget        *widget,         // IN: unused
                        GtkSelectionData *selection_data, // IN: requested type
                                                          // OUT:the data to be sent
                        guint            info,            // IN: unused
                        guint            time_stamp,      // IN: unsued
                        gpointer         data)            // IN: unused
{
   if ((widget == NULL) || (selection_data == NULL)) {
      g_debug("CopyPasteSelectionGetCB: Error, widget or selection_data is invalid\n");
      return;
   }

   GdkAtom target;
#ifndef GTK3
   target = selection_data->target;
#else
   target = gtk_selection_data_get_target(selection_data);
#endif

   /* If it is text copy paste, return gHostClipboardBuf. */
   if (GDK_SELECTION_TYPE_STRING == target ||
       GDK_SELECTION_TYPE_UTF8_STRING == target) {
      char *outBuf = gHostClipboardBuf;
      char *outStringBuf = NULL;
      size_t len = strlen(gHostClipboardBuf);

      /*
       * If target is GDK_SELECTION_TYPE_STRING, assume encoding is local code
       * set. Convert from utf8 to local one.
       */
      if (GDK_SELECTION_TYPE_STRING == target &&
          !CodeSet_Utf8ToCurrent(gHostClipboardBuf,
                                 strlen(gHostClipboardBuf),
                                 &outStringBuf,
                                 &len)) {
         g_debug("CopyPasteSelectionGetCB: can not convert to current codeset\n");
         return;
      }

      if (outStringBuf != NULL) {
         outBuf = outStringBuf;
      }

      gtk_selection_data_set(selection_data, target, 8, outBuf, len);
      g_debug("CopyPasteSelectionGetCB: Set text [%s]\n", outBuf);

      free(outStringBuf);
      return;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteSelectionClearCB --
 *
 *      Callback for the gtk signal "selection_clear".
 *
 * Results:
 *      Always TRUE.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gint
CopyPasteSelectionClearCB(GtkWidget          *widget,         // IN: unused
                          GdkEventSelection  *event,          // IN: unused
                          gpointer           data)            // IN: unused
{
   g_debug("CopyPasteSelectionClearCB got clear signal\n");
   gIsOwner = FALSE;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteSetBackdoorSelections --
 *
 *      Set the clipboard one of two ways, the old way or the new way.
 *      The old way uses CopyPaste_SetSelLength and there's only one selection.
 *      Set backdoor selection with either primary selection or clipboard.
 *      The primary selection is the first priority, then clipboard.
 *      If both unavailable, set backdoor selection length to be 0.
 *      This will be used by older VMXs or VMXs on Windows hosts (which
 *      has only one clipboard). Doing this gives us backwards
 *      compatibility.
 *
 *      The new way uses new sets both PRIMARY and CLIPBOARD. Newer Linux
 *      VMXs will use these rather than the above method and have the two
 *      selections set separately.
 *
 *      XXX: The "new way" doesn't exist yet, the vmx has no support for it.
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
   size_t len;
   size_t aligned_len;
   size_t primaryLen;
   size_t clipboardLen;
   unsigned int i;

   primaryLen = strlen(gGuestSelPrimaryBuf);
   clipboardLen = strlen(gGuestSelClipboardBuf);

   if (primaryLen && clipboardLen) {
      /* Pick latest one if both are available. */
      p = gGuestSelPrimaryTime >= gGuestSelClipboardTime ?
         (uint32 const *)gGuestSelPrimaryBuf :
         (uint32 const *)gGuestSelClipboardBuf;
   } else if (primaryLen) {
      /*
       * Send primary selection to backdoor if it exists.
       */
      p = (uint32 const *)gGuestSelPrimaryBuf;
   } else if (clipboardLen) {
      /*
       * Otherwise send clipboard to backdoor if it exists.
       */
      p = (uint32 const *)gGuestSelClipboardBuf;
   } else {
      /*
       * Neither selection is set
       */
      p = NULL;
   }

   if (p == NULL) {
      CopyPaste_SetSelLength(0);
      g_debug("CopyPasteSetBackdoorSelections Set empty text.\n");
   } else {
      len = strlen((char *)p);
      g_debug("CopyPasteSetBackdoorSelections Set text [%s].\n", (char *)p);
      aligned_len = (len + 4) & ~3;

      /* Here long string should already be truncated. */
      ASSERT(aligned_len <= MAX_SELECTION_BUFFER_LENGTH);

      CopyPaste_SetSelLength(len);
      for (i = 0; i < len; i += 4, p++) {
         CopyPaste_SetNextPiece(*p);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_GetBackdoorSelections --
 *
 *      Get the clipboard "the old way".
 *      The old way uses CopyPaste_GetHostSelectionLen and there's only one
 *      selection. We don't have to do anything for the "new way", since the
 *      host will just push PRIMARY and/or CLIPBOARD when they are available
 *      on the host.
 *
 *      XXX: the "new way" isn't availble yet because the vmx doesn't
 *           implement separate clipboards. Even when it does this
 *           function will still exist for backward compatibility
 *
 * Results:
 *      TRUE if selection length>=0, FALSE otherwise.
 *
 * Side effects:
 *      This application becomes the selection owner for PRIMARY and/or
        CLIPBOARD selections.
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
      g_debug("CopyPaste_GetBackdoorSelections Get text [%s].\n", gHostClipboardBuf);
      gtk_selection_owner_set(gUserMainWidget,
                              GDK_SELECTION_CLIPBOARD,
                              GDK_CURRENT_TIME);
      gtk_selection_owner_set(gUserMainWidget,
                              GDK_SELECTION_PRIMARY,
                              GDK_CURRENT_TIME);
      gIsOwner = TRUE;
   }
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
   /* Text copy/paste initialization for all versions. */
#ifndef GDK_SELECTION_CLIPBOARD
   GDK_SELECTION_CLIPBOARD = gdk_atom_intern("CLIPBOARD", FALSE);
#endif

#ifndef GDK_SELECTION_TYPE_TIMESTAMP
   GDK_SELECTION_TYPE_TIMESTAMP = gdk_atom_intern("TIMESTAMP", FALSE);
#endif

#ifndef GDK_SELECTION_TYPE_UTF8_STRING
   GDK_SELECTION_TYPE_UTF8_STRING = gdk_atom_intern("UTF8_STRING", FALSE);
#endif

   /*
    * String is always in supported list. FCP atoms will dynamically be
    * added and removed.
    */
   gtk_selection_add_target(mainWnd, GDK_SELECTION_PRIMARY,
                            GDK_SELECTION_TYPE_STRING, 0);
   gtk_selection_add_target(mainWnd, GDK_SELECTION_CLIPBOARD,
                            GDK_SELECTION_TYPE_STRING, 0);
   gtk_selection_add_target(mainWnd, GDK_SELECTION_PRIMARY,
                            GDK_SELECTION_TYPE_UTF8_STRING, 0);
   gtk_selection_add_target(mainWnd, GDK_SELECTION_CLIPBOARD,
                            GDK_SELECTION_TYPE_UTF8_STRING, 0);

#ifndef GTK3
   gtk_signal_connect(GTK_OBJECT(mainWnd), "selection_received",
                      GTK_SIGNAL_FUNC(CopyPasteSelectionReceivedCB), mainWnd);
   gtk_signal_connect(GTK_OBJECT(mainWnd), "selection_get",
                      GTK_SIGNAL_FUNC(CopyPasteSelectionGetCB), mainWnd);
   gtk_signal_connect(GTK_OBJECT(mainWnd), "selection_clear_event",
                      GTK_SIGNAL_FUNC(CopyPasteSelectionClearCB), mainWnd);
#else
   g_signal_connect(G_OBJECT(mainWnd), "selection_received",
                    G_CALLBACK(CopyPasteSelectionReceivedCB), mainWnd);
   g_signal_connect(G_OBJECT(mainWnd), "selection_get",
                    G_CALLBACK(CopyPasteSelectionGetCB), mainWnd);
   g_signal_connect(G_OBJECT(mainWnd), "selection_clear_event",
                    G_CALLBACK(CopyPasteSelectionClearCB), mainWnd);
#endif

   CopyPasteStateInit();

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_Unregister --
 *
 *      Cleanup copy/paste related things.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      copy/paste is stopped, the rpc channel to the vmx is closed.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPaste_Unregister(GtkWidget* mainWnd)
{
   g_debug("%s: enter\n", __FUNCTION__);
#ifndef GTK3
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(CopyPasteSelectionReceivedCB),
                                 mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(CopyPasteSelectionGetCB),
                                 mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(CopyPasteSelectionClearCB),
                                 mainWnd);
#else
   g_signal_handlers_disconnect_by_func(G_OBJECT(mainWnd),
                               G_CALLBACK(CopyPasteSelectionReceivedCB),
                               mainWnd);
   g_signal_handlers_disconnect_by_func(G_OBJECT(mainWnd),
                               G_CALLBACK(CopyPasteSelectionGetCB),
                               mainWnd);
   g_signal_handlers_disconnect_by_func(G_OBJECT(mainWnd),
                               G_CALLBACK(CopyPasteSelectionClearCB),
                               mainWnd);
#endif
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


/*
 *----------------------------------------------------------------------------
 *
 * CopyPasteStateInit --
 *
 *    Initalialize CopyPaste State.
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
CopyPasteStateInit(void)
{
   g_debug("%s: enter\n", __FUNCTION__);
   gHostClipboardBuf[0] = '\0';
   gGuestSelPrimaryBuf[0] = '\0';
   gGuestSelClipboardBuf[0] = '\0';
   gIsOwner = FALSE;
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
