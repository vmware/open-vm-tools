/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * copyPaste.c --
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
 *
 *    G->H Copy/Paste (version 2)
 *    --------------------
 *    When Ungrab, host vmx will send out rpc command "copypaste.gh.data.get",
 *    which handler is CopyPasteRpcInGHSetDataCB. It first tries to get selection
 *    contents and sends it back as rpc result. For file transfer, host vmx
 *    will transfer files with hgFileCopy lib. Function CopyPasteGHFileListGetNext
 *    will handle the file list during transfer.
 *
 *    H->G Copy/Paste (version 2)
 *    --------------------
 *    When grab, host vmx will send host selection content to guest with
 *    out rpc command "copypaste.hg.data.set", which handler is
 *    CopyPasteRpcInHGSetDataCB. It first keeps a copy then claim as selection
 *    owner. If some app asks for files, CopyPasteSelectionGetCB will ask host
 *    vmx to transfer files into a temp dir with rpc "copypaste.hgCopyFiles",
 *    then reply with host selection file list. For KDE and gnome the file list
 *    format is different. If someapp asks for text, CopyPasteSelectionGetCB
 *    will provide the data.
 */

#include "vmwareuserInt.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#include "vm_assert.h"
#include "debug.h"
#include "str.h"
#include "strutil.h"
#include "vm_app.h"
#include "eventManager.h"
#include "guestApp.h"
#include "dnd.h"
#include "util.h"
#include "cpName.h"
#include "cpNameUtil.h"
#include "guestInfoServer.h"
#include "vmblock.h"
#include "file.h"
#include "codeset.h"


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

/* FCP target used in gnome. */
#define FCP_TARGET_NAME_GNOME_COPIED_FILES   "x-special/gnome-copied-files"
#define FCP_TARGET_INFO_GNOME_COPIED_FILES   0
/* FCP target used in KDE. */
#define FCP_TARGET_NAME_URI_LIST             "text/uri-list"
#define FCP_TARGET_INFO_URI_LIST             1
/* Number of FCP targets. */
#define NR_FCP_TARGETS                       2

typedef struct FCPGHState {
   char *fileList;
   char *fileListNext;
   size_t fileListSize;
} FCPGHState;

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
static char gHostClipboardBuf[MAX_SELECTION_BUFFER_LENGTH];

/* Guest->Host state. */
FCPGHState gFcpGHState;
/* RPC buffer for Guest->Host FCP. */
static char *gGHFCPRpcResultBuffer;
/* File list size for Guest->Host FCP. */
static size_t gGHFCPListSize;
static Bool gHGFCPPending;
/* Current selection is text or file list (for FCP). */
static Bool gHGIsClipboardFCP;
/* 
 * Total file size in selection list. This is used to check if there is enough
 * space in guest OS for host->guest file transfer.
 */
static uint64 gHGFCPTotalSize;

static GdkAtom gFCPAtom[NR_FCP_TARGETS];

/* Host->guest file transfer status, used for sync between transfer and paste. */
int gHGFCPFileTransferStatus;

static char gFileRoot[DND_MAX_PATH];
static size_t gFileRootSize;
static Bool gIsOwner;

/*
 * Forward Declarations
 */
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
static Bool CopyPasteRpcInGHSetDataCB(char const **result, size_t *resultLen,
                                      const char *name, const char *args,
                                      size_t argsSize,void *clientData);
static Bool CopyPasteRpcInHGSetDataCB(char const **result, size_t *resultLen,
                                      const char *name, const char *args,
                                      size_t argsSize,void *clientData);

static INLINE void CopyPasteGHFileListClear(void);
static INLINE void CopyPasteGHFileListSet(char *fileList, size_t fileListSize);

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
   selectionLists = gtk_object_get_data(GTK_OBJECT (widget), selection_handler_key);
   tempList = selectionLists;
   while (tempList) {
      /* Enumerate the list to find the selection. */
      targetList = tempList->data;
      if (targetList->selection == selection) {
         /* Remove target. */
         gtk_target_list_remove(targetList->list, target);
         /* If no more target, remove selection from list. */
         if (!targetList->list->list) {
            /* Free target list. */
            gtk_target_list_unref(targetList->list);
            g_free(targetList);
            /* Remove and free selection node. */
            selectionLists = g_list_remove_link(selectionLists, tempList);
            g_list_free_1(tempList);
         }
         break;
      }
      tempList = tempList->next;
   }
   /* Put new selection list back. */
   gtk_object_set_data (GTK_OBJECT (widget), selection_handler_key, selectionLists);
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
 *      None.
 *
 * Side effects:
 *      The owner of the clipboard will get a request from our application.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPaste_RequestSelection(void)
{
   if (gVmxCopyPasteVersion > 1) {
      return;
   }

   /*
    * Ask for both the PRIMARY and CLIPBOARD selections.
    */
   gGuestSelPrimaryBuf[0] = '\0';
   gGuestSelClipboardBuf[0] = '\0';

   /* Only send out request if we are not the owner. */
   if (!gIsOwner) {
      /* Try to get utf8 text from primary and clipboard. */
      gWaitingOnGuestSelection = TRUE;
      gtk_selection_convert(gUserMainWidget,
                            GDK_SELECTION_PRIMARY,
                            GDK_SELECTION_TYPE_UTF8_STRING,
                            GDK_CURRENT_TIME);
      while (gWaitingOnGuestSelection) gtk_main_iteration();

      gWaitingOnGuestSelection = TRUE;
      gtk_selection_convert(gUserMainWidget,
                            GDK_SELECTION_CLIPBOARD,
                            GDK_SELECTION_TYPE_UTF8_STRING,
                            GDK_CURRENT_TIME);
      while (gWaitingOnGuestSelection) gtk_main_iteration();

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
         while (gWaitingOnGuestSelection) gtk_main_iteration();

         gWaitingOnGuestSelection = TRUE;
         gtk_selection_convert(gUserMainWidget,
                               GDK_SELECTION_CLIPBOARD,
                               GDK_SELECTION_TYPE_STRING,
                               GDK_CURRENT_TIME);
         while (gWaitingOnGuestSelection) gtk_main_iteration();
      }
   }
   /* Send text to host. */
   Debug("CopyPaste_RequestSelection: Prim is [%s], Clip is [%s]\n",
         gGuestSelPrimaryBuf, gGuestSelClipboardBuf);
   CopyPasteSetBackdoorSelections();
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteSelectionReceivedCB --
 *
 *      Callback for the gtk signal "selection_recieved".
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
   char *utf8Str = NULL;
   size_t len;
   size_t aligned_len;

   if ((widget == NULL) || (selection_data == NULL)) {
      Debug("CopyPasteSelectionReceivedCB: Error, widget or selection_data is invalid\n");
      goto exit;
   }

   if (selection_data->length < 0) {
      Debug("CopyPasteSelectionReceivedCB: Error, length less than 0\n");
      goto exit;
   }

   /* Try to get clipboard or selection timestamp. */
   if (selection_data->target == GDK_SELECTION_TYPE_TIMESTAMP) {
      if (selection_data->selection == GDK_SELECTION_PRIMARY) {
         if (selection_data->length == 4) {
            gGuestSelPrimaryTime = *(uint32 *)selection_data->data;
            Debug("CopyPasteSelectionReceivedCB: Got pri time [%"FMT64"u]\n",
                  gGuestSelPrimaryTime);
         } else if (selection_data->length == 8) {
            gGuestSelPrimaryTime = *(uint64 *)selection_data->data;
            Debug("CopyPasteSelectionReceivedCB: Got pri time [%"FMT64"u]\n",
                  gGuestSelPrimaryTime);
         } else {
            Debug("CopyPasteSelectionReceivedCB: Unknown pri time. Size %d\n",
                  selection_data->length);
         }
      }
      if (selection_data->selection == GDK_SELECTION_CLIPBOARD) {
         if (selection_data->length == 4) {
            gGuestSelClipboardTime = *(uint32 *)selection_data->data;
            Debug("CopyPasteSelectionReceivedCB: Got clip time [%"FMT64"u]\n",
                  gGuestSelClipboardTime);
         } else if (selection_data->length == 8) {
            gGuestSelClipboardTime = *(uint64 *)selection_data->data;
            Debug("CopyPasteSelectionReceivedCB: Got clip time [%"FMT64"u]\n",
                  gGuestSelClipboardTime);
         } else {
            Debug("CopyPasteSelectionReceivedCB: Unknown clip time. Size %d\n",
                  selection_data->length);
         }
      }
      goto exit;
   }

   if (selection_data->selection == GDK_SELECTION_PRIMARY) {
      target = gGuestSelPrimaryBuf;
   } else if (selection_data->selection == GDK_SELECTION_CLIPBOARD) {
      target = gGuestSelClipboardBuf;
   } else {
      goto exit;
   }

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

   /*
    * If target is GDK_SELECTION_TYPE_STRING, assume encoding is local code
    * set. Convert to utf8 before send to vmx.
    */
   if (selection_data->target == GDK_SELECTION_TYPE_STRING &&
       !CodeSet_CurrentToUtf8(selection_data->data,
                              selection_data->length,
                              &utf8Str,
                              &len)) {
      Debug("CopyPasteSelectionReceivedCB: Couldn't convert to utf8 code set\n");
      gWaitingOnGuestSelection = FALSE;
      return;
   }

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
   if (selection_data->target == GDK_SELECTION_TYPE_STRING) {
      free(utf8Str);
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
   const char *begin = NULL;
   const char *end = NULL;
   const char *next = NULL;
   const char *pre = NULL;
   const char *post = NULL;
   size_t preLen = 0;
   size_t postLen = 0;
   size_t len = 0;
   char *text = NULL;
   size_t textLen = 1;
   Bool blockAdded = FALSE;
   Bool gnomeFCP = FALSE;

   if ((widget == NULL) || (selection_data == NULL)) {
      Debug("CopyPasteSelectionGetCB: Error, widget or selection_data is invalid\n");
      return;
   }

   /* If it is text copy paste, return gHostClipboardBuf. */
   if (GDK_SELECTION_TYPE_STRING == selection_data->target ||
       GDK_SELECTION_TYPE_UTF8_STRING == selection_data->target) {
      char *outBuf = gHostClipboardBuf;
      size_t len = strlen(gHostClipboardBuf);

      /*
       * If target is GDK_SELECTION_TYPE_STRING, assume encoding is local code
       * set. Convert from utf8 to local one.
       */
      if (GDK_SELECTION_TYPE_STRING == selection_data->target &&
          !CodeSet_Utf8ToCurrent(gHostClipboardBuf,
                                 strlen(gHostClipboardBuf),
                                 &outBuf,
                                 &len)) {
         Debug("CopyPasteSelectionGetCB: can not convert to current codeset\n");
         return;
      }

      gtk_selection_data_set(selection_data, selection_data->target, 8,
                             outBuf, len);
      Debug("CopyPasteSelectionGetCB: Set text [%s]\n", outBuf);

      if (GDK_SELECTION_TYPE_STRING == selection_data->target) {
         free(outBuf);
      }

      return;
   }
   
   if (selection_data->target != gFCPAtom[FCP_TARGET_INFO_URI_LIST] &&
       selection_data->target != gFCPAtom[FCP_TARGET_INFO_GNOME_COPIED_FILES]) {
      Debug("CopyPasteSelectionGetCB: Got unknown target\n");
      return;
   }

   if (!gHGIsClipboardFCP) {
      Debug("CopyPasteSelectionGetCB: no file list available\n");
      return;
   }

   if (gHGFCPFileTransferStatus == FCP_FILE_TRANSFER_NOT_YET) {
      if (GetAvailableDiskSpace(gFileRoot) < gHGFCPTotalSize) {
         Debug("CopyPasteSelectionGetCB no enough space to copy file from host.\n");
         return;
      }
      /* Send host a rpc to start file transfer. */
      if (!GuestApp_RpcSendOneCPName("copypaste.hg.copy.files", ' ',
                                     gFileRoot, gFileRootSize)) {
         Debug("CopyPasteSelectionGetCB: failed sending copypaste.hg.copy.files "
               "with CPName");
         return;
      }
      gHGFCPFileTransferStatus = FCP_FILE_TRANSFERRING;
   }

   if (gBlockFd > 0) {
      /* Add a block on the staging directory for this command. */
      if (DnD_AddBlock(gBlockFd, gFileRoot)) {
         Debug("CopyPasteSelectionGetCB: add block [%s].\n", gFileRoot);
         blockAdded = TRUE;
      } else {
         Warning("CopyPasteSelectionGetCB: Unable to add block [%s].\n", gFileRoot);
      }
   }
   
   if (!blockAdded) {
      /*
       * If there is no blocking driver, wait here till file copy is done.
       * 2 reasons to keep this:
       * 1. If run vmware-user stand-alone as non-root, blocking driver can not
       *    be opened. Debug purpose only.
       * 2. Other platforms (Solaris, FreeBSD, etc) may also use this code, and there
       *    is no blocking driver yet.
       */
      Debug("CopyPasteSelectionGetCB no blocking driver, waiting for "
            "HG file copy done ...\n");
      while (gHGFCPFileTransferStatus != FCP_FILE_TRANSFERRED) {
         struct timeval tv;
         int nr;

         tv.tv_sec = 0;
         nr = EventManager_ProcessNext(gEventQueue, (uint64 *)&tv.tv_usec);
         if (nr != 1) {
            Debug("CopyPasteSelectionGetCB unexpected end of loop: returned "
                  "value is %d.\n", nr);
            return;
         }
         if (select(0, NULL, NULL, NULL, &tv) == -1) {
            Debug("CopyPasteSelectionGetCB error in select (%s).\n", 
                  strerror(errno));
            return;         
         }
      }

      Debug("CopyPasteSelectionGetCB file transfer done!\n");
   }

   /* Setup the format string components */
   if (selection_data->target == gFCPAtom[FCP_TARGET_INFO_URI_LIST]) {
      Debug("CopyPasteSelectionGetCB Got uri_list request!\n");
      pre = DND_URI_LIST_PRE_KDE;
      preLen = sizeof DND_URI_LIST_PRE_KDE - 1;
      post = DND_URI_LIST_POST;
      postLen = sizeof DND_URI_LIST_POST - 1;
   }
   if (selection_data->target == gFCPAtom[FCP_TARGET_INFO_GNOME_COPIED_FILES]) {
      Debug("CopyPasteSelectionGetCB Got gnome_copied request!\n");
      pre = FCP_GNOME_LIST_PRE;
      preLen = sizeof FCP_GNOME_LIST_PRE - 1;
      post = FCP_GNOME_LIST_POST;
      postLen = sizeof FCP_GNOME_LIST_POST - 1;
      gnomeFCP = TRUE;

      textLen += 5;
      text = Util_SafeRealloc(text, textLen);
      Str_Snprintf(text, 6, "copy\n");
   }

   if (!pre) {
      Debug("CopyPasteSelectionGetCB: invalid drag target info\n");
      return;
   }


   /*
    * Set begin to first non-NUL character and end to last NUL character to
    * prevent errors in calling CPName_GetComponentGeneric().
    */
   for(begin = gHostClipboardBuf; *begin == '\0'; begin++)
      ;
   end = CPNameUtil_Strrchr(gHostClipboardBuf, gGHFCPListSize + 1, '\0');
   ASSERT(end);

   /* Build up selection data */
   while ((len = CPName_GetComponentGeneric(begin, end, "", &next)) != 0) {
      const size_t origTextLen = textLen;

      if (len < 0) {
         Debug("CopyPasteSelectionGetCB: error getting next component\n");
         if (text) {
            free(text);
         }
         return;
      }

      /*
       * Append component.  NUL terminator was accounted for by initializing
       * textLen to one above.
       */
      textLen += preLen + len + postLen;
      text = Util_SafeRealloc(text, textLen);

      /* 
       * Bug 143147: Gnome FCP does not like the trailing newlines. We don't
       * have this problem for targets that ask for URI lists. So we don't see
       * this problem on:
       * - KDE which asks for URI lists during FCP
       * - DnD in both Gnome and KDE since they ask for URI lists.
       *
       * This is a problem only for Gnome FCP which expects a specially
       * formatted 'copy' command string containing the file list which it then
       * converts into a URI list internally.
       */
      Str_Snprintf(text + origTextLen - 1,
                   textLen - origTextLen + 1,
                   "%s%s%s", pre, begin, gnomeFCP && next == end ? "" : post);

      /* Iterate to next component */
      begin = next;
   }

   /*
    * Send out the data using the selection system. When sending a string, GTK will
    * ensure that a null terminating byte is added to the end so we do not need to
    * add it. GTK also copies the data so the original will never be modified.
    */
   Debug("CopyPasteSelectionGetCB: set file list [%s]\n", text);
   gtk_selection_data_set(selection_data, selection_data->target,
                          8, /* 8 bits per character. */
                          text, textLen);
   free(text);
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
   Debug("CopyPasteSelectionClearCB got clear signal\n");
   gIsOwner = FALSE;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteSetBackdoorSelections --
 *
 *      Set the clipboard one of two ways, the old way or the new way.
 *      The old way uses GuestApp_SetSel and there's only one selection.
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

   if (primaryLen) {
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
      GuestApp_SetSelLength(0);
      Debug("CopyPasteSetBackdoorSelections Set empty text.\n");
   } else {
      len = strlen((char *)p);
      Debug("CopyPasteSetBackdoorSelections Set text [%s].\n", (char *)p);
      aligned_len = (len + 4) & ~3;

      /* Here long string should already be truncated. */
      ASSERT(aligned_len <= MAX_SELECTION_BUFFER_LENGTH);

      GuestApp_SetSelLength(len);
      for (i = 0; i < len; i += 4, p++) {
         GuestApp_SetNextPiece(*p);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_GetBackdoorSelections --
 *
 *      Get the clipboard "the old way".
 *      The old way uses GuestApp_SetSel and there's only one selection.
 *      We don't have to do anything for the "new way", since the host
 *      will just push PRIMARY and/or CLIPBOARD when they are available
 *      on the host.
 *
 *      XXX: the "new way" isn't availble yet because the vmx doesn't
 *           implement separate clipboards. Even when it does this
 *           function will still exist for backward compatibility
 *
 * Results:
 *      TRUE if selection length>=0, FLASE otherwise.
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
   int iAtom;

   if (gVmxCopyPasteVersion > 1) {
      return TRUE;
   }

   selLength = GuestApp_GetHostSelectionLen();
   if (selLength < 0) {
      return FALSE;
   } else if (selLength > 0) {
      memset(gHostClipboardBuf, 0, sizeof (gHostClipboardBuf));
      GuestApp_GetHostSelection(selLength, gHostClipboardBuf);
      Debug("CopyPaste_GetBackdoorSelections Get text [%s].\n", gHostClipboardBuf);
      gtk_selection_owner_set(gUserMainWidget,
                              GDK_SELECTION_CLIPBOARD,
                              GDK_CURRENT_TIME);
      gtk_selection_owner_set(gUserMainWidget,
                              GDK_SELECTION_PRIMARY,
                              GDK_CURRENT_TIME);
      gIsOwner = TRUE;
      gHGIsClipboardFCP = FALSE;
      for (iAtom = 0; iAtom < NR_FCP_TARGETS; iAtom++) {
         CopyPasteSelectionRemoveTarget(gUserMainWidget,
                                        GDK_SELECTION_PRIMARY,
                                        gFCPAtom[iAtom]);
         CopyPasteSelectionRemoveTarget(gUserMainWidget,
                                        GDK_SELECTION_CLIPBOARD,
                                        gFCPAtom[iAtom]);
      }
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteRpcInGHSetDataCB --
 *
 *    Handler function for the "copypaste.gh.data.get" RPC command. Host is
 *    asking for clipboard contents for guest->host copy/paste. If both primary
 *    selection and clipboard are empty, the empty list should also be sent back
 *    because Host should release clipboard owner.
 *
 *    For Guest->Host copy/paste operations only.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    The owner of the clipboard will get requests from our application.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CopyPasteRpcInGHSetDataCB(char const **result,  // OUT
                          size_t *resultLen,    // OUT
                          const char *name,     // IN
                          const char *args,     // IN
                          size_t argsSize,      // Ignored
                          void *clientData)     // Ignored
{
   int iAtom;
   GdkAtom activeSelection = GDK_SELECTION_PRIMARY;
   char *source = gGuestSelPrimaryBuf;
   char format[256];
   char *rpcBody = NULL;
   size_t rpcBodySize = 0;

   gGuestSelPrimaryBuf[0] = '\0';
   gGuestSelClipboardBuf[0] = '\0';

   if (gIsOwner) {
      Debug("CopyPasteRpcInGHSetDataCB Send empty buf to host\n");
      return RpcIn_SetRetVals(result, resultLen, "", TRUE);
   }

   /* First check which one is newer, primary selection or clipboard. */
   gWaitingOnGuestSelection = TRUE;
   gtk_selection_convert(gUserMainWidget,
                         GDK_SELECTION_PRIMARY,
                         GDK_SELECTION_TYPE_TIMESTAMP,
                         GDK_CURRENT_TIME);
   while (gWaitingOnGuestSelection) gtk_main_iteration();

   gWaitingOnGuestSelection = TRUE;
   gtk_selection_convert(gUserMainWidget,
                         GDK_SELECTION_CLIPBOARD,
                         GDK_SELECTION_TYPE_TIMESTAMP,
                         GDK_CURRENT_TIME);
   while (gWaitingOnGuestSelection) gtk_main_iteration();

   if (gGuestSelPrimaryTime < gGuestSelClipboardTime) {
      activeSelection = GDK_SELECTION_CLIPBOARD;
      source = gGuestSelClipboardBuf;
   }

   /* Check if it is file list in the active selection. */
   for (iAtom = 0; iAtom < NR_FCP_TARGETS; iAtom++) {
      gWaitingOnGuestSelection = TRUE;
      gtk_selection_convert(gUserMainWidget,
                            activeSelection,
                            gFCPAtom[iAtom],
                            GDK_CURRENT_TIME);
      while (gWaitingOnGuestSelection) gtk_main_iteration();
      if (source[0] != '\0') {
         if (gVmxCopyPasteVersion < 2) {
            /* Only vmx version greater than 2 support file copy/paste. */
            Debug("CopyPasteRpcInGHSetDataCB invalid operation\n");
            return RpcIn_SetRetVals(result, resultLen,
                                    "invalid operation", FALSE);
         }
         break;
      }
   }

   if (source[0] != '\0') {
      char *currName;
      size_t currSize;
      size_t index = 0;
      char *ghFileList = NULL;
      size_t ghFileListSize = 0;

      /*
       * In gnome, before file list there may be a extra line indicating it
       * is a copy or cut.
       */
      if (strncmp(source, "copy", 4) == 0) {
         source += 4;
      }
      if (strncmp(source, "cut", 3) == 0) {
         source += 3;
      }

      while (*source == '\n' || *source == '\r' || *source == ' ') {
         source++;
      }

      /*
       * Get the the full filenames and last components from the URI list.  The
       * body of the RPC message will be these last components delimited with
       * NUL characters; the Guest->Host file list will be the full paths
       * delimited by NUL characters.
       */
      while ((currName = DnD_UriListGetNextFile(source,
                                                &index,
                                                &currSize))) {
         size_t lastComponentSize;
         char *lastComponentStart;

         /* Append current filename to Guest->Host list */
         ghFileList = Util_SafeRealloc(ghFileList,
                                       ghFileListSize + currSize + 1);
         memcpy(ghFileList + ghFileListSize, currName, currSize);
         ghFileListSize += currSize;
         ghFileList[ghFileListSize] = '\0';
         ghFileListSize++;

         /* Append last component to RPC body */
         lastComponentStart = CPNameUtil_Strrchr(currName, currSize, DIRSEPC);
         if (!lastComponentStart) {
            /*
             * This shouldn't happen since filenames are absolute, but handle
             * it as if the file name is the last component
             */
            lastComponentStart = currName;
         } else {
            /* Skip the last directory separator */
            lastComponentStart++;
         }

         lastComponentSize = currName + currSize - lastComponentStart;
         rpcBody = Util_SafeRealloc(rpcBody, rpcBodySize + lastComponentSize + 1);
         memcpy(rpcBody + rpcBodySize, lastComponentStart, lastComponentSize);
         rpcBodySize += lastComponentSize;
         rpcBody[rpcBodySize] = '\0';
         rpcBodySize++;

         free(currName);
      }

      if (!ghFileList || !rpcBody) {
         Warning("CopyPasteRpcInGHSetDataCB: no filenames retrieved "
                 "from URI list\n");
         free(ghFileList);
         free(rpcBody);
         return RpcIn_SetRetVals(result, resultLen,
                                 "error retrieving file name", FALSE);
      }

      /* Set the list of full paths */
      CopyPasteGHFileListSet(ghFileList, ghFileListSize);

      /* rpcBody (and its size) will always contain a trailing NUL character */
      rpcBodySize--;
      Debug("CopyPasteRpcInGHSetDataCB: Sending: [%s] (%zu)\n",
            CPName_Print(rpcBody, rpcBodySize), rpcBodySize);

      Str_Sprintf(format, sizeof format, "%d ", CPFORMAT_FILELIST);
      *resultLen = rpcBodySize + strlen(format);

      free(gGHFCPRpcResultBuffer);
      gGHFCPRpcResultBuffer = Util_SafeCalloc(1, rpcBodySize + strlen(format));

      memcpy(gGHFCPRpcResultBuffer, format, strlen(format));
      memcpy(gGHFCPRpcResultBuffer + strlen(format), rpcBody, rpcBodySize);
      free(rpcBody);
      *result = gGHFCPRpcResultBuffer;
      return TRUE;
   } else {
      /* Try to get utf8 text from active selection. */
      gWaitingOnGuestSelection = TRUE;
      gtk_selection_convert(gUserMainWidget,
                            activeSelection,
                            GDK_SELECTION_TYPE_UTF8_STRING,
                            GDK_CURRENT_TIME);
      while (gWaitingOnGuestSelection) gtk_main_iteration();

      if (source[0] == '\0') {
         /* Try to get text from active selection. */
         gWaitingOnGuestSelection = TRUE;
         gtk_selection_convert(gUserMainWidget,
                              activeSelection,
                              GDK_SELECTION_TYPE_STRING,
                              GDK_CURRENT_TIME);
         while (gWaitingOnGuestSelection) gtk_main_iteration();
      }

      if (source[0] != '\0') {
         free(gGHFCPRpcResultBuffer);

         gGHFCPRpcResultBuffer =
            Str_Asprintf(NULL, "%d %s", CPFORMAT_TEXT, source);

         if (!gGHFCPRpcResultBuffer) {
            Debug("CopyPasteRpcInGHSetDataCB failed to alloc memory.\n");
            return RpcIn_SetRetVals(result, resultLen,
                                    "error allocating memory", FALSE);
         }

         *result = gGHFCPRpcResultBuffer;
         *resultLen = strlen(gGHFCPRpcResultBuffer);

         Debug("CopyPasteRpcInGHSetDataCB creating text: %s\n", source);

         return TRUE;
      }
      /* Neither file list nor text is available, send empty list back. */
      Debug("CopyPasteRpcInGHSetDataCB Send empty buf to host\n");
      return RpcIn_SetRetVals(result, resultLen, "", TRUE);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPasteRpcInGHFinishCB --
 *
 *    For Guest->Host operations only.
 *
 *    Invoked when host side of copyPaste operation has finished.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
CopyPasteRpcInGHFinishCB(char const **result,      // OUT
                         size_t *resultLen,        // OUT
                         const char *name,         // IN
                         const char *args,         // IN
                         size_t argsSize,          // Ignored
                         void *clientData)         // IN
{
   char *effect = NULL;
   unsigned int index = 0;

   gFcpGHState.fileListNext = gFcpGHState.fileList;

   effect = StrUtil_GetNextToken(&index, args, " ");
   if (!effect) {
      Warning("CopyPasteRpcInGHFinishCB: no drop effect provided\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "drop effect not provided", FALSE);
   }

   Debug("CopyPasteRpcInGHFinishCB got effect %s\n", effect);

   free(effect);
   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPasteGHFileListClear --
 *
 *    Clears existing Guest->Host file list, releasing any used resources.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
CopyPasteGHFileListClear(void)
{
   Debug("CopyPasteGHFileListClear: clearing G->H file list\n");
   if (gFcpGHState.fileList) {
      free(gFcpGHState.fileList);
      gFcpGHState.fileList = NULL;
   }
   gFcpGHState.fileListSize = 0;
   gFcpGHState.fileListNext = NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPasteGHFileListSet --
 *
 *    Sets the Guest->Host file list that is accessed through
 *    CopyPasteGHFileListGetNext().
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Clears the existing Guest->Host file list if it exists.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
CopyPasteGHFileListSet(char *fileList,      // IN: new Guest->Host file list
                       size_t fileListSize) // IN: size of the provided list
{
   CopyPasteGHFileListClear();
   gFcpGHState.fileList = fileList;
   gFcpGHState.fileListSize = fileListSize;
   gFcpGHState.fileListNext = fileList;

   Debug("CopyPasteGHFileListSet: [%s] (%"FMTSZ"u)\n",
         CPName_Print(gFcpGHState.fileList, gFcpGHState.fileListSize),
         gFcpGHState.fileListSize);
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPasteGHFileListGetNext --
 *
 *    Retrieves the next file in the Guest->Host file list.
 *
 *    Note that this function may only be called after calling
 *    CopyPasteGHFileListSet() and before calling CopyPasteGHFileListClear().
 *
 * Results:
 *    TRUE on success, FALSE on failure.  If TRUE is returned, fileName is
 *    given a pointer to the filename's location or NULL if there are no more
 *    files, and fileNameSize is given the length of fileName.
 *
 * Side effects:
 *    The fileListNext value of the Guest->Host global state is updated.
 *
 *----------------------------------------------------------------------------
 */

Bool
CopyPasteGHFileListGetNext(char **fileName,       // OUT: fill with filename location
                           size_t *fileNameSize)  // OUT: fill with filename length
{
   char const *end;
   char const *next;
   int len;

   ASSERT(gFcpGHState.fileList);
   ASSERT(gFcpGHState.fileListNext);
   ASSERT(gFcpGHState.fileListSize > 0);

   /* Ensure end is the last NUL character */
   end = CPNameUtil_Strrchr(gFcpGHState.fileList,
                            gFcpGHState.fileListSize,
                            '\0');
   ASSERT(end);

   /* Get the length of this filename and a pointer to the next one */
   len = CPName_GetComponentGeneric(gFcpGHState.fileListNext, end, "", &next);
   if (len < 0) {
      Warning("CopyPasteGHFileListGetNext: error retrieving next component\n");
      return FALSE;
   }

   /* No more entries in the list */
   if (len == 0) {
      Debug("CopyPasteGHFileListGetNext: no more entries\n");
      *fileName = NULL;
      *fileNameSize = 0;
      gFcpGHState.fileListNext = gFcpGHState.fileList;
      return TRUE;
   }

   Debug("CopyPasteGHFileListGetNext: returning [%s] (%d)\n",
         gFcpGHState.fileListNext, len);

   *fileName = gFcpGHState.fileListNext;
   *fileNameSize = len;
   gFcpGHState.fileListNext = (char *)next;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteHGSetData --
 *
 *    Host is sending text for copy/paste.
 *
 *    RPC command format:
 *    1. Format
 *    2. Size of text
 *    3. If text size > 0, then followed by text, otherwise nothing
 *
 *    For Host->Guest operations only.
 *
 * Results:
 *    TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CopyPasteHGSetData(char const **result,     // OUT
                   size_t *resultLen,       // OUT
                   const char *args)        // IN
{
   char *format = NULL;
   char *sSize = NULL;
   uint32 textSize;
   unsigned int index = 0;
   Bool ret = FALSE;
   int iAtom;

   /* Parse value string. */
   format = StrUtil_GetNextToken(&index, args, " ");
   index++; /* Ignore leading space before data. */
   sSize = StrUtil_GetNextToken(&index, args, " ");
   index++;
   if (!format || !sSize) {
      Debug("CopyPasteHGSetData failed to parse format & size\n");
      ret = RpcIn_SetRetVals(result, resultLen,
                             "format and size is not completed", FALSE);
      goto exit;
   }

   textSize = atoi(sSize);
   gHostClipboardBuf[0] = '\0';

   if (textSize > 0) {
      if (textSize >= MAX_SELECTION_BUFFER_LENGTH) {
         textSize = MAX_SELECTION_BUFFER_LENGTH - 1;
      }
      memcpy(gHostClipboardBuf, args + index, textSize);
      gHostClipboardBuf[textSize] = '\0';
      Debug("CopyPasteHGSetData: Set text [%s]\n", gHostClipboardBuf);
   }
      
   gtk_selection_owner_set(gUserMainWidget,
                           GDK_SELECTION_CLIPBOARD,
                           GDK_CURRENT_TIME);
   gtk_selection_owner_set(gUserMainWidget,
                           GDK_SELECTION_PRIMARY,
                           GDK_CURRENT_TIME);
   gIsOwner = TRUE;
   gHGIsClipboardFCP = FALSE;

   /* We put text into selection, so remove file target types from list. */
   for (iAtom = 0; iAtom < NR_FCP_TARGETS; iAtom++) {
      CopyPasteSelectionRemoveTarget(gUserMainWidget,
                                     GDK_SELECTION_PRIMARY,
                                     gFCPAtom[iAtom]);
      CopyPasteSelectionRemoveTarget(gUserMainWidget,
                                     GDK_SELECTION_CLIPBOARD,
                                     gFCPAtom[iAtom]);
   }

   ret = RpcIn_SetRetVals(result, resultLen, "", TRUE);

exit:
   free(format);
   free(sSize);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteRpcInHGDataFinishCB --
 *
 *       For Host->Guest operations only.
 *       Host has finished transferring copyPaste data to the guest. We do any
 *       post H->G operation cleanup here, like picking a new file root.
 *
 * Results:
 *       TRUE on success, FALSE otherwise
 *
 * Side effects:
 *       Copied files will be deleted in error or cancel case.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CopyPasteRpcInHGDataFinishCB(char const **result,   // OUT
                             size_t *resultLen,     // OUT
                             const char *name,      // IN
                             const char *args,      // IN
                             size_t argsSize,       // Ignored
                             void *clientData)      // IN: pointer to mainWnd
{
   unsigned int index = 0;
   char *state;

   Debug("CopyPasteRpcInHGDataFinishCB received copypaste data finish\n");

   state = StrUtil_GetNextToken(&index, args, " ");

   if (!state) {
      Debug("CopyPasteRpcInHGDataFinishCB failed to parse data state\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "must specify data finish state", FALSE);
   }

   if (strcmp(state, "success") != 0) {
      Debug("CopyPasteRpcInHGDataFinishCB data transfer error\n");
      /*
       * Delete staging directory in error or cancel case, otherwise
       * target application may still try to get copied files because
       * the file list is provided right after adding block to staging
       * directory.
       */
      File_DeleteDirectoryTree(gFileRoot);
   }

   free(state);

   ASSERT(gHGFCPFileTransferStatus == FCP_FILE_TRANSFERRING);
   gHGFCPFileTransferStatus = FCP_FILE_TRANSFERRED;

   if (gBlockFd > 0 && !DnD_RemoveBlock(gBlockFd, gFileRoot)) {
      Warning("CopyPasteRpcInHGDataFinishCB: Unable to remove block [%s].\n",
              gFileRoot);
   }

   /* get new root dir for next FCP operation. */
   gFileRootSize = DnD_GetNewFileRoot(gFileRoot, sizeof gFileRoot);

   Debug("CopyPasteRpcInHGDataFinishCB create staging dir [%s]\n", gFileRoot);

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteHGSetFileList --
 *
 *    Host is sending file list for FCP (file copy/paste).
 *
 *    RPC command format:
 *    1. Format
 *    2. Total size of all files in the list
 *    3. Size of file list string
 *    4. If list size > 0, then followed by file list, otherwise nothing
 *
 *    For Host->Guest FCP operations only.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CopyPasteHGSetFileList(char const **result,     // OUT
                       size_t *resultLen,       // OUT
                       const char *args)        // IN
{
   char *format = NULL;
   char *data = NULL;
   char *sListSize = NULL;
   size_t listSize;
   char *sTotalSize = NULL;
   char *stagingDirName = NULL;
   char mountDirName[DND_MAX_PATH];
   unsigned int index = 0;
   Bool ret = FALSE;
   char *retStr;
   int iAtom;

   gHGFCPFileTransferStatus = FCP_FILE_TRANSFER_NOT_YET;
   /* Parse value string. */
   format = StrUtil_GetNextToken(&index, args, " ");
   index++; /* Ignore leading space before data. */
   sTotalSize = StrUtil_GetNextToken(&index, args, " ");
   index++;
   sListSize = StrUtil_GetNextToken(&index, args, " ");
   index++;
   if (!format || !sTotalSize || !sListSize) {
      Debug("CopyPasteHGSetFileList failed to parse format & size\n");
      retStr = "format or size is not completed";
      goto exit;
   }

   listSize = atoi(sListSize);
   /* 
    * Total file size in selection list. This is used to check if there is enough
    * space in guest OS for host->guest file transfer.
    */
   gHGFCPTotalSize = atol(sTotalSize);

   if (listSize <= 0) {
      Debug("CopyPasteHGSetFileList: got empty list\n");
      gHostClipboardBuf[0] = '\0';
      retStr = "";
      ret = TRUE;
      goto exit;
   }

   /*
    * XXX Should do code set convertion here from utf8 to current for file list,
    * but right now should not do that. The reason is that the hgfs server
    * always puts utf8 file name into guest, which is not right if local guest
    * encoding is non-utf8. DnD has same problem.
    */

   data = (char *)Util_SafeCalloc(1, listSize + 1);
   memcpy(data, args + index, listSize);
   data[listSize] = '\0';

   /*
    * This data could have come from either a Windows or Linux host.
    * Therefore, we need to verify that it doesn't contain any illegal
    * characters for the current platform.
    */
   if (DnD_DataContainsIllegalCharacters(data, listSize)) {
      Debug("CopyPasteHGSetFileList: data contains illegal characters\n");
      retStr = DND_ILLEGAL_CHARACTERS;
      goto exit;
   }

   if (gBlockFd > 0) {
      /*
       * Here we take the last component of the actual file root, which is
       * a temporary directory for this DnD operation, and append it to the
       * mount point for vmblock.  This is where we want the target application
       * to access the file since it will enable vmblock to block that
       * application's progress if necessary.
       */
      stagingDirName = DnD_GetLastDirName(gFileRoot);
      if (!stagingDirName) {
         Debug("CopyPasteHGSetFileList: error construct stagingDirName\n");
         retStr = "error construct stagingDirName";
         goto exit;
      }
      if (sizeof VMBLOCK_MOUNT_POINT - 1 +
          (sizeof DIRSEPS - 1) * 2 + strlen(stagingDirName) >= sizeof mountDirName) {
         Debug("CopyPasteHGSetFileList: directory name too large.\n");
         retStr = "directory name too large";
         goto exit;
      }
      Str_Sprintf(mountDirName, sizeof mountDirName,
                  VMBLOCK_MOUNT_POINT DIRSEPS"%s"DIRSEPS, stagingDirName);
   }

   /* Add the file root to the relative paths received from host */
   if (!DnD_PrependFileRoot(gBlockFd > 0 ? mountDirName : gFileRoot,
                            &data, &listSize)) {
      Debug("CopyPasteHGSetFileList: error prepending guest file root\n");
      retStr = "error prepending file root";
      goto exit;
   }

   if (listSize + 1 > sizeof gHostClipboardBuf) {
      Debug("CopyPasteHGSetFileList: data too large\n");
      retStr = "data too large";
      goto exit;
   }

   memcpy(gHostClipboardBuf, data, listSize + 1);
   gGHFCPListSize = listSize;
   gHGIsClipboardFCP = TRUE;
   Debug("CopyPasteHGSetFileList: get file list [%s] (%zu)\n",
         CPName_Print(gHostClipboardBuf, gGHFCPListSize), gGHFCPListSize);

   for (iAtom = 0; iAtom < NR_FCP_TARGETS; iAtom++) {
      gtk_selection_add_target(gUserMainWidget, GDK_SELECTION_PRIMARY,
                               gFCPAtom[iAtom], 0);
      gtk_selection_add_target(gUserMainWidget, GDK_SELECTION_CLIPBOARD,
                               gFCPAtom[iAtom], 0);
   }

   Debug("CopyPasteHGSetFileList: added targets\n");
   gtk_selection_owner_set(gUserMainWidget,
                           GDK_SELECTION_CLIPBOARD,
                           GDK_CURRENT_TIME);
   gtk_selection_owner_set(gUserMainWidget,
                           GDK_SELECTION_PRIMARY,
                           GDK_CURRENT_TIME);
   gIsOwner = TRUE;

   retStr = "";
   ret = TRUE;

exit:
   free(format);
   free(data);
   free(sTotalSize);
   free(sListSize);
   free(stagingDirName);
   return RpcIn_SetRetVals(result, resultLen, retStr, ret);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteRpcInHGSetDataCB --
 *
 *    Host is sending data for copy/paste. The data can be text, file list, etc.
 *
 *    For Host->Guest operations only.
 *
 * Results:
 *    TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CopyPasteRpcInHGSetDataCB(char const **result,  // OUT
                          size_t *resultLen,    // OUT
                          const char *name,     // IN
                          const char *args,     // IN
                          size_t argsSize,      // Ignored
                          void *clientData)     // IN: ignored
{
   char *formatStr = NULL;
   DND_CPFORMAT format;
   Bool ret = FALSE;

   unsigned int index = 0;

   if (gHGFCPFileTransferStatus == FCP_FILE_TRANSFERRING) {
      return RpcIn_SetRetVals(result, resultLen,
                              "", TRUE);
   }

   /* Parse value string. */
   formatStr = StrUtil_GetNextToken(&index, args, " ");
   index++; /* Ignore leading space before data. */

   if (!formatStr) {
      Debug("CopyPasteTcloHGDataSet failed to parse format\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "format and size is not completed", FALSE);
   }

   format = (DND_CPFORMAT)atoi(formatStr);
   free(formatStr);

   switch (format) {
   case CPFORMAT_TEXT:
      ret = CopyPasteHGSetData(result, resultLen, args);
      break;
   case CPFORMAT_FILELIST:
      /* Only vmx version greater than 2 support file copy/paste. */
      if (gVmxCopyPasteVersion < 2) {
         Debug("CopyPasteRpcInHGSetDataCB invalid operation\n");
         return RpcIn_SetRetVals(result, resultLen,
                                 "invalid operation", FALSE);
      }
      ret = CopyPasteHGSetFileList(result, resultLen, args);
      break;
   default:
      Debug("CopyPasteTcloHGDataSet unknown format\n");
      ret = RpcIn_SetRetVals(result, resultLen,
                             "unknown format", FALSE);
      break;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPasteRpcInGHGetNextFileCB --
 *
 *    For Guest->Host operations only.
 *
 *    Invoked when the host is compiling its list of files to copy from the
 *    guest.  Here we provide the path of the next file in our Guest->Host file
 *    list in guest path format (for display purposes) and CPName format (for
 *    file copy operation).
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    Iterator pointer within file list of GH state is iterated to next list
 *    entry (through call to CopyPasteGHFileListGetNext()).
 *
 *----------------------------------------------------------------------------
 */

static Bool
CopyPasteRpcInGHGetNextFileCB(char const **result,      // OUT
                              size_t *resultLen,        // OUT
                              const char *name,         // IN
                              const char *args,         // IN
                              size_t argsSize,          // Ignored
                              void *clientData)         // IN
{
   static char resultBuffer[DND_MAX_PATH];
   char *fileName;
   size_t fileNameSize;
   uint32 cpNameSize;
   Bool res;

   /*
    * Retrieve a pointer to the next filename and its size from the list stored
    * in the G->H DnD state.  Note that fileName should not be free(3)d here
    * since an additional copy is not allocated.
    */
   res = CopyPasteGHFileListGetNext(&fileName, &fileNameSize);

   if (!res) {
      Warning("CopyPasteRpcInGHGetNextFileCB: error retrieving file name\n");
      return RpcIn_SetRetVals(result, resultLen, "error getting file", FALSE);
   }

   if (!fileName) {
      /* There are no more files to send */
      Debug("CopyPasteRpcInGHGetNextFileCB: reached end of Guest->Host file list\n");
      return RpcIn_SetRetVals(result, resultLen, "|end|", TRUE);
   }

   if (fileNameSize + 1 + fileNameSize > sizeof resultBuffer) {
      Warning("CopyPasteRpcInGHGetNextFileCB: filename too large (%"FMTSZ"u)\n", fileNameSize);
      return RpcIn_SetRetVals(result, resultLen, "filename too large", FALSE);
   }

   /*
    * Construct a reply message of the form:
    * <file name in guest format><NUL><filename in CPName format>
    */
   memcpy(resultBuffer, fileName, fileNameSize);
   resultBuffer[fileNameSize] = '\0';

   cpNameSize = CPNameUtil_ConvertToRoot(fileName,
                                         sizeof resultBuffer - (fileNameSize + 1),
                                         resultBuffer + fileNameSize + 1);
   if (cpNameSize < 0) {
      Warning("CopyPasteRpcInGHGetNextFileCB: could not convert to CPName\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "error on CPName conversion", FALSE);
   }

   /* Set manually because RpcIn_SetRetVals() assumes no NUL characters */
   *result = resultBuffer;
   *resultLen = fileNameSize + 1 + cpNameSize;

   Debug("CopyPasteRpcInGHGetNextFileCB: [%s] (%"FMTSZ"u)\n",
         CPName_Print(*result, *resultLen), *resultLen);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_GetVmxCopyPasteVersion --
 *
 *      Ask the vmx for it's copy/paste version.
 *
 * Results:
 *      The copy/paste version the vmx supports, 1 if the vmx doesn't know
 *      what we're talking about.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int32
CopyPaste_GetVmxCopyPasteVersion(void)
{
   char *reply = NULL;
   size_t replyLen;

   if (!RpcOut_sendOne(&reply, &replyLen, "vmx.capability.copypaste_version")) {
      Debug("CopyPaste_GetVmxCopyPasteVersion: could not get VMX copyPaste "
            "version capability: %s\n", reply ? reply : "NULL");
      gVmxCopyPasteVersion = 1;
   } else {
      gVmxCopyPasteVersion = atoi(reply);
   }

   free(reply);
   Debug("CopyPaste_GetVmxCopyPasteVersion: got version %d\n", gVmxCopyPasteVersion);
   return gVmxCopyPasteVersion;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_RegisterCapability --
 *
 *      Register the "copypaste" capability. Sometimes this needs to be done
 *      separately from the rest of copy/paste registration, so we provide it
 *      separately here.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CopyPaste_RegisterCapability(void)
{
   /* Tell the VMX about the copyPaste version we support. */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.copypaste_version 2")) {
      Debug("CopyPaste_RegisterCapability: could not set guest copypaste "
            "version capability\n");
      gVmxCopyPasteVersion = 1;
      return FALSE;
   }
   Debug("CopyPaste_RegisterCapability: set copypaste version 2\n");
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
CopyPaste_Register(GtkWidget* mainWnd)
{
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

   gFCPAtom[FCP_TARGET_INFO_GNOME_COPIED_FILES] = 
      gdk_atom_intern(FCP_TARGET_NAME_GNOME_COPIED_FILES, FALSE);
   gFCPAtom[FCP_TARGET_INFO_URI_LIST] = 
      gdk_atom_intern(FCP_TARGET_NAME_URI_LIST, FALSE);

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

   gtk_signal_connect(GTK_OBJECT(mainWnd), "selection_received",
                      GTK_SIGNAL_FUNC(CopyPasteSelectionReceivedCB), mainWnd);
   gtk_signal_connect(GTK_OBJECT(mainWnd), "selection_get",
                      GTK_SIGNAL_FUNC(CopyPasteSelectionGetCB), mainWnd);
   gtk_signal_connect(GTK_OBJECT(mainWnd), "selection_clear_event",
                      GTK_SIGNAL_FUNC(CopyPasteSelectionClearCB), mainWnd);

   gHostClipboardBuf[0] = '\0';
   gGuestSelPrimaryBuf[0] = '\0';
   gGuestSelClipboardBuf[0] = '\0';
   gIsOwner = FALSE;
   gGHFCPRpcResultBuffer = NULL;
   gHGFCPPending = FALSE;
   gHGFCPFileTransferStatus = FCP_FILE_TRANSFER_NOT_YET;

   RpcIn_RegisterCallback(gRpcIn, "copypaste.hg.data.set",
                          CopyPasteRpcInHGSetDataCB, NULL);
   RpcIn_RegisterCallback(gRpcIn, "copypaste.hg.data.finish",
                          CopyPasteRpcInHGDataFinishCB, NULL);
   RpcIn_RegisterCallback(gRpcIn, "copypaste.gh.data.get",
                          CopyPasteRpcInGHSetDataCB, NULL);
   RpcIn_RegisterCallback(gRpcIn, "copypaste.gh.get.next.file",
                          CopyPasteRpcInGHGetNextFileCB, NULL);
   RpcIn_RegisterCallback(gRpcIn, "copypaste.gh.finish",
                          CopyPasteRpcInGHFinishCB, NULL);

   if (CopyPaste_GetVmxCopyPasteVersion() >= 2) {
      /*
       * Create staging directory for file copy/paste. This is for vmx with version 2
       * or greater.
       */
      gFileRootSize = DnD_GetNewFileRoot(gFileRoot, sizeof gFileRoot);
      Debug("CopyPaste_Register create file root [%s]\n", gFileRoot);
   }
   return CopyPaste_RegisterCapability();
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
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(CopyPasteSelectionReceivedCB),
                                 mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(CopyPasteSelectionGetCB),
                                 mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(CopyPasteSelectionClearCB),
                                 mainWnd);
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPaste_InProgress --
 *
 *    Indicates whether a copy/paste data transfer is currently in progress.
 *
 * Results:
 *    TRUE if in progress, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CopyPaste_InProgress(void)
{
   /* XXX We currently have no way to determine if a G->H FCP is ongoing. */
   return gHGFCPFileTransferStatus == FCP_FILE_TRANSFERRING;
}
