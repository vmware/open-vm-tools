/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * dnd.c --
 *
 *  Handles the guest side of host<->guest DnD operations.
 *
 *  Guest->Host DnD
 *  ---------------
 *
 *  The DnD process within the guest starts when we receive a "dnd.ungrab" RPC
 *  message from the host, which invokes DnDRpcInMouseUngrabCB().  The MKS
 *  sends this RPC when it sees the mouse stray outside of the clip (guest's
 *  viewable area).  RpcInMouseUngrabCB() will determine whether a DnD is
 *  pending by calling DnDDragPending():
 *   o if a DnD is not pending, it replies with a "dnd.notpending" RPC and we
 *     are done,
 *   o if a DnD is pending, we send fake X events to the X server that place
 *     our invisible window at the location of the mouse pointer and generate
 *     mouse movements over the window.
 *
 *  Faking mouse movement over our window causes Gtk to send us a "drag_motion"
 *  signal, which invokes DnDGtkDragMotionCB().  Here we find a common target
 *  (drop type) and request the data from the drag source via
 *  gtk_drag_get_data().
 *
 *  When the data is ready, Gtk signals us with a "data_received" signal.  We
 *  parse the provided data and send the file names to the host with
 *  a "dnd.data.set" RPC.  Then we start the DnD operation with a "dnd.enter"
 *  RPC.  Upon receiving the "dnd.enter", the MKS will allow the ungrab of the
 *  mouse from the guest window and the user will be able to select a location
 *  to drop the files.
 *
 *  (Note that it is important that the guest reply to the "dnd.ungrab" with
 *   either a "dnd.notpending" or a "dnd.enter" in a timely manner, since the
 *   MKS will delay mouse packets until it has received a reply from the
 *   guest.)
 *
 *  When the user drops the files, the host will send us a "dnd.data.get.file"
 *  for each file, which invokes DnDRpcInGetNextFileCB().  On each invocation,
 *  we reply with the next file from the Guest->Host file list (obtained from
 *  DnDGHFileListGetNext()), and "|end|" when there are no more files.  With
 *  this information, the host copies the files from the guest using HGFS.
 *
 *  When the host has finished copying the files, it sends us a "dnd.finish"
 *  RPC, which invokes DnDRpcInFinishCB().  At this point, we fake X events
 *  that cause a mouse button release over our window.
 *
 *  This button release causes Gtk to send us a "drag_drop" signal, which
 *  invokes DnDGtkDragDropCB().  Here we simply clean up our state and
 *  indicate that the drag finished successfully by calling gtk_drag_finish().
 *
 *  If an error occurs at any point, the host sends us a "dnd.finish cancel"
 *  RPC.  We will fake an ESC key press and release to cancel the pending DnD
 *  in the guest.
 *
 *
 *  Host->Guest DnD
 *  ---------------
 *
 *  A host->guest DnD begins with a "dnd.data.set" from the vmx to provide the
 *  list of files being dragged into the guest, then a "dnd.enter" to begin the
 *  DnD operation.  When the "dnd.enter" is received, this process will send
 *  a fake mouse button press and mouse movement on its window, starting the
 *  DnD operation within the guest.  At this point the mouse still has not been
 *  grabbed by the guest and all mouse movements go only to the host.
 *
 *  As part of the normal DnD protocol on the host, the UI in the host will
 *  receive updates on the location of the mouse within its target window.
 *  This location is translated to guest coordinates and sent to us via the
 *  "dnd.move" RPC, at which point we fake additional mouse movements to that
 *  location.  When the user releases the mouse, the host UI is again notified
 *  and sends us a "dnd.drop" RPC.
 *
 *  When the drop occurs, we add a block (via vmblock) on the directory
 *  containing the files to be given to the target application, then fake
 *  a mouse release at the location of the drop.  This will cause the target
 *  application to request the data, which we provide through our
 *  "drag_data_get" handler (DnDGtkDataRequestCB()).  When the application
 *  attempts to access these files it will be blocked by vmblock.
 *
 *  After the drop is sent, the host will send the files to the hgfs server
 *  running inside this process, and will notify us when that transfer is
 *  complete via the "dnd.data.finish" RPC.  If the transfer is successful, we
 *  remove the block to allow the target application to access the files.  If
 *  the transfer is unsuccessful, we remove any partially copied files then
 *  remove the block; this has the effect of failing the DnD operation since
 *  the target cannot access the necessary files.  Once this is done, we
 *  generate a new file root within the staging directory and send that to the
 *  host for the next DnD operation.
 *
 *  Note that we used to fake the mouse release only after the data transfer
 *  completed (and Windows guests still behave that way), but this was changed
 *  since the Linux UI was modified to allow guest interaction while the
 *  progress dialog (for the file transfer) was displayed and updating.  This
 *  caused a lot of instability since the mouse was no longer in a predictable
 *  state when the fake release was sent.  vmblock let us work around this by
 *  changing where the block occurred.
 */


#include <string.h>
#include <stdlib.h>
#include <X11/extensions/XTest.h>       /* for XTest*() */
#include <X11/keysym.h>                 /* for XK_Escape */
#include <X11/Xatom.h>                  /* for XA_WINDOW */

#include "vmwareuserInt.h"
#include "vm_app.h"
#include "vm_assert.h"
#include "vm_basic_defs.h"
#include "eventManager.h"
#include "debug.h"
#include "strutil.h"
#include "str.h"
#include "file.h"
#include "guestApp.h"
#include "cpName.h"
#include "cpNameUtil.h"
#include "dnd.h"
#include "util.h"
#include "hgfsVirtualDir.h"
#include "hgfsServerPolicy.h"
#include "vmblock.h"
#include "escape.h"


#define DND_MAX_PATH                    6144
#define DRAG_TARGET_NAME_URI_LIST       "text/uri-list"
#define DRAG_TARGET_INFO_URI_LIST       0
#define DRAG_TARGET_NAME_TEXT_PLAIN     "text/plain"
#define DRAG_TARGET_INFO_TEXT_PLAIN     1
#define DRAG_TARGET_NAME_STRING         "STRING"
#define DRAG_TARGET_INFO_STRING         2
/*
 * We support all three drag targets from Host->Guest since we can present
 * filenames in any of these forms if an application requests.  However, we
 * only support file drag targets (text/uri-list) from Guest->Host since we
 * can only DnD files across the backdoor.
 */
#define NR_DRAG_TARGETS                 3
#define NR_GH_DRAG_TARGETS              1

#define DROPEFFECT_NONE 0
#define DROPEFFECT_COPY 1
#define DROPEFFECT_MOVE 2
#define DROPEFFECT_LINK 4

/*
 * More friendly names for calling DnDFakeXEvents().  This is really ugly but
 * it allows us to keep all of the X fake event code in one place.
 *
 * Operation | showWidget | buttonEvent | buttonPress | moveWindow | coordsProvided
 * ----------+------------+-------------+-------------+------------+---------------
 * G->H Drag |    Yes     |      No     |     n/a     |    Yes     |       No
 * G->H Drop |     No     |     Yes     |   Release   |    Yes     |       No
 * H->G Drag |    Yes     |     Yes     |    Press    |    Yes     |       No
 * H->G Move |     No     |      No     |     n/a     |     No     |      Yes
 * H->G Drop |     No     |     Yes     |   Release   |     No     |      Yes
 * ----------+------------+-------------+-------------+------------+---------------
 */
#define DnDGHFakeDrag(widget) \
    DnDFakeXEvents(widget, TRUE, FALSE, FALSE, TRUE, FALSE, 0, 0)
#define DnDGHFakeDrop(widget) \
    DnDFakeXEvents(widget, FALSE, TRUE, FALSE, TRUE, FALSE, 0, 0)
#define DnDHGFakeDrag(widget) \
    DnDFakeXEvents(widget, TRUE, TRUE, TRUE, TRUE, FALSE, 0, 0)
#define DnDHGFakeMove(widget, x, y) \
    DnDFakeXEvents(widget, FALSE, FALSE, FALSE, FALSE, TRUE, x, y)
#define DnDHGFakeDrop(widget, x, y) \
    DnDFakeXEvents(widget, FALSE, TRUE, FALSE, FALSE, TRUE, x, y)

#ifdef GTK2
# define GDKATOM_TO_ATOM(gdkAtom) gdk_x11_atom_to_xatom(gdkAtom)
#else
# define GDKATOM_TO_ATOM(gdkAtom) gdkAtom
#endif

/*
 * Forward Declarations
 */
static Bool DnDRpcInEnterCB      (char const **result, size_t *resultLen,
                                  const char *name, const char *args,
                                  size_t argsSize,void *clientData);
static Bool DnDRpcInDataSetCB    (char const **result, size_t *resultLen,
                                  const char *name, const char *args,
                                  size_t argsSize,void *clientData);
static Bool DnDRpcInMoveCB       (char const **result, size_t *resultLen,
                                  const char *name, const char *args,
                                  size_t argsSize,void *clientData);
static Bool DnDRpcInDataFinishCB (char const **result, size_t *resultLen,
                                  const char *name, const char *args,
                                  size_t argsSize,void *clientData);
static Bool DnDRpcInDropCB       (char const **result, size_t *resultLen,
                                  const char *name, const char *args,
                                  size_t argsSize,void *clientData);
static Bool DnDRpcInMouseUngrabCB(char const **result, size_t *resultLen,
                                  const char *name, const char *args,
                                  size_t argsSize,void *clientData);
static Bool DnDRpcInGetNextFileCB(char const **result, size_t *resultLen,
                                  const char *name, const char *args,
                                  size_t argsSize,void *clientData);
static Bool DnDRpcInFinishCB     (char const **result, size_t *resultLen,
                                  const char *name, const char *args,
                                  size_t argsSize,void *clientData);

/*
 * Gtk DnD specific event/signal callbacks.
 */
/* For Host->Guest DnD */
static void DnDGtkBeginCB(GtkWidget *widget, GdkDragContext *dc, gpointer data);
static void DnDGtkEndCB(GtkWidget *widget, GdkDragContext *dc, gpointer data);
static void DnDGtkDataRequestCB(GtkWidget *widget, GdkDragContext *dc,
                                GtkSelectionData *selection_data,
                                guint info, guint time, gpointer data);

/* For Guest->Host DnD */
static gboolean DnDGtkDragMotionCB(GtkWidget *widget, GdkDragContext *dc, gint x,
                                   gint y, guint time, gpointer data);
static void DnDGtkDragDataReceivedCB(GtkWidget *widget, GdkDragContext *dc,
                                     gint x, gint y, GtkSelectionData *dragData,
                                     guint info, guint time, gpointer data);
static gboolean DnDGtkDragDropCB(GtkWidget *widget, GdkDragContext *dc,
                                 gint x, gint y, guint time, gpointer data);

/*
 * Utility
 */
static Bool DnDSendVmxNewFileRoot(char *rpcCmd);
static Bool DnDFakeXEvents(GtkWidget *widget, Bool showWidget,
                           Bool buttonEvent, Bool buttonPress,
                           Bool moveWindow,
                           Bool coordsProvided, int x, int y);
static void DnDSendEscapeKey(GtkWidget *mainWnd);
static INLINE Bool DnDGHDragPending(GtkWidget *widget);
static INLINE Bool DnDGHXdndDragPending(GtkWidget *widget);
static INLINE void DnDGHXdndClearPending(GtkWidget *widget);
static INLINE Bool DnDGHMotifDragPending(GtkWidget *widget);
static INLINE void DnDGHFileListClear(void);
static INLINE void DnDGHFileListSet(char *fileList, size_t fileListSize);
static INLINE Bool DnDGHFileListGetNext(char **fileName, size_t *fileNameSize);
static INLINE void DnDGHStateInit(GtkWidget *widget);
static INLINE void DnDHGStateInit(void);
static INLINE Bool DnDGHCancel(GtkWidget *widget);
static Bool DnDGHXEventTimeout(void *clientData);

/*
 * Globals
 */
struct ghState {
   Bool dragInProgress;
   Bool ungrabReceived;
   char *dndFileList;
   char *dndFileListNext;
   size_t dndFileListSize;
   GdkDragContext *dragContext;
   guint time;
   Event *event;
} gGHState;
static Bool gHGDnDInProgress;
static Bool gDoneDragging;
static Bool gHGDataPending;
static char gDnDData[1024];
static GdkDragContext *gDragCtx;
static GtkTargetEntry gTargetEntry[NR_DRAG_TARGETS];
static GdkAtom gTargetEntryAtom[NR_GH_DRAG_TARGETS];
static char gFileRoot[DND_MAX_PATH];
static size_t gFileRootSize;
static size_t gDnDDataSize;


/*
 * From vmwareuserInt.h
 */
RpcIn     *gRpcIn;
Display   *gXDisplay;
Window     gXRoot;


/*
 * Host->Guest RPC callback implementations
 */

/*
 *-----------------------------------------------------------------------------
 *
 * DnDRpcInEnterCB --
 *
 *       For Host->Guest operations only.
 *       User has dragged something over this guest's MKS window
 *
 * Results:
 *       TRUE on success, FALSE otherwise
 *
 * Side effects:
 *	 Some GdkEvents are generated which will "drag" the mouse. A directory
 *       is created.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
DnDRpcInEnterCB(char const **result,     // OUT
                size_t *resultLen,       // OUT
                const char *name,        // IN
                const char *args,        // IN
                size_t argsSize,         // Ignored
                void *clientData)        // IN
{
   char *numFormats;
   char *pFormat;
   unsigned int index = 0;
   int nFormats;
   int i;

   Debug("Got DnDRpcInEnterCB\n");
   GtkWidget *mainWnd = GTK_WIDGET(clientData);
   if (mainWnd == NULL) {
      return RpcIn_SetRetVals(result, resultLen,
                              "bad clientData passed to callback", FALSE);
   }

   if (gBlockFd < 0) {
      Debug("DnDRpcInEnterCB: cannot allow H->G DnD without vmblock.\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "blocking file system unavailable", FALSE);
   }

   numFormats = StrUtil_GetNextToken(&index, args, " ");
   if (!numFormats) {
      Debug("DnDRpcInEnterCB: Failed to parse numformats\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "must specify number of formats", FALSE);
   }

   /* Skip whitespace character. */
   index++;

   nFormats = atoi(numFormats);
   free(numFormats);

   for (i = 0; i < nFormats; i++) {
      pFormat = StrUtil_GetNextToken(&index, args, ",");

      if (!pFormat) {
         Debug("DnDRpcInEnterCB: Failed to parse format list\n");
         return RpcIn_SetRetVals(result, resultLen,
                                 "Failed to read format list", FALSE);
      } else {
         /*
          * TODO: check that formats are ok for us to handle. For now, this is
          * ok since there should only be a CF_HDROP. But, we really should figure
          * out a much more cross-platform format scheme
          */
         free(pFormat);
      }
   }

   if (!DnDHGFakeDrag(mainWnd)) {
      Debug("DnDRpcInEnterCB: Failed to fake X events\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "failed to fake drag", FALSE);
   }

   RpcIn_SetRetVals(result, resultLen, "", TRUE);
   RpcOut_sendOne(NULL, NULL, "dnd.feedback copy");
   Debug("DnDRpcInEnterCB finished\n");
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDRpcInDataSetCB --
 *
 *       For Host->Guest operations only.
 *       Host is sending data from a DnD operation.
 *
 * Results:
 *       TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *	 None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
DnDRpcInDataSetCB(char const **result,  // OUT
                  size_t *resultLen,    // OUT
                  const char *name,     // IN
                  const char *args,     // IN
                  size_t argsSize,      // IN: Size of args
                  void *clientData)     // Ignored
{
   char blockDir[DND_MAX_PATH];
   char *perDnDDir = NULL;
   char *format;
   char  *data;
   unsigned int index = 0;
   size_t dataSize;
   char *retStr;
   Bool ret = FALSE;

   Debug("DnDRpcInDataSetCB: enter\n");

   if (gBlockFd < 0) {
      Debug("DnDRpcInDataSetCB: blocking file system not available.\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "blocking file system not available", FALSE);
   }

   /* Parse the data type & value string. */
   format = StrUtil_GetNextToken(&index, args, " ");
   if (!format) {
      Debug("DnDRpcInDataSetCB: Failed to parse format\n");
      return RpcIn_SetRetVals(result, resultLen, "need format", FALSE);
   }

   index++; /* Ignore leading space before data. */
   dataSize = argsSize - index;
   data = Util_SafeMalloc(dataSize);
   memcpy(data, args + index, dataSize);

   Debug("DnDRpcInDataSetCB: Received data from host: (%s) [%s] (%"FMTSZ"u)\n",
         format, CPName_Print(data, dataSize), dataSize);

   /*
    * This data could have come from either a Windows or Linux host.
    * Therefore, we need to verify that it doesn't contain any illegal
    * characters for the current platform.
    */
   if (DnD_DataContainsIllegalCharacters(data, dataSize)) {
      Debug("DnDRpcInDataSetCB: data contains illegal characters\n");
      retStr = DND_ILLEGAL_CHARACTERS;
      goto out;
   }

   /*
    * Here we take the last component of the actual file root, which is
    * a temporary directory for this DnD operation, and append it to the mount
    * point for vmblock.  This is where we want the target application to
    * access the file since it will enable vmblock to block that application's
    * progress if necessary.
    */
   DnD_GetLastDirName(gFileRoot, gFileRootSize, &perDnDDir);
   if (!perDnDDir) {
      Debug("DnDRpcInDataSetCB: cannot obtain dirname of root.\n");
      retStr = "error obtaining dirname of root";
      goto out;
   }

   if (sizeof VMBLOCK_MOUNT_POINT - 1 +
       (sizeof DIRSEPS - 1) * 2 + strlen(perDnDDir) >= sizeof blockDir) {
      Debug("DnDRpcInDataSetCB: blocking directory path too large.\n");
      retStr = "blocking directory path too large";
      goto out;
   }

   Str_Sprintf(blockDir, sizeof blockDir,
               VMBLOCK_MOUNT_POINT DIRSEPS "%s" DIRSEPS, perDnDDir);

   /* Add the file root to the relative paths received from host */
   if (!DnD_PrependFileRoot(blockDir, &data, &dataSize)) {
      Debug("DnDRpcInDataSsetCB: error prepending guest file root\n");
      retStr = "error prepending file root";
      goto out;
   }
   if (dataSize + 1 > sizeof gDnDData) {
      Debug("DnDRpcInDataSetCB: data too large\n");
      retStr = "data too large";
      goto out;
   }

   memcpy(gDnDData, data, dataSize + 1);
   gDnDDataSize = dataSize;
   Debug("DnDRpcInDataSetCB: prepended file root [%s] (%"FMTSZ"u)\n",
         CPName_Print(gDnDData, gDnDDataSize), gDnDDataSize);

   retStr = "";
   ret = TRUE;

out:
   free(format);
   free(data);
   free(perDnDDir);
   return RpcIn_SetRetVals(result, resultLen, retStr, ret);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDRpcInMoveCB --
 *
 *       For Host->Guest operations only.
 *       Host user is dragging data over this guest's MKS window
 *
 * Results:
 *       TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *	 Send a gdk event that "moves" the mouse.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
DnDRpcInMoveCB(char const **result,     // OUT
               size_t *resultLen,       // OUT
               const char *name,        // IN
               const char *args,        // IN
               size_t argsSize,         // Ignored
               void *clientData)        // IN: pointer to mainWnd
{
   GtkWidget *mainWnd;
   char *sXCoord;
   char *sYCoord;
   unsigned int index = 0;
   int xCoord, yCoord;

   mainWnd = GTK_WIDGET(clientData);
   if (mainWnd == NULL) {
      return RpcIn_SetRetVals(result, resultLen,
                              "bad clientData passed to callback", FALSE);
   }

   sXCoord = StrUtil_GetNextToken(&index, args, " ");
   sYCoord = StrUtil_GetNextToken(&index, args, " ");

   if (!sXCoord || !sYCoord) {
      Debug("DnDRpcInMove: Failed to parse coords\n");
      free(sXCoord);
      free(sYCoord);
      return RpcIn_SetRetVals(result, resultLen,
                              "error reading mouse move data", FALSE);
   }

   xCoord = atoi(sXCoord);
   yCoord = atoi(sYCoord);

   free(sXCoord);
   free(sYCoord);

   /* Fake a mouse move */
   if (!DnDHGFakeMove(mainWnd, xCoord, yCoord)) {
      Debug("DnDRpcInMove: Failed to fake mouse movement\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "failed to move mouse", FALSE);
   }

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDRpcInDataFinishCB --
 *
 *       For Host->Guest operations only.
 *       Host has finished transferring DnD data to the guest. We do any post
 *       H->G operation cleanup here, like removing the block on the staging
 *       directory, picking a new file root, and informing the host of the new
 *       root.
 *
 * Results:
 *       TRUE on success, FALSE otherwise
 *
 * Side effects:
 *	 None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
DnDRpcInDataFinishCB(char const **result,   // OUT
                     size_t *resultLen,     // OUT
                     const char *name,      // IN
                     const char *args,      // IN
                     size_t argsSize,       // Ignored
                     void *clientData)      // Ignored
{
   unsigned int index = 0;
   char *state;

   Debug("DnDRpcInDataFinishCB: enter\n");

   state = StrUtil_GetNextToken(&index, args, " ");
   if (!state) {
      Debug("DnDRpcInDataFinishCB: could not get dnd finish state.\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "could not get dnd finish state", FALSE);
   }

   /* 
    * If the guest doesn't support vmblock, we'll have bailed out of 
    * DndRpcInDropCB before setting gHGDataPending. Thus, it doesn't make sense
    * to pop a warning here, but let's keep the message around just in case 
    * there can be a failure worth hearing about.
    */
   if (!gHGDataPending) {
      Debug("DnDRpcInDataFinishCB: expected gHGDataPending to be set.\n");
   }

   gHGDataPending = FALSE;

   /*
    * The host will send us "success" or "error", depending on whether the
    * transfer finished successfully.  In either case we remove the pending
    * block, but in the "error" case we also need to delete all the files so
    * the destination application doesn't access the partially copied files and
    * mistake them for a successful drop.
    */
   if (strcmp(state, "success") != 0) {
      /* On any non-success input, delete the files. */
      DnD_DeleteStagingFiles(gFileRoot, FALSE);
   }

   free(state);

   if (gBlockFd >= 0 && !DnD_RemoveBlock(gBlockFd, gFileRoot)) {
      Warning("DnDRpcInDataFinishCB: could not remove block on %s\n",
              gFileRoot);
   }

   /* Pick a new file root and send that to the host for the next DnD. */
   if (!DnDSendVmxNewFileRoot("dnd.setGuestFileRoot")) {
      Debug("DnDRpcInDataFinishCB: Failed to send dnd.setGuestFileRoot "
            "message to host\n");
      return RpcIn_SetRetVals(result, resultLen, "could not send guest root", FALSE);
   }

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDRpcInDropCB --
 *
 *       For Host->Guest operations only.
 *       Host user has dropped data over this guest's MKS window.  We add
 *       a block on the staging directory then send a fake mouse release to
 *       invoke the drop completion (from Gtk's point of view).
 *
 * Results:
 *       TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *	 None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
DnDRpcInDropCB(char const **result,   // OUT
               size_t *resultLen,     // OUT
               const char *name,      // IN
               const char *args,      // IN
               size_t argsSize,       // Ignored
               void *clientData)      // IN: Gtk window widget
{
   GtkWidget *mainWnd;
   char *sXCoord;
   char *sYCoord;
   unsigned int index = 0;
   int xCoord, yCoord;

   Debug("DnDRpcInDropCB: enter\n");

   gDoneDragging = TRUE;

   mainWnd = GTK_WIDGET(clientData);
   if (mainWnd == NULL) {
      return RpcIn_SetRetVals(result, resultLen,
                              "bad clientData passed to callback", FALSE);
   }

   sXCoord = StrUtil_GetNextToken(&index, args, " ");
   sYCoord = StrUtil_GetNextToken(&index, args, " ");

   if (!sXCoord || !sYCoord) {
      Debug("DnDRpcInDropCB: Failed to parse coords\n");
      free(sXCoord);
      free(sYCoord);
      return RpcIn_SetRetVals(result, resultLen,
                              "must specify drop coordinates", FALSE);
   }

   xCoord = atoi(sXCoord);
   yCoord = atoi(sYCoord);

   free(sXCoord);
   free(sYCoord);

   Debug("DnDRpcInDropCB: Received drop notification at (%d,%d)\n",
         xCoord, yCoord);

   /*
    * Add a block on the guest file root, warp the pointer, then fake the mouse
    * release.  Make sure we'll succeed before modifying any mouse state in the
    * guest.
    */
   if (gBlockFd < 0) {
      /*
       * We shouldn't get here since DnDRpcInEnterCB() checks this, but we'll
       * check rather than ASSERT just in case.
       */
      return RpcIn_SetRetVals(result, resultLen,
                              "blocking file system unavailable", FALSE);
   }

   if (!DnD_AddBlock(gBlockFd, gFileRoot)) {
      return RpcIn_SetRetVals(result, resultLen, "could not add block", FALSE);
   }

   /* Update state before causing faking any mouse or keyboard changes. */
   gHGDataPending = TRUE;


   if (!DnDHGFakeDrop(mainWnd, xCoord, yCoord)) {
      Debug("DnDRpcInDropCB: failed to fake drop\n");
      return RpcIn_SetRetVals(result, resultLen, "failed to fake drop", FALSE);
   }

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 * Guest->Host RPC callback implementations
 */

/*
 *----------------------------------------------------------------------------
 *
 * DnDRpcInMouseUngrabCB --
 *
 *    For Guest->Host operations only.
 *
 *    Called when a mouse ungrab is attempted with the mouse button down.  When
 *    the MKS sees mouse movements outside of the clip (the viewable portion of
 *    the guest's display) while a mouse button is down, this function is
 *    called so we can inform the MKS whether to allow the ungrab (and start
 *    a DnD if one is pending).
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    The GDK window is moved and resized, and the mouse is moved over it.
 *
 *----------------------------------------------------------------------------
 */

static Bool
DnDRpcInMouseUngrabCB(char const **result,      // OUT
                      size_t *resultLen,        // OUT
                      const char *name,         // IN
                      const char *args,         // IN
                      size_t argsSize,          // Ignored
                      void *clientData)         // IN
{
   unsigned int index = 0;
   GtkWidget *mainWnd;
   int32 xPos;
   int32 yPos;

   Debug("Got DnDRpcInMouseUngrabCB\n");
   mainWnd = GTK_WIDGET(clientData);
   if (mainWnd == NULL) {
      Warning("DnDRpcInMouseUngrabCB: invalid clientData\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "bad clientData passed to callback", FALSE);
   }

   /*
    * If there is already a DnD or copy/paste in progress (including the file
    * transfer), don't allow another.
    */
   if (gHGDataPending || gGHState.dragInProgress || CopyPaste_InProgress()) {
      RpcOut_sendOne(NULL, NULL, "dnd.notpending");
      return RpcIn_SetRetVals(result, resultLen,
                              "dnd already in progress", FALSE);
   }

   if (!StrUtil_GetNextIntToken(&xPos, &index, args, " ")) {
      Warning("DnDRpcInMouseUngrabCB: could not parse x coordinate\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "Failed to parse x coordinate", FALSE);
   }

   if (!StrUtil_GetNextIntToken(&yPos, &index, args, " ")) {
      Warning("DnDRpcInMouseUngrabCB: could not parse y coordinate\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "Failed to parse y coordinate", FALSE);
   }

   Debug("DnDRpcInMouseUngrabCB: Received (%d,%d)\n", xPos, yPos);

   /*
    * If there is no DnD pending, inform the host so the MKS can start sending
    * mouse packets again.
    */
   if (!DnDGHDragPending(mainWnd)) {
      RpcOut_sendOne(NULL, NULL, "dnd.notpending");
      return RpcIn_SetRetVals(result, resultLen, "DnD not pending", TRUE);
   }

   /* The host only gives us coordinates within our screen */
   ASSERT(xPos >= 0);
   ASSERT(yPos >= 0);

   /*
    * Fake mouse movements over the window to try and generate a "drag_motion"
    * signal from GTK.  If a drag is pending, that signal will be sent to our
    * widget and DnDGtkDragMotionCB will be invoked to start the DnD
    * operation.
    */
   if (!DnDGHFakeDrag(mainWnd)) {
      Warning("DnDRpcInMouseUngrabCB: could not fake X events\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "error faking X events", FALSE);
   }

   /*
    * Add event to fire and hide our widget if a DnD is not pending.  Note that
    * this is here in case our drag pending heuristic for Xdnd and Motif does
    * not encompass all cases, or if the X events we generate don't cause the
    * "drag_motion" for some other reason.
    */
   gGHState.event = EventManager_Add(gEventQueue, RPCIN_POLL_TIME * 100,
                                     DnDGHXEventTimeout, mainWnd);
   if (!gGHState.event) {
      Warning("DnDRpcInMouseUngrabCB: could not create event\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "could not create timeout event", FALSE);
   }

   gGHState.dragInProgress = FALSE;
   gGHState.ungrabReceived = TRUE;

   Debug("DnDRpcInMouseUngrabCB finished\n");
   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDRpcInGetNextFileCB --
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
 *    entry (through call to DnDGHFileListGetNext()).
 *
 *----------------------------------------------------------------------------
 */

static Bool
DnDRpcInGetNextFileCB(char const **result,      // OUT
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
   GtkWidget *mainWnd;
   Bool res;

   mainWnd = GTK_WIDGET(clientData);
   if (mainWnd == NULL) {
      DnDGHCancel(NULL);
      return RpcIn_SetRetVals(result, resultLen,
                              "bad clientData passed to callback", FALSE);
   }

   /*
    * Retrieve a pointer to the next filename and its size from the list stored
    * in the G->H DnD state.  Note that fileName should not be free(3)d here
    * since an additional copy is not allocated.
    */
   res = DnDGHFileListGetNext(&fileName, &fileNameSize);

   if (!res) {
      Warning("DnDRpcInGetNextFileCB: error retrieving file name\n");
      DnDGHCancel(mainWnd);
      return RpcIn_SetRetVals(result, resultLen, "error getting file", FALSE);
   }

   if (!fileName) {
      /* There are no more files to send */
      Debug("DnDRpcInGetNextFileCB: reached end of Guest->Host file list\n");
      return RpcIn_SetRetVals(result, resultLen, "|end|", TRUE);
   }

   if (fileNameSize + 1 + fileNameSize > sizeof resultBuffer) {
      Warning("DnDRpcInGetNextFileCB: filename too large (%"FMTSZ"u)\n", fileNameSize);
      DnDGHCancel(mainWnd);
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
      Warning("DnDRpcInGetNextFileCB: could not convert to CPName\n");
      DnDGHCancel(mainWnd);
      return RpcIn_SetRetVals(result, resultLen,
                              "error on CPName conversion", FALSE);
   }

   /* Set manually because RpcIn_SetRetVals() assumes no NUL characters */
   *result = resultBuffer;
   *resultLen = fileNameSize + 1 + cpNameSize;

   Debug("DnDRpcInGetNextFileCB: [%s] (%"FMTSZ"u)\n",
         CPName_Print(*result, *resultLen), *resultLen);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDRpcInFinishCB --
 *
 *    For Guest->Host operations only.
 *
 *    Invoked when host side of DnD operation has finished.
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
DnDRpcInFinishCB(char const **result,      // OUT
                 size_t *resultLen,        // OUT
                 const char *name,         // IN
                 const char *args,         // IN
                 size_t argsSize,          // Ignored
                 void *clientData)         // IN
{
   GtkWidget *mainWnd;
   char *effect = NULL;
   unsigned int index = 0;
   char *retStr;
   Bool ret = FALSE;

   mainWnd = GTK_WIDGET(clientData);
   if (mainWnd == NULL) {
      retStr = "bad clientData passed to callback";
      goto exit;
   }

   effect = StrUtil_GetNextToken(&index, args, " ");
   if (!effect) {
      Warning("DnDRpcInFinishCB: no drop effect provided\n");
      retStr = "drop effect not provided";
      goto exit;
   }

   if (strcmp(effect, "cancel") == 0) {
      DnDSendEscapeKey(mainWnd);
      DnDGHCancel(mainWnd);
   } else {
      /*
       * The drop happened on the host.  Fake X events such that our window is
       * placed at the mouse's coordinates and raised, then fake a button
       * release on the window.  This causes us to get a "drag_drop" signal
       * from GTK on our widget.
       */
      if (!DnDGHFakeDrop(mainWnd)) {
         Warning("DnDRpcInFinishCB: could not fake X events\n");
         retStr = "error faking X events";
         goto exit;
      }

      gGHState.event = EventManager_Add(gEventQueue, RPCIN_POLL_TIME * 10,
                                           DnDGHXEventTimeout, mainWnd);
      if (!gGHState.event) {
         Warning("DnDRpcInFinishCB: could not create event\n");
         retStr = "could not create timeout event";
         goto exit;
      }
   }

   retStr = "";
   ret = TRUE;

exit:
   if (!ret) {
      DnDGHCancel(mainWnd);
   }

   free(effect);
   gGHState.dragInProgress = FALSE;
   return RpcIn_SetRetVals(result, resultLen, retStr, ret);
}


/*
 * Host->Guest (drop source) Gtk callback implementations
 */

/*
 *-----------------------------------------------------------------------------
 *
 * DnDGtkBeginCB --
 *
 *       "drag_begin" signal handler for GTK.  This signal will be received
 *       after the fake mouse press sent in DnDRpcInEnterCB() is performed.
 *       Here we simply initialize our state variables.
 *
 * Results:
 *       None
 *
 * Side effects:
 *	 None
 *
 *-----------------------------------------------------------------------------
 */

static void
DnDGtkBeginCB(GtkWidget *widget,   // IN: the widget under the drag
              GdkDragContext *dc,  // IN: the drag context maintained by gdk
              gpointer data)       // IN: unused
{
   GtkWidget *mainWnd = GTK_WIDGET(data);

   Debug("DnDGtkBeginCB: entry\n");

   if ((widget == NULL) || (mainWnd == NULL) || (dc == NULL)) {
      return;
   }

   gHGDnDInProgress = TRUE;
   gDoneDragging = FALSE;
   gHGDataPending = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDGtkEndCB --
 *
 *       "drag_end" signal handler for GTK. This is called when a drag and drop has
 *       completed. So this function is the last one to be called in any given DnD
 *       operation.
 *
 * Results:
 *       None
 *
 * Side effects:
 *	 None
 *
 *-----------------------------------------------------------------------------
 */

static void
DnDGtkEndCB(GtkWidget *widget,   // IN: the widget under the drag
            GdkDragContext *dc,  // IN: the drag context maintained by gdk
            gpointer data)       // IN: unused
{

   GtkWidget *mainWnd = GTK_WIDGET(data);

   Debug("DnDGtkEndCB: enter\n");

   if (mainWnd == NULL || dc == NULL) {
      return;
   }

   /*
    * Do not set gHGDataPending to FALSE since DnD operation completes before
    * the data transfer.
    */
   gDoneDragging = FALSE;
   gHGDnDInProgress = FALSE;

   RpcOut_sendOne(NULL, NULL, "dnd.finish %d", DROPEFFECT_COPY);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDGtkDataRequestCB --
 *       DnD "drag_data_get" handler, for handling requests for DnD data on the
 *       specified widget. This function is called when there is need for DnD data
 *       on thesource, so this function is responsible for setting up the dynamic
 *       data exchange buffer and sending it out.
 *
 * Results:
 *       None
 *
 * Side effects:
 *	 Data is avaiable to drop target.
 *
 *-----------------------------------------------------------------------------
 */

static void
DnDGtkDataRequestCB(GtkWidget *widget,                // IN
                    GdkDragContext *dc,               // IN
                    GtkSelectionData *selection_data, // IN/OUT: buffer for the data
                    guint info,                       // IN: the requested fo the data
                    guint time,                       // IN: unused
                    gpointer data)                    // IN: unused
{
   const char *begin;
   const char *end;
   const char *next;
   const char *pre;
   const char *post;
   size_t preLen;
   size_t postLen;
   size_t len;
   Bool insertSpace;
   char *text = NULL;
   size_t textLen = 1;

   Debug("DnDGtkDataRequestCB: enter\n");

   if ((widget == NULL) || (dc == NULL) || (selection_data == NULL)) {
      Debug("DnDGtkDataRequestCB: Error, widget or dc or selection_data is invalid\n");
      return;
   }

   /* Do nothing if we have not finished dragging yet */
   if (!gDoneDragging) {
      Debug("DnDGtkDataRequestCB: not done dragging yet\n");
      return;
   }

   /* Setup the format string components */
   switch (info) {
   case DRAG_TARGET_INFO_URI_LIST:      /* text/uri-list */
      pre = DND_URI_LIST_PRE;
      preLen = sizeof DND_URI_LIST_PRE - 1;
      post = DND_URI_LIST_POST;
      postLen = sizeof DND_URI_LIST_POST - 1;
      insertSpace = FALSE;
      break;
   case DRAG_TARGET_INFO_TEXT_PLAIN:    /* text/plain */
      pre = DND_TEXT_PLAIN_PRE;
      preLen = sizeof DND_TEXT_PLAIN_PRE - 1;
      post = DND_TEXT_PLAIN_POST;
      postLen = sizeof DND_TEXT_PLAIN_POST - 1;
      insertSpace = TRUE;
      break;
   case DRAG_TARGET_INFO_STRING:        /* STRING */
      pre = DND_STRING_PRE;
      preLen = sizeof DND_STRING_PRE - 1;
      post = DND_STRING_POST;
      postLen = sizeof DND_STRING_POST - 1;
      insertSpace = TRUE;
      break;
   default:
      Log("DnDGtkDataRequestCB: invalid drag target info\n");
      return;
   }


   /*
    * Set begin to first non-NUL character and end to last NUL character to
    * prevent errors in calling CPName_GetComponentGeneric().
    */
   for(begin = gDnDData; *begin == '\0'; begin++)
      ;
   end = CPNameUtil_Strrchr(gDnDData, gDnDDataSize + 1, '\0');
   ASSERT(end);

   /* Build up selection data */
   while ((len = CPName_GetComponentGeneric(begin, end, "", &next)) != 0) {
      const size_t origTextLen = textLen;
      Bool freeBegin = FALSE;

      if (len < 0) {
         Log("DnDGtkDataRequestCB: error getting next component\n");
         if (text) {
            free(text);
         }
         return;
      }

      /*
       * A URI list will expect the provided path to be escaped.  If we cannot
       * escape the path for some reason we just use the unescaped version and
       * hope that it works.
       */
      if (info == DRAG_TARGET_INFO_URI_LIST) {
         size_t newLen;
         char *escapedComponent;
         int bytesToEsc[256] = { 0, };

         /* We escape the following characters based on RFC 1630. */
         bytesToEsc['#'] = 1;
         bytesToEsc['?'] = 1;
         bytesToEsc['*'] = 1;
         bytesToEsc['!'] = 1;
         bytesToEsc['%'] = 1;  /* Escape character */

         escapedComponent = Escape_Do('%', bytesToEsc, begin, len, &newLen);
         if (escapedComponent) {
            begin = escapedComponent;
            len = newLen;
            freeBegin = TRUE;
         }
      }

      /*
       * Append component.  NUL terminator was accounted for by initializing
       * textLen to one above.
       */
      textLen += preLen + len + postLen + (insertSpace ? 1 : 0);
      text = Util_SafeRealloc(text, textLen);
      Str_Snprintf(text + origTextLen - 1,
                   textLen - origTextLen + 1,
                   "%s%s%s", pre, begin, post);

      if (insertSpace && next != end) {
         ASSERT(textLen - 2 >= 0);
         text[textLen - 2] = ' ';
         text[textLen - 1] = '\0';
      }

      if (freeBegin) {
         free((void *)begin);
      }

      /* Iterate to next component */
      begin = next;
   }

   /*
    * Send out the data using the selection system. When sending a string, GTK will
    * ensure that a null terminating byte is added to the end so we do not need to
    * add it. GTK also copies the data so the original will never be modified.
    */
   Debug("DnDGtkDataRequestCB: calling gtk_selection_data_set with [%s]\n", text);
   gtk_selection_data_set(selection_data, selection_data->target,
                          8, /* 8 bits per character. */
                          text, textLen);
   free(text);
}


/*
 * Guest->Host (drop target) Gtk callback implementations
 */

/*
 *----------------------------------------------------------------------------
 *
 * DnDGtkDragMotionCB --
 *
 *    "drag_motion" signal handler for GTK.  This is invoked each time the
 *    mouse moves over the drag target (destination) window when a DnD is
 *    pending.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    RPC messages are sent to the host to proxy the DnD over.
 *
 *----------------------------------------------------------------------------
 */

static gboolean
DnDGtkDragMotionCB(GtkWidget *widget,    // IN: target widget
                   GdkDragContext *dc,   // IN: the GDK drag context
                   gint x,               // IN: x position of mouse
                   gint y,               // IN: y position of mouse
                   guint time,           // IN: time of event
                   gpointer data)        // IN: our private data
{
   GdkAtom commonTarget = 0;
   Bool found = FALSE;
   uint32 i;

   ASSERT(widget);
   ASSERT(widget == data);
   ASSERT(dc);

   Debug("DnDGtkDragMotionCB: entry (x=%d, y=%d, time=%d)\n", x, y, time);

   /*
    * We'll get a number of these and should only carry on these operations on
    * the first one.
    */
   if (gGHState.dragInProgress) {
      Debug("DnDGtkDragMotionCB: drag already in progress\n");
      return FALSE;
   }

   /*
    * Sometimes (rarely) real user mouse movements will trigger "drag_motion"
    * signals after we have already handled them.  Prevent resetting the data
    * and trying to start a new DnD operation.
    */
   if (!gGHState.ungrabReceived) {
      Debug("DnDGtkDragMotionCB: extra drag motion without ungrab\n");
      return FALSE;
   }

   gGHState.ungrabReceived = FALSE;

   /* Remove event that hides our widget out of band from the DnD protocol. */
   if (gGHState.event) {
      Debug("DnDGtkDragMotionCB: removed pending event\n");
      EventManager_Remove(gGHState.event);
      gGHState.event = NULL;
   }

   /*
    * Note that gdk_drag_status() is called for us by GTK since we passed in
    * GTK_DEST_DEFAULT_MOTION to gtk_drag_dest_set().  We'd handle it
    * ourselves, but GTK 1.2.10 has a "bug" that requires us to provide this
    * flag to get drag_leave and drag_drop signals.
    */

   /*
    * We need to try and find a common target format with the list of formats
    * offered by the drag source.  This list is stored in the drag context's
    * targets field, and each list member's data variable is a GdkAtom.  We
    * translated our supported targets into GdkAtoms in gTargetEntryAtom at
    * initialization.  Note that the GdkAtom value is an index into a table of
    * strings maintained by the X server, so if they are equivalent then
    * a common mime type is found.
    */
   for (i = 0; i < ARRAYSIZE(gTargetEntryAtom) && !found; i++) {
      GList *currContextTarget = dc->targets;

      while (currContextTarget) {
         if (gTargetEntryAtom[i] == (GdkAtom)currContextTarget->data) {
            commonTarget = gTargetEntryAtom[i];
            found = TRUE;
            break;
         }
         currContextTarget = currContextTarget->next;
      }
   }

   if (!found) {
      Warning("DnDGtkDragMotionCB: could not find a common target format\n");
      DnDGHCancel(widget);
      return FALSE;
   }

   /*
    * Request the data.  A "drag_data_received" signal will be sent to widget
    * (that's us) upon completion.
    */
   gtk_drag_get_data(widget, dc, commonTarget, time);


   gGHState.dragInProgress = TRUE;
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGtkDragDataReceivedCB --
 *
 *    "drag_data_received" signal handler for GTK.  Invoked when the data
 *    requested by a gtk_drag_get_data() call is ready.
 *
 *    This function actually begins the drag operation with the host by first
 *    setting the data ("dnd.data.set" RPC command) and then starting the DnD
 *    ("dnd.enter" RPC command).
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
DnDGtkDragDataReceivedCB(GtkWidget *widget,          // IN
                         GdkDragContext *dc,         // IN
                         gint x,                     // IN
                         gint y,                     // IN
                         GtkSelectionData *dragData, // IN
                         guint info,                 // IN
                         guint time,                 // IN
                         gpointer data)              // IN
{
   const char rpcHeader[] = "dnd.data.set CF_HDROP ";
   const size_t rpcHeaderSize = sizeof rpcHeader - 1;
   char *rpcBody = NULL;
   size_t rpcBodySize = 0;
   char *rpc;
   size_t rpcSize;


   Debug("DnDGtkDragDataReceivedCB: entry\n");

   if (dragData->length < 0) {
      Warning("DnDGtkDragDataReceivedCB: received length < 0 error\n");
      goto error;
   }

   gGHState.dragContext = dc;
   gGHState.time = time;

   /*
    * Construct the body of the RPC message and our Guest->Host file list.
    */
   if (dragData->target == gTargetEntryAtom[DRAG_TARGET_INFO_URI_LIST]) {
      char *currName;
      size_t currSize;
      size_t index = 0;
      char *ghFileList = NULL;
      size_t ghFileListSize = 0;

      Debug("DnDGtkDragDataReceivedCB: uri-list [%s]\n", dragData->data);

      /*
       * Get the the full filenames and last components from the URI list.  The
       * body of the RPC message will be these last components delimited with
       * NUL characters; the Guest->Host file list will be the full paths
       * delimited by NUL characters.
       */
      while ((currName = DnD_UriListGetNextFile(dragData->data,
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
         Warning("DnDGtkDragDataReceivedCB: no filenames retrieved "
                 "from URI list\n");
         free(ghFileList);
         free(rpcBody);
         goto error;
      }

      /* Set the list of full paths for use in the "dnd.data.get.file" callback */
      DnDGHFileListSet(ghFileList, ghFileListSize);

      /* rpcBody (and its size) will always contain a trailing NUL character */
      rpcBodySize--;
   } else {
      Warning("DnDGtkDragDataReceivedCB: unknown target format used [%s]\n",
              dragData->data);
      goto error;
   }

   /*
    * Set the drag data on the host, followed by sending the drag enter
    */
   rpcSize = rpcHeaderSize + rpcBodySize;
   rpc = Util_SafeMalloc(rpcSize);
   memcpy(rpc, rpcHeader, rpcHeaderSize);
   memcpy(rpc + rpcHeaderSize, rpcBody, rpcBodySize);
   free(rpcBody);

   Debug("DnDGtkDragMotionCB: Sending: [%s] (%"FMTSZ"u)\n",
         CPName_Print(rpc, rpcSize), rpcSize);
   if (!RpcOut_SendOneRaw(rpc, rpcSize, NULL, NULL)) {
      Warning("DnDGtkDragMotionCB: failed to send dnd.data.set message\n");
      free(rpc);
      goto error;
   }

   free(rpc);

   if (!RpcOut_sendOne(NULL, NULL, "dnd.enter 1 CF_HDROP")) {
      Warning("DnDGtkDragMotionCB: failed to send dnd.enter message\n");
      goto error;
   }

   return;

error:
   DnDGHCancel(widget);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGtkDragDropCB --
 *
 *    "drag_drop" signal handler for GTK.  This is invoked when a mouse button
 *    release occurs on our widget.  We generate that mouse button release in
 *    DnDRpcInFinishCB() when the host indicates that the drop has occurred and
 *    the files have been successfully transferred to the guest.
 *
 * Results:
 *    TRUE indicates to GTK that it need not run other handlers, FALSE
 *    otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static gboolean
DnDGtkDragDropCB(GtkWidget *widget,  // IN: widget event occurred on
                 GdkDragContext *dc, // IN: Destination drag context
                 gint x,             // IN: x coordinate of drop
                 gint y,             // IN: y coordinate of drop
                 guint time,         // IN: time of event
                 gpointer data)      // IN: our private data
{
   ASSERT(widget);
   ASSERT(widget == data);

   Debug("DnDGtkDragDropCB: entry (%d, %d)\n", x, y);

   /* Remove timeout callback that was set in case we didn't get here */
   if (gGHState.event) {
      Debug("DnDGtkDragDropCB: removed pending event\n");
      EventManager_Remove(gGHState.event);
      gGHState.event = NULL;
   }

   /* Hide our window so we don't receive stray signals */
   gtk_widget_hide(widget);

   gtk_drag_finish(dc, TRUE, FALSE, time);

   /* Reset all Guest->Host state */
   DnDGHStateInit(widget);

   return FALSE;
}


/*
 * Utility functions
 */

/*
 *----------------------------------------------------------------------------
 *
 * DnD_GetNewFileRoot --
 *
 *    Convenience function that gets a new file root for use on a single DnD
 *    operation and sets the global file root variable accordingly.
 *
 * Results:
 *    Size of root string (not including NUL terminator).
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
DnD_GetNewFileRoot(char *fileRoot,         // IN/OUT
                   int bufSize)            // IN: sizeof fileRoot
{
   char *newDir = NULL;
   size_t fileRootSize;

   newDir = DnD_CreateStagingDirectory();
   if (newDir == NULL) {
      /*
       * Fallback on base of file root if we couldn't create a staging
       * directory for this DnD operation.  This is what Windows DnD does.
       */
      Str_Strcpy(fileRoot, DnD_GetFileRoot(), bufSize);
      return strlen(fileRoot);
   } else {
      fileRootSize = strlen(newDir);
      ASSERT(fileRootSize < bufSize);
      memcpy(fileRoot, newDir, fileRootSize);
      fileRoot[fileRootSize] = '\0';
      free(newDir);
      return fileRootSize;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDSendVmxNewFileRoot --
 *
 *    Sends the VMX a new file root with the provided RPC command.
 *
 * Results:
 *    TRUE on success, FALSE on failure
 *
 * Side effects:
 *    gFileRoot is repopulated.
 *
 *----------------------------------------------------------------------------
 */

static Bool
DnDSendVmxNewFileRoot(char *rpcCmd)     // IN: RPC command
{
   int32 rpcCommandSize;
   int32 cpNameSize;
   int32 rpcMessageSize;
   char *rpcMessage;
   char *cur;

   /* Repopulate gFileRoot */
   gFileRootSize = DnD_GetNewFileRoot(gFileRoot, sizeof gFileRoot);

   /*
    * Here we must convert the file root before sending it across the
    * backdoor.  We can only communicate with new VMXs (v2 DnD), so we only
    * need to handle that case here.
    *
    * <rpcCmd> <file root in local format><NUL><file root in CPName>
    */
   rpcCommandSize = strlen(rpcCmd);

   /*
    * ConvertToRoot below will append the root share name, so we need to
    * make room for it in our buffer.
    */
   rpcMessageSize = rpcCommandSize + 1 +
                    gFileRootSize + 1 +
                    HGFS_STR_LEN(HGFS_SERVER_POLICY_ROOT_SHARE_NAME) + 1 +
                    gFileRootSize + 1;

   rpcMessage = Util_SafeCalloc(1, rpcMessageSize);
   memcpy(rpcMessage, rpcCmd, rpcCommandSize);
   rpcMessage[rpcCommandSize] = ' ';
   cur = rpcMessage + rpcCommandSize + 1;

   memcpy(cur, gFileRoot, gFileRootSize);
   cur += gFileRootSize;
   *cur = '\0';
   cur++;

   Debug("DnDSendVmxNewFileRoot: calling CPNameUtil_ConvertToRoot(%s, %"FMTSZ"u, %p)\n",
         gFileRoot, rpcMessageSize - (cur - rpcMessage), cur);
   cpNameSize = CPNameUtil_ConvertToRoot(gFileRoot,
                                         rpcMessageSize - (cur - rpcMessage),
                                         cur);
   if (cpNameSize < 0) {
      Debug("DnDSendVmxNewFileRoot: Could not convert file root to CPName\n");
      free(rpcMessage);
      return FALSE;
   }

   /* Readjust message size for actual length */
   rpcMessageSize = rpcCommandSize + 1 +
                    gFileRootSize + 1 +
                    cpNameSize + 1;

   Debug("DnDSendVmxNewFileRoot: sending root [%s] (%d)\n",
         CPName_Print(rpcMessage, rpcMessageSize), rpcMessageSize);

   /*
    * We must use RpcOut_SendOneRaw() here since RpcOut_sendOne() assumes a
    * string and we are using CPName format.
    */
   if (!RpcOut_SendOneRaw(rpcMessage, rpcMessageSize, NULL, NULL)) {
      Debug("DnDSendVmxNewFileRoot: Failed to send %s message to host\n", rpcCmd);
      free(rpcMessage);
      return FALSE;
   }

   free(rpcMessage);
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFakeXEvents --
 *
 *    Fake X mouse events and window movement for the provided Gtk widget.
 *
 *    This function will optionally show the widget, move the provided widget
 *    to either the provided location or the current mouse position if no
 *    coordinates are provided, and cause a button press or release event.
 *
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    Other X events should be generated from those faked here.
 *
 *----------------------------------------------------------------------------
 */

static Bool
DnDFakeXEvents(GtkWidget *widget,  // IN: the Gtk widget
               Bool showWidget,    // IN: whether to show Gtk widget
               Bool buttonEvent,   // IN: whether to send a button event
               Bool buttonPress,   // IN: whether to press or release mouse
               Bool moveWindow,    // IN: whether to move our window too
               Bool coordsProvided,// IN: whether coordinates provided
               int xCoord,         // IN: x coordinate
               int yCoord)         // IN: y coordinate
{
   Window rootWnd;
   Bool ret;
   Display *dndXDisplay;
   Window dndXWindow;

   ASSERT(widget);

   dndXDisplay = GDK_WINDOW_XDISPLAY(widget->window);
   dndXWindow = GDK_WINDOW_XWINDOW(widget->window);

   /*
    * Turn on X synchronization in order to ensure that our X events occur in
    * the order called.  In particular, we want the window movement to occur
    * before the mouse movement so that the events we are coercing do in fact
    * happen.
    */
   XSynchronize(dndXDisplay, True);

   if (showWidget) {
      Debug("DnDFakeXEvents: showing Gtk widget\n");
      gtk_widget_show(widget);
      gdk_window_show(widget->window);
   }

   /* Get the current location of the mouse if coordinates weren't provided. */
   if (!coordsProvided) {
      Window rootReturn;
      Window childReturn;
      int rootXReturn;
      int rootYReturn;
      int winXReturn;
      int winYReturn;
      unsigned int maskReturn;

      rootWnd = RootWindow(dndXDisplay, DefaultScreen(dndXDisplay));
      ret = XQueryPointer(dndXDisplay, rootWnd, &rootReturn, &childReturn,
                          &rootXReturn, &rootYReturn, &winXReturn, &winYReturn,
                          &maskReturn);
      if (ret == False) {
         Warning("DnDFakeXEvents: XQueryPointer() returned False.\n");
         XSynchronize(dndXDisplay, False);
         return FALSE;
      }

      Debug("DnDFakeXEvents: mouse is at (%d, %d)\n", rootXReturn, rootYReturn);

      xCoord = rootXReturn;
      yCoord = rootYReturn;
   }

   if (moveWindow) {
      /*
       * Make sure the window is at this point and at the top (raised).  The
       * window is resized to be a bit larger than we would like to increase
       * the likelihood that mouse events are attributed to our window -- this
       * is okay since the window is invisible and hidden on cancels and DnD
       * finish.
       */
      XMoveResizeWindow(dndXDisplay, dndXWindow, xCoord, yCoord, 25, 25);
      XRaiseWindow(dndXDisplay, dndXWindow);
   }

   /*
    * Generate mouse movements over the window.  The second one makes ungrabs
    * happen more reliably on KDE, but isn't necessary on GNOME.
    */
   XTestFakeMotionEvent(dndXDisplay, -1, xCoord, yCoord, CurrentTime);
   XTestFakeMotionEvent(dndXDisplay, -1, xCoord + 1, yCoord + 1, CurrentTime);

   if (buttonEvent) {
      Debug("DnDFakeXEvents: faking left mouse button %s\n",
            buttonPress ? "press" : "release");
      XTestFakeButtonEvent(dndXDisplay, 1, buttonPress, CurrentTime);
   }

   XSynchronize(dndXDisplay, False);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDSendEscapeKey --
 *
 *    Sends the escape key, canceling any pending drag and drop on the guest.
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
DnDSendEscapeKey(GtkWidget *mainWnd)  // IN
{
   Display *dndXDisplay;
   uint32 escKeycode;

   Debug("DnDRpcInFinishCB: faking ESC key press/release\n");

   dndXDisplay = GDK_WINDOW_XDISPLAY(mainWnd->window);
   escKeycode = XKeysymToKeycode(dndXDisplay, XK_Escape);

   XTestFakeKeyEvent(dndXDisplay, escKeycode, TRUE, CurrentTime);
   XTestFakeKeyEvent(dndXDisplay, escKeycode, FALSE, CurrentTime);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGHDragPending --
 *
 *    Determine whether a drag is currently pending within the guest by
 *    inspecting the internal state of the X server.  Note that Gtk supports
 *    both the Xdnd and Motif protocols, so we check each one of those.
 *
 * Results:
 *    TRUE if a Drag operation is pending (waiting for a drop), FALSE
 *    otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
DnDGHDragPending(GtkWidget *widget) // IN: our widget
{
   /* Xdnd is much more prevalent, so call it first */
   return DnDGHXdndDragPending(widget) || DnDGHMotifDragPending(widget);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGHXdndDragPending --
 *
 *    Determines whether an Xdnd protocol drag is pending.
 *
 * Results:
 *    TRUE is a drag is pending, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
DnDGHXdndDragPending(GtkWidget *widget) // IN: our widget
{
   GdkAtom xDnDSelection;
   Window owner;

   xDnDSelection = gdk_atom_intern("XdndSelection", TRUE);
   if (xDnDSelection == None) {
      Warning("DnDGHXdndDragPending: could not obtain Xdnd selection atom\n");
      return FALSE;
   }

   /*
    * The XdndSelection atom will only have an owner if there is a drag in
    * progress.
    */
   owner = XGetSelectionOwner(GDK_WINDOW_XDISPLAY(widget->window),
                              GDKATOM_TO_ATOM(xDnDSelection));

   Debug("DnDGHXdndDragPending: an Xdnd drag is %spending\n",
         owner != None ? "" : "not ");

   return owner != None;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGHXdndClearPending --
 *
 *    Clear the ownership of the XdndSelection selection atom that we use to
 *    determine if a Xdnd drag is pending.
 *
 *    Note that this function should only be called when a DnD is not in
 *    progress.
 *
 *    Also note that this is function is only necessary to handle desktop
 *    environments that don't clear the selection owner themselves (read KDE).
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
DnDGHXdndClearPending(GtkWidget *widget) // IN: program's widget
{
   GdkAtom xDnDSelection;

   ASSERT(!gGHState.dragInProgress);

   xDnDSelection = gdk_atom_intern("XdndSelection", TRUE);
   if (xDnDSelection == None) {
      return;
   }

   /* Clear current owner by setting owner to None */
   XSetSelectionOwner(GDK_WINDOW_XDISPLAY(widget->window),
                      GDKATOM_TO_ATOM(xDnDSelection), None, CurrentTime);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMotifDragPending --
 *
 *    Determines whether a Motif protocol drag is pending.
 *
 *    XXX This has not yet been tested (looking for an app that actually uses
 *    the Motif protocol)
 *
 * Results:
 *    TRUE if a drag is pending, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
DnDGHMotifDragPending(GtkWidget *widget) // IN: our widget
{
   GdkAtom motifDragWindow;
   Display *dndXDisplay;
   int ret;
   Window rootXWindow;
   Atom type;
   int format;
   unsigned long nitems;
   unsigned long bytesAfter;
   unsigned char *prop;

   motifDragWindow = gdk_atom_intern("_MOTIF_DRAG_WINDOW", TRUE);
   if (motifDragWindow == None) {
      Warning("DnDGHMotifDragPending: could not obtain Motif "
              "drag window atom\n");
      return FALSE;
   }

   dndXDisplay = GDK_WINDOW_XDISPLAY(widget->window);
   rootXWindow = RootWindow(dndXDisplay, DefaultScreen(dndXDisplay));

   /*
    * Try and get the Motif drag window property from X's root window.  If one
    * is provided, a DnD is pending.
    */
   ret = XGetWindowProperty(dndXDisplay, rootXWindow, GDKATOM_TO_ATOM(motifDragWindow),
                            0, 1, False, XA_WINDOW,
                            &type, &format, &nitems, &bytesAfter, &prop);
   if (ret != Success) {
      Warning("DnDGHMotifDragPending: XGetWindowProperty() error.\n");
      return FALSE;
   }

   if (type == None) {
      Debug("DnDGHXdndDragPending: a Motif drag is not pending\n");
      return FALSE;
   }

   Debug("DnDGHXdndDragPending: a Motif drag is pending\n");

   XFree(prop);
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGHFileListClear --
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
DnDGHFileListClear(void)
{
   Debug("DnDGHFileListClear: clearing G->H file list\n");
   if (gGHState.dndFileList) {
      free(gGHState.dndFileList);
      gGHState.dndFileList = NULL;
   }
   gGHState.dndFileListSize = 0;
   gGHState.dndFileListNext = NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGHFileListSet --
 *
 *    Sets the Guest->Host file list that is accessed through
 *    DnDGHFileListGetNext().
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
DnDGHFileListSet(char *fileList,      // IN: new Guest->Host file list
                 size_t fileListSize) // IN: size of the provided list
{
   DnDGHFileListClear();
   gGHState.dndFileList = fileList;
   gGHState.dndFileListSize = fileListSize;
   gGHState.dndFileListNext = fileList;

   Debug("DnDGHFileListSet: [%s] (%"FMTSZ"u)\n",
         CPName_Print(gGHState.dndFileList, gGHState.dndFileListSize),
         gGHState.dndFileListSize);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGHFileListGetNext --
 *
 *    Retrieves the next file in the Guest->Host file list.
 *
 *    Note that this function may only be called after calling
 *    DnDGHFileListSet() and before calling DnDGHFileListClear().
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

static INLINE Bool
DnDGHFileListGetNext(char **fileName,       // OUT: fill with filename location
                     size_t *fileNameSize)  // OUT: fill with filename length
{
   char const *end;
   char const *next;
   int len;

   ASSERT(gGHState.dndFileList);
   ASSERT(gGHState.dndFileListNext);
   ASSERT(gGHState.dndFileListSize > 0);

   /* Ensure end is the last NUL character */
   end = CPNameUtil_Strrchr(gGHState.dndFileList, gGHState.dndFileListSize, '\0');
   ASSERT(end);

   /* Get the length of this filename and a pointer to the next one */
   len = CPName_GetComponentGeneric(gGHState.dndFileListNext, end, "", &next);
   if (len < 0) {
      Warning("DnDGHFileListGetNext: error retrieving next component\n");
      return FALSE;
   }

   /* No more entries in the list */
   if (len == 0) {
      Debug("DnDGHFileListGetNext: no more entries\n");
      *fileName = NULL;
      *fileNameSize = 0;
      return TRUE;
   }

   Debug("DnDGHFileListGetNext: returning [%s] (%d)\n",
         gGHState.dndFileListNext, len);

   *fileName = gGHState.dndFileListNext;
   *fileNameSize = len;
   gGHState.dndFileListNext = (char *)next;
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGHStateInit --
 *
 *    Initializes the Guest->Host DnD state.
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
DnDGHStateInit(GtkWidget *widget)  // IN
{
   Debug("DnDGHStateInit: initializing guest->host state\n");
   gGHState.time = 0;
   gGHState.dragContext = NULL;
   gGHState.dragInProgress = FALSE;
   gGHState.ungrabReceived = FALSE;
   gGHState.event = NULL;
   DnDGHXdndClearPending(widget);
   gtk_widget_hide(widget);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDHGStateInit --
 *
 *    Initialize the Host->Guest DnD state.
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
DnDHGStateInit(void)
{
   gHGDnDInProgress = FALSE;
   gDoneDragging = FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGHCancel --
 *
 *    Resets state and sends a DnD cancel message to the host.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    DnD operation is cancelled.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
DnDGHCancel(GtkWidget *widget) // IN: program's widget
{
   /* Hide our widget so we don't receive stray signals */
   if (widget) {
      gtk_widget_hide(widget);
   }

   if (gGHState.dragContext) {
      gdk_drag_status(gGHState.dragContext, 0, gGHState.time);
   }

   gGHState.dragInProgress = FALSE;

   /*
    * We don't initialize Guest->Host state here since an ungrab/grab/ungrab
    * will cause a cancel but we want the drop of the DnD to still work.
    */
   return RpcOut_sendOne(NULL, NULL, "dnd.finish cancel");
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDGHXEventTimeout --
 *
 *    Cleans up after fake X events do not cause intended events.  Hides the
 *    provided widget and resets all Guest->Host DnD state.
 *
 *    Note that this is expected to occur on ungrab if there is not a DnD
 *    pending, but may also occur at other times (sometimes we do not receive
 *    the drag drop after the mouse button release is faked on KDE).
 *
 *    This function is invoked by the event manager; it is added/removed
 *    to/from the queue in both DnDRpcInMouseUngrabCB() and DnDRpcInFinishCB(),
 *    and DnDGtkDragMotionCB() and DnDGtkDragDropCB() respectively.
 *
 * Results:
 *    TRUE always, so the event manager doesn't stop running.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
DnDGHXEventTimeout(void *clientData) // IN: our widget
{
   GtkWidget *widget = (GtkWidget *)clientData;

   Debug("DnDGHXEventTimeout time out \n");

   if (!gGHState.dragInProgress) {
      gtk_widget_hide(widget);
   }

   /* gGHState.event is cleared with the rest of Guest->Host state */
   DnDGHStateInit(widget);

   return TRUE;
}


/*
 * Public functions invoked by the rest of vmware-user
 */

/*
 *-----------------------------------------------------------------------------
 *
 * DnD_GetVmxDnDVersion --
 *
 *      Ask the vmx for it's dnd version.
 *
 * Results:
 *      The dnd version the vmx supports, 0 if the vmx doesn't know
 *      what we're talking about.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

uint32
DnD_GetVmxDnDVersion(void)
{
   char *reply = NULL;
   size_t replyLen;
   uint32 vmxVersion;

   if (!RpcOut_sendOne(&reply, &replyLen, "vmx.capability.dnd_version")) {
      Debug("DnD_GetVmxDnDVersion: could not get VMX DnD version "
            "capability: %s\n", reply ? reply : "NULL");
      vmxVersion = 0;
   } else {
      vmxVersion = atoi(reply);
      ASSERT(vmxVersion > 1);      /* DnD versions start at 2 */
   }

   free(reply);
   return vmxVersion;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_RegisterCapability --
 *
 *      Register the "dnd" capability. Sometimes this needs to be done separately
 *      from the rest of DnD registration, so we provide it separately here.
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
DnD_RegisterCapability(void)
{
   /* Tell the VMX about the DnD version we support. */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.dnd_version 2")) {
      Debug("DnD_RegisterCapability: could not set guest DnD version capability\n");
      return FALSE;
   } else if (!DnDSendVmxNewFileRoot("dnd.ready enable")) {
      Debug("DnD_RegisterCapability: failed to send dnd.ready message to host\n");
      return FALSE;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_Register --
 *
 *      Register the DnD capability, setup callbacks, initialize.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      mainWnd will be a dragSource in the guest, and dnd will work from
 *      host to guest.
 *
 *-----------------------------------------------------------------------------
 */

Bool
DnD_Register(GtkWidget *mainWnd) // IN: The widget to register as a drag source.
{
   gDragCtx = NULL;
   uint32 i;

   if (DnD_GetVmxDnDVersion() < 2) {
      goto error;
   }

   /*
    * We can't pass in NULL to XTestQueryExtension(), so pass in a dummy
    * variable to avoid segfaults.  If we have a reason to check the major and
    * minor numbers of the running extension, that would go here.
    */
   if (!XTestQueryExtension(GDK_WINDOW_XDISPLAY(mainWnd->window),
                            &i, &i, &i, &i)) {
      goto error;
   }

   /* Host->Guest RPC callbacks */
   RpcIn_RegisterCallback(gRpcIn, "dnd.data.set", DnDRpcInDataSetCB, mainWnd);
   RpcIn_RegisterCallback(gRpcIn, "dnd.enter", DnDRpcInEnterCB, mainWnd);
   RpcIn_RegisterCallback(gRpcIn, "dnd.move", DnDRpcInMoveCB, mainWnd);
   RpcIn_RegisterCallback(gRpcIn, "dnd.drop", DnDRpcInDropCB, mainWnd);
   RpcIn_RegisterCallback(gRpcIn, "dnd.data.finish", DnDRpcInDataFinishCB,
                          mainWnd);

   /* Guest->Host RPC callbacks */
   RpcIn_RegisterCallback(gRpcIn, "dnd.ungrab",
                          DnDRpcInMouseUngrabCB, mainWnd);
   RpcIn_RegisterCallback(gRpcIn, "dnd.data.get.file",
                          DnDRpcInGetNextFileCB, mainWnd);
   RpcIn_RegisterCallback(gRpcIn, "dnd.finish",
                          DnDRpcInFinishCB, mainWnd);

   /*
    * Setup mainWnd as a DND source/dest.
    *
    * Note that G->H drag targets should come first in this array.  Currently
    * G->H only supports text/uri-list targets.
    */
   gTargetEntry[0].target = DRAG_TARGET_NAME_URI_LIST;
   gTargetEntry[0].info = DRAG_TARGET_INFO_URI_LIST;
   gTargetEntry[0].flags = 0;
   gTargetEntry[1].target = DRAG_TARGET_NAME_TEXT_PLAIN;
   gTargetEntry[1].info = DRAG_TARGET_INFO_TEXT_PLAIN;
   gTargetEntry[1].flags = 0;
   gTargetEntry[2].target = DRAG_TARGET_NAME_STRING;
   gTargetEntry[2].info = DRAG_TARGET_INFO_STRING;
   gTargetEntry[2].flags = 0;

   /* Populate our GdkAtom table for our supported Guest->Host targets */
   for (i = 0;
        i < ARRAYSIZE(gTargetEntry) && i < ARRAYSIZE(gTargetEntryAtom);
        i++) {
      gTargetEntryAtom[i] = gdk_atom_intern(gTargetEntry[i].target, FALSE);
   }

   /* Drag source for Host->Guest */
   gtk_drag_source_set(mainWnd, GDK_BUTTON1_MASK,
                       gTargetEntry, ARRAYSIZE(gTargetEntry),
                       GDK_ACTION_COPY | GDK_ACTION_MOVE);

   gtk_signal_connect(GTK_OBJECT(mainWnd), "drag_begin",
                      GTK_SIGNAL_FUNC(DnDGtkBeginCB), mainWnd);
   gtk_signal_connect(GTK_OBJECT(mainWnd), "drag_end",
                      GTK_SIGNAL_FUNC(DnDGtkEndCB), mainWnd);
   gtk_signal_connect(GTK_OBJECT(mainWnd), "drag_data_get",
                      GTK_SIGNAL_FUNC(DnDGtkDataRequestCB), mainWnd);


   /*
    * Drop target (destination) for Guest->Host
    *
    * We provide NR_GH_DRAG_TARGETS (rather than ARRAYSIZE(gTargetEntry)) to
    * gtk_drag_dest_set() since we support less targets for G->H than H->G.
    */
   gtk_drag_dest_set(mainWnd,
                     GTK_DEST_DEFAULT_MOTION,
                     gTargetEntry, NR_GH_DRAG_TARGETS,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);

   gtk_signal_connect(GTK_OBJECT(mainWnd), "drag_motion",
                      GTK_SIGNAL_FUNC(DnDGtkDragMotionCB), mainWnd);
   gtk_signal_connect(GTK_OBJECT(mainWnd), "drag_data_received",
                      GTK_SIGNAL_FUNC(DnDGtkDragDataReceivedCB),
                      mainWnd);
   gtk_signal_connect(GTK_OBJECT(mainWnd), "drag_drop",
                      GTK_SIGNAL_FUNC(DnDGtkDragDropCB), mainWnd);

   DnDHGStateInit();
   DnDGHStateInit(mainWnd);

   if (DnD_RegisterCapability()) {
      return TRUE;
   }

   /*
    * We get here if DnD registration fails for some reason
    */
error:
   DnD_Unregister(mainWnd);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_Unregister --
 *
 *      Cleanup dnd related things.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      DnD is stopped, the rpc channel to the vmx is closed.
 *
 *-----------------------------------------------------------------------------
 */

void
DnD_Unregister(GtkWidget *mainWnd) // IN: program's widget
{
   RpcOut_sendOne(NULL, NULL, "dnd.ready disable");

   DnDGHFileListClear();

   /* Unregister source for Host->Guest DnD. */
   gtk_drag_source_unset(mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(DnDGtkBeginCB),
                                 mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(DnDGtkEndCB),
                                 mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(DnDGtkDataRequestCB),
                                 mainWnd);

   /* Unregister destination for Guest->Host DnD. */
   gtk_drag_dest_unset(mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(DnDGtkDragMotionCB),
                                 mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(DnDGtkDragDataReceivedCB),
                                 mainWnd);
   gtk_signal_disconnect_by_func(GTK_OBJECT(mainWnd),
                                 GTK_SIGNAL_FUNC(DnDGtkDragDropCB),
                                 mainWnd);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_OnReset --
 *
 *    Handles reinitializing DnD state on a reset.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    DnD is stopped and restarted.
 *
 *----------------------------------------------------------------------------
 */

void
DnD_OnReset(GtkWidget *mainWnd) // IN: program's widget
{
   Debug("DnD_OnReset: entry\n");
   /*
    * If a DnD in either direction was in progress during suspend, send an
    * escape to cancel the operation and reset the pointer state.
    */
   if (gHGDnDInProgress || gGHState.dragInProgress) {
      Debug("DnD_OnReset: sending escape\n");
      DnDSendEscapeKey(mainWnd);
   }

   if (gGHState.dragInProgress) {
      Debug("DnD_OnReset: canceling host->guest DnD\n");
      DnDGHCancel(mainWnd);
   }

   /* Reset DnD state. */
   DnDHGStateInit();
   DnDGHStateInit(mainWnd);
   DnDGHFileListClear();
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_InProgress --
 *
 *    Indicates whether a DnD (or its data transfer) is currently in progress.
 *
 * Results:
 *    TRUE if a DnD is in progress, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnD_InProgress(void)
{
   return gGHState.dragInProgress || gHGDnDInProgress || gHGDataPending;
}
