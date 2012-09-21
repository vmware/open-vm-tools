/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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

/**
 * @file dndUIX11.cpp --
 *
 * This class implements stubs for the methods that allow DnD between
 * host and guest.
 */

#define G_LOG_DOMAIN "dndcp"

#include "dndUIX11.h"
#include "guestDnDCPMgr.hh"

extern "C" {
#include "vmblock.h"
#include "file.h"
#include "copyPasteCompat.h"
#include "dnd.h"
#include "dndMsg.h"
#include "dndClipboard.h"
#include "cpName.h"
#include "cpNameUtil.h"
#include "hostinfo.h"
#include "rpcout.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/XTest.h>       /* for XTest*() */
#include "vmware/guestrpc/tclodefs.h"
}

/* IsXExtensionPointer may be not defined with old Xorg. */
#ifndef IsXExtensionPointer
#define IsXExtensionPointer 4
#endif

#include "copyPasteDnDWrapper.h"

/**
 *
 * Constructor.
 */

DnDUIX11::DnDUIX11(ToolsAppCtx *ctx)
    : m_ctx(ctx),
      m_DnD(NULL),
      m_detWnd(NULL),
      m_blockCtrl(NULL),
      m_HGGetFileStatus(DND_FILE_TRANSFER_NOT_STARTED),
      m_blockAdded(false),
      m_GHDnDInProgress(false),
      m_GHDnDDataReceived(false),
      m_unityMode(false),
      m_inHGDrag(false),
      m_effect(DROP_NONE),
      m_mousePosX(0),
      m_mousePosY(0),
      m_dc(NULL),
      m_numPendingRequest(0),
      m_destDropTime(0),
      mTotalFileSize(0)
{
   g_debug("%s: enter\n", __FUNCTION__);
}


/**
 *
 * Destructor.
 */

DnDUIX11::~DnDUIX11()
{
   g_debug("%s: enter\n", __FUNCTION__);
   if (m_detWnd) {
      delete m_detWnd;
   }
   CPClipboard_Destroy(&m_clipboard);
   /* Any files from last unfinished file transfer should be deleted. */
   if (DND_FILE_TRANSFER_IN_PROGRESS == m_HGGetFileStatus &&
       !m_HGStagingDir.empty()) {
      uint64 totalSize = File_GetSizeEx(m_HGStagingDir.c_str());
      if (mTotalFileSize != totalSize) {
         g_debug("%s: deleting %s, expecting %"FMT64"d, finished %"FMT64"d\n",
                 __FUNCTION__, m_HGStagingDir.c_str(),
                 mTotalFileSize, totalSize);
         DnD_DeleteStagingFiles(m_HGStagingDir.c_str(), FALSE);
      } else {
         g_debug("%s: file size match %s\n",
                 __FUNCTION__, m_HGStagingDir.c_str());
      }
   }
   CommonResetCB();
}


/**
 *
 * Initialize DnDUIX11 object.
 */

bool
DnDUIX11::Init()
{
   g_debug("%s: enter\n", __FUNCTION__);
   bool ret = true;

   CPClipboard_Init(&m_clipboard);

   GuestDnDCPMgr *p = GuestDnDCPMgr::GetInstance();
   ASSERT(p);
   m_DnD = p->GetDnDMgr();
   ASSERT(m_DnD);

   m_detWnd = new DragDetWnd();
   if (!m_detWnd) {
      g_debug("%s: unable to allocate DragDetWnd object\n", __FUNCTION__);
      goto fail;
   }

#if defined(DETWNDDEBUG)

   /*
    * This code can only be called when DragDetWnd is derived from
    * Gtk::Window. The normal case is that DragDetWnd is an instance of
    * Gtk::Invisible, which doesn't implement the methods that SetAttributes
    * relies upon.
    */

   m_detWnd->SetAttributes();
#endif

   SetTargetsAndCallbacks();

   /* Set common layer callbacks. */
   m_DnD->srcDragBeginChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonDragStartCB));
   m_DnD->srcDropChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonSourceDropCB));
   m_DnD->srcCancelChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonSourceCancelCB));
   m_DnD->getFilesDoneChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonSourceFileCopyDoneCB));

   m_DnD->destCancelChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonDestCancelCB));
   m_DnD->privDropChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonDestPrivateDropCB));

   m_DnD->updateDetWndChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonUpdateDetWndCB));
   m_DnD->moveMouseChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonUpdateMouseCB));

   m_DnD->updateUnityDetWndChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonUpdateUnityDetWndCB));
   m_DnD->destMoveDetWndToMousePosChanged.connect(
      sigc::mem_fun(this, &DnDUIX11::CommonMoveDetWndToMousePos));
   /* Set Gtk+ callbacks for source. */
   m_detWnd->signal_drag_begin().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkSourceDragBeginCB));
   m_detWnd->signal_drag_data_get().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkSourceDragDataGetCB));
   m_detWnd->signal_drag_end().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkSourceDragEndCB));

   m_detWnd->signal_enter_notify_event().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkEnterEventCB));
   m_detWnd->signal_leave_notify_event().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkLeaveEventCB));
   m_detWnd->signal_map_event().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkMapEventCB));
   m_detWnd->signal_unmap_event().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkUnmapEventCB));
   m_detWnd->signal_realize().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkRealizeEventCB));
   m_detWnd->signal_unrealize().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkUnrealizeEventCB));
   m_detWnd->signal_motion_notify_event().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkMotionNotifyEventCB));
   m_detWnd->signal_configure_event().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkConfigureEventCB));
   m_detWnd->signal_button_press_event().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkButtonPressEventCB));
   m_detWnd->signal_button_release_event().connect(
      sigc::mem_fun(this, &DnDUIX11::GtkButtonReleaseEventCB));

   CommonUpdateDetWndCB(false, 0, 0);
   CommonUpdateUnityDetWndCB(false, 0, false);
   goto out;
fail:
   ret = false;
   if (m_DnD) {
      delete m_DnD;
      m_DnD = NULL;
   }
   if (m_detWnd) {
      delete m_detWnd;
      m_detWnd = NULL;
   }
out:
   return ret;
}


/**
 *
 * Setup targets we support, claim ourselves as a drag destination, and
 * register callbacks for Gtk+ drag and drop callbacks the platform will
 * send to us.
 */

void
DnDUIX11::SetTargetsAndCallbacks()
{
   g_debug("%s: enter\n", __FUNCTION__);

   /* Construct supported target list for HG DnD. */
   std::list<Gtk::TargetEntry> targets;

   /* File DnD. */
   targets.push_back(Gtk::TargetEntry(DRAG_TARGET_NAME_URI_LIST));

   /* RTF text DnD. */
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_APPLICATION_RTF));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_TEXT_RICHTEXT));

   /* Plain text DnD. */
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_UTF8_STRING));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_STRING));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_TEXT_PLAIN));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_COMPOUND_TEXT));

   /*
    * We don't want Gtk handling any signals for us, we want to
    * do it ourselves based on the results from the guest.
    *
    * Second argument in drag_dest_set defines the automatic behaviour options
    * of the destination widget. We used to not define it (0) and in some
    * distributions (like Ubuntu 6.10) DragMotion only get called once,
    * and not send updated mouse position to guest, and also got cancel
    * signal when user drop the file (bug 175754). With flag DEST_DEFAULT_MOTION
    * the bug is fixed. Almost all other example codes use DEST_DEFAULT_ALL
    * but in our case, we will call drag_get_data during DragMotion, and
    * will cause X dead with DEST_DEFAULT_ALL. The reason is unclear.
    */
   m_detWnd->drag_dest_set(targets, Gtk::DEST_DEFAULT_MOTION,
                           Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
   m_detWnd->signal_drag_leave().connect(sigc::mem_fun(this, &DnDUIX11::GtkDestDragLeaveCB));
   m_detWnd->signal_drag_motion().connect(sigc::mem_fun(this, &DnDUIX11::GtkDestDragMotionCB));
   m_detWnd->signal_drag_drop().connect(sigc::mem_fun(this, &DnDUIX11::GtkDestDragDropCB));
   m_detWnd->signal_drag_data_received().connect(sigc::mem_fun(this, &DnDUIX11::GtkDestDragDataReceivedCB));
}

/* Begin of callbacks issued by common layer code */

/**
 *
 * Reset Callback to reset dnd ui state.
 */

void
DnDUIX11::CommonResetCB(void)
{
   g_debug("%s: entering\n", __FUNCTION__);
   m_GHDnDDataReceived = false;
   m_HGGetFileStatus = DND_FILE_TRANSFER_NOT_STARTED;
   m_GHDnDInProgress = false;
   m_effect = DROP_NONE;
   m_inHGDrag = false;
   m_dc = NULL;
   RemoveBlock();
}


/* Source functions for HG DnD. */

/**
 *
 * Called when host successfully detected a pending HG drag.
 *
 * param[in] clip cross-platform clipboard
 * param[in] stagingDir associated staging directory
 */

void
DnDUIX11::CommonDragStartCB(const CPClipboard *clip, std::string stagingDir)
{
   Glib::RefPtr<Gtk::TargetList> targets;
   Gdk::DragAction actions;
   GdkEventMotion event;

   CPClipboard_Clear(&m_clipboard);
   CPClipboard_Copy(&m_clipboard, clip);

   g_debug("%s: enter\n", __FUNCTION__);

   /*
    * Before the DnD, we should make sure that the mouse is released
    * otherwise it may be another DnD, not ours. Send a release, then
    * a press here to cover this case.
    */
   SendFakeXEvents(false, true, false, false, false, 0, 0);
   SendFakeXEvents(true, true, true, false, true, 0, 0);

   /*
    * Construct the target and action list, as well as a fake motion notify
    * event that's consistent with one that would typically start a drag.
    */
   targets = Gtk::TargetList::create(std::list<Gtk::TargetEntry>());

   if (CPClipboard_ItemExists(&m_clipboard, CPFORMAT_FILELIST)) {
      m_HGStagingDir = stagingDir;
      if (!m_HGStagingDir.empty()) {
         targets->add(Glib::ustring(DRAG_TARGET_NAME_URI_LIST));
         /* Add private data to tag dnd as originating from this vm. */
         char *pid;
         g_debug("%s: adding re-entrant drop target, pid %d\n", __FUNCTION__, (int)getpid());
         pid = Str_Asprintf(NULL, "guest-dnd-target %d", static_cast<int>(getpid()));
         if (pid) {
            targets->add(Glib::ustring(pid));
            free(pid);
         }
      }
   }

   if (CPClipboard_ItemExists(&m_clipboard, CPFORMAT_FILECONTENTS)) {
      if (WriteFileContentsToStagingDir()) {
         targets->add(Glib::ustring(DRAG_TARGET_NAME_URI_LIST));
      }
   }

   if (CPClipboard_ItemExists(&m_clipboard, CPFORMAT_TEXT)) {
      targets->add(Glib::ustring(TARGET_NAME_STRING));
      targets->add(Glib::ustring(TARGET_NAME_TEXT_PLAIN));
      targets->add(Glib::ustring(TARGET_NAME_UTF8_STRING));
      targets->add(Glib::ustring(TARGET_NAME_COMPOUND_TEXT));
   }

   if (CPClipboard_ItemExists(&m_clipboard, CPFORMAT_RTF)) {
      targets->add(Glib::ustring(TARGET_NAME_APPLICATION_RTF));
      targets->add(Glib::ustring(TARGET_NAME_TEXT_RICHTEXT));
   }

   actions = Gdk::ACTION_COPY | Gdk::ACTION_MOVE;

   /* TODO set the x/y coords to the actual drag initialization point. */
   event.type = GDK_MOTION_NOTIFY;
   event.window = m_detWnd->get_window()->gobj();
   event.send_event = false;
   event.time = GDK_CURRENT_TIME;
   event.x = 10;
   event.y = 10;
   event.axes = NULL;
   event.state = GDK_BUTTON1_MASK;
   event.is_hint = 0;
   event.device = gdk_device_get_core_pointer();
   event.x_root = 0;
   event.y_root = 5;

   /* Tell Gtk that a drag should be started from this widget. */
   m_detWnd->drag_begin(targets, actions, 1, (GdkEvent *)&event);
   m_blockAdded = false;
   m_HGGetFileStatus = DND_FILE_TRANSFER_NOT_STARTED;
   SourceDragStartDone();
   /* Initialize host hide feedback to DROP_NONE. */
   m_effect = DROP_NONE;
   SourceUpdateFeedback(m_effect);
}


/**
 *
 * Cancel current HG DnD.
 */

void
DnDUIX11::CommonSourceCancelCB(void)
{
   g_debug("%s: entering\n", __FUNCTION__);

   /*
    * Force the window to show, position the mouse over it, and release.
    * Seems like moving the window to 0, 0 eliminates frequently observed
    * flybacks when we cancel as user moves mouse in and out of destination
    * window in a H->G DnD.
    */
   CommonUpdateDetWndCB(true, 0, 0);
   SendFakeXEvents(true, true, false, true, true, 0, 0);
   CommonUpdateDetWndCB(false, 0, 0);
   m_inHGDrag = false;
   m_HGGetFileStatus = DND_FILE_TRANSFER_NOT_STARTED;
   m_effect = DROP_NONE;
   RemoveBlock();
}


/**
 *
 * Handle common layer private drop CB.
 *
 * @param[in] x position to release the mouse button (ignored).
 * @param[in] y position to release the mouse button (ignored).
 *
 * @note We ignore the coordinates, because we just need to release the mouse
 * in its current position.
 */

void
DnDUIX11::CommonDestPrivateDropCB(int32 x,
                                  int32 y)
{
   g_debug("%s: entering\n", __FUNCTION__);
   /* Unity manager in host side may already send the drop into guest. */
   if (m_GHDnDInProgress) {

      /*
       * Release the mouse button.
       */
      SendFakeXEvents(false, true, false, false, false, 0, 0);
   }
   CommonResetCB();
}


/**
 *
 * Cancel current DnD (G->H only).
 */

void
DnDUIX11::CommonDestCancelCB(void)
{
   g_debug("%s: entering\n", __FUNCTION__);
   /* Unity manager in host side may already send the drop into guest. */
   if (m_GHDnDInProgress) {
      CommonUpdateDetWndCB(true, 0, 0);

      /*
       * Show the window, move it to the mouse position, and release the
       * mouse button.
       */
      SendFakeXEvents(true, true, false, true, false, 0, 0);
   }
   m_destDropTime = GetTimeInMillis();
   CommonResetCB();
}


/**
 *
 * Got drop from host side. Release the mouse button in the detection window
 */

void
DnDUIX11::CommonSourceDropCB(void)
{
   g_debug("%s: enter\n", __FUNCTION__);
   CommonUpdateDetWndCB(true, 0, 0);

   /*
    * Move the mouse to the saved coordinates, and release the mouse button.
    */
   SendFakeXEvents(false, true, false, false, true, m_mousePosX, m_mousePosY);
   CommonUpdateDetWndCB(false, 0, 0);
}


/**
 *
 * Callback when file transfer is done, which finishes the file
 * copying from host to guest staging directory.
 *
 * @param[in] success if true, transfer was successful
 */

void
DnDUIX11::CommonSourceFileCopyDoneCB(bool success)
{
   g_debug("%s: %s\n", __FUNCTION__, success ? "success" : "failed");

   /*
    * If hg drag is not done yet, only remove block. GtkSourceDragEndCB will
    * call CommonResetCB(). Otherwise destination may miss the data because
     * we are already reset.
    */

   m_HGGetFileStatus = DND_FILE_TRANSFER_FINISHED;

   if (!m_inHGDrag) {
      CommonResetCB();
   } else {
      RemoveBlock();
   }
}


/**
 *
 * Shows/hides drag detection windows based on the mask.
 *
 * @param[in] bShow if true, show the window, else hide it.
 * @param[in] x x-coordinate to which the detection window needs to be moved
 * @param[in] y y-coordinate to which the detection window needs to be moved
 */

void
DnDUIX11::CommonUpdateDetWndCB(bool bShow,
                               int32 x,
                               int32 y)
{
   g_debug("%s: enter 0x%lx show %d x %d y %d\n",
         __FUNCTION__,
         (unsigned long) m_detWnd->get_window()->gobj(), bShow, x, y);

   /* If the window is being shown, move it to the right place. */
   if (bShow) {
      x = MAX(x - DRAG_DET_WINDOW_WIDTH / 2, 0);
      y = MAX(y - DRAG_DET_WINDOW_WIDTH / 2, 0);

      m_detWnd->Show();
      m_detWnd->Raise();
      m_detWnd->SetGeometry(x, y, DRAG_DET_WINDOW_WIDTH * 2, DRAG_DET_WINDOW_WIDTH * 2);
      g_debug("%s: show at (%d, %d, %d, %d)\n", __FUNCTION__, x, y, DRAG_DET_WINDOW_WIDTH * 2, DRAG_DET_WINDOW_WIDTH * 2);
      /*
       * Wiggle the mouse here. Especially for G->H DnD, this improves
       * reliability of making the drag escape the guest window immensly.
       * Stolen from the legacy V2 DnD code.
       */

      SendFakeMouseMove(x + 2, y + 2);
      m_detWnd->SetIsVisible(true);
   } else {
      g_debug("%s: hide\n", __FUNCTION__);
      m_detWnd->Hide();
      m_detWnd->SetIsVisible(false);
   }
}


/**
 *
 * Shows/hides full-screen Unity drag detection window.
 *
 * @param[in] bShow if true, show the window, else hide it.
 * @param[in] unityWndId active front window
 * @param[in] bottom if true, adjust the z-order to be bottom most.
 */

void
DnDUIX11::CommonUpdateUnityDetWndCB(bool bShow,
                                    uint32 unityWndId,
                                    bool bottom)
{
   g_debug("%s: enter 0x%lx unityID 0x%x\n",
         __FUNCTION__,
         (unsigned long) m_detWnd->get_window()->gobj(),
         unityWndId);
   if (bShow && ((unityWndId > 0) || bottom)) {
      int width = m_detWnd->GetScreenWidth();
      int height = m_detWnd->GetScreenHeight();
      m_detWnd->SetGeometry(0, 0, width, height);
      m_detWnd->Show();
      if (bottom) {
         m_detWnd->Lower();
      }

      g_debug("%s: show, (0, 0, %d, %d)\n", __FUNCTION__, width, height);
   } else {
      if (m_detWnd->GetIsVisible() == true) {
         if (m_unityMode) {

            /*
             * Show and move detection window to current mouse position
             * and resize.
             */
            SendFakeXEvents(true, false, true, true, false, 0, 0);
         }
      } else {
         m_detWnd->Hide();
         g_debug("%s: hide\n", __FUNCTION__);
      }
   }
}


/**
 *
 * Move detection windows to current cursor position.
 */

void
DnDUIX11::CommonMoveDetWndToMousePos(void)
{
   SendFakeXEvents(true, false, true, true, false, 0, 0);
}


/**
 *
 * Handle request from common layer to update mouse position.
 *
 * @param[in] x x coordinate of pointer
 * @param[in] y y coordinate of pointer
 */

void
DnDUIX11::CommonUpdateMouseCB(int32 x,
                              int32 y)
{
   // Position the pointer, and record its position.

   SendFakeXEvents(false, false, false, false, true, x, y);
   m_mousePosX = x;
   m_mousePosY = y;

   if (m_dc && !m_GHDnDInProgress) {

      // If we are the context of a DnD, send DnD feedback to the source.

      DND_DROPEFFECT effect;
      effect = ToDropEffect((Gdk::DragAction)(m_dc->action));
      if (effect != m_effect) {
         m_effect = effect;
         g_debug("%s: Updating feedback\n", __FUNCTION__);
         SourceUpdateFeedback(m_effect);
      }
   }
}

/* Beginning of Gtk+ Callbacks */

/*
 * Source callbacks from Gtk+. Most are seen only when we are acting as a
 * drag source.
 */

/**
 *
 * "drag_motion" signal handler for GTK. We should respond by setting drag
 * status. Note that there is no drag enter signal. We need to figure out
 * if a new drag is happening on our own. Also, we don't respond with a
 * "allowed" drag status right away, we start a new drag operation over VMDB
 * (which tries to notify the host of the new operation). Once the host has
 * responded), we respond with a proper drag status.
 *
 * @param[in] dc associated drag context
 * @param[in] x x coordinate of the drag motion
 * @param[in] y y coordinate of the drag motion
 * @param[in] time time of the drag motion
 *
 * @return returning false means we won't get notified of future motion. So,
 * we only return false if we don't recognize the types being offered. We
 * return true otherwise, even if we don't accept the drag right now for some
 * other reason.
 *
 * @note you may see this callback during DnD when detection window is acting
 * as a source. In that case it will be ignored. In a future refactoring,
 * we will try and avoid this.
 */

bool
DnDUIX11::GtkDestDragMotionCB(const Glib::RefPtr<Gdk::DragContext> &dc,
                              int x,
                              int y,
                              guint timeValue)
{
   /*
    * If this is a Host to Guest drag, we are done here, so return.
    */
   unsigned long curTime = GetTimeInMillis();
   g_debug("%s: enter dc %p, m_dc %p\n", __FUNCTION__,
         dc ? dc->gobj() : NULL, m_dc ? m_dc : NULL);
   if (curTime - m_destDropTime <= 1000) {
      g_debug("%s: ignored %ld %ld %ld\n", __FUNCTION__,
            curTime, m_destDropTime, curTime - m_destDropTime);
      return true;
   }

   g_debug("%s: not ignored %ld %ld %ld\n", __FUNCTION__,
         curTime, m_destDropTime, curTime - m_destDropTime);

   if (m_inHGDrag || (m_HGGetFileStatus != DND_FILE_TRANSFER_NOT_STARTED)) {
      g_debug("%s: ignored not in hg drag or not getting hg data\n", __FUNCTION__);
      return true;
   }

   Gdk::DragAction srcActions;
   Gdk::DragAction suggestedAction;
   Gdk::DragAction dndAction = (Gdk::DragAction)0;
   Glib::ustring target = m_detWnd->drag_dest_find_target(dc);

   if (!m_DnD->IsDnDAllowed()) {
      g_debug("%s: No dnd allowed!\n", __FUNCTION__);
      dc->drag_status(dndAction, timeValue);
      return true;
   }

   /* Check if dnd began from this vm. */

   /*
    * TODO: Once we upgrade to shipping gtkmm 2.12, we can go back to
    *       Gdk::DragContext::get_targets, but API/ABI broke between 2.10 and
    *       2.12, so we work around it like this for now.
    */
   Glib::ListHandle<std::string, Gdk::AtomStringTraits> targets(
      dc->gobj()->targets, Glib::OWNERSHIP_NONE);

   std::list<Glib::ustring> as = targets;
   std::list<Glib::ustring>::iterator result;
   char *pid;
   pid = Str_Asprintf(NULL, "guest-dnd-target %d", static_cast<int>(getpid()));
   if (pid) {
      result = std::find(as.begin(), as.end(), std::string(pid));
      free(pid);
   } else {
      result = as.end();
   }
   if (result != as.end()) {
      g_debug("%s: found re-entrant drop target, pid %s\n", __FUNCTION__, pid );
      return true;
   }

   m_dc = dc->gobj();

   if (target != "") {
      /*
       * We give preference to the suggested action from the source, and prefer
       * copy over move.
       */
      suggestedAction = dc->get_suggested_action();
      srcActions = dc->get_actions();
      if (suggestedAction == Gdk::ACTION_COPY || suggestedAction == Gdk::ACTION_MOVE) {
         dndAction = suggestedAction;
      } else if (srcActions & Gdk::ACTION_COPY) {
         dndAction= Gdk::ACTION_COPY;
      } else if (srcActions & Gdk::ACTION_MOVE) {
         dndAction = Gdk::ACTION_MOVE;
      } else {
         dndAction = (Gdk::DragAction)0;
      }
   } else {
      dndAction = (Gdk::DragAction)0;
   }

   if (dndAction != (Gdk::DragAction)0) {
      dc->drag_status(dndAction, timeValue);
      if (!m_GHDnDInProgress) {
         g_debug("%s: new drag, need to get data for host\n", __FUNCTION__);
         /*
          * This is a new drag operation. We need to start a drag thru the
          * backdoor, and to the host. Before we can tell the host, we have to
          * retrieve the drop data.
          */
         m_GHDnDInProgress = true;
         /* only begin drag enter after we get the data */
         /* Need to grab all of the data. */
         if (!RequestData(dc, timeValue)) {
            g_debug("%s: RequestData failed.\n", __FUNCTION__);
            return false;
         }
      } else {
         g_debug("%s: Multiple drag motions before gh data has been received.\n",
               __FUNCTION__);
      }
   } else {
      g_debug("%s: Invalid drag\n", __FUNCTION__);
      return false;
   }
   return true;
}


/**
 *
 * "drag_leave" signal handler for GTK. Log the reception of this signal,
 * but otherwise unhandled in our implementation.
 *
 *  @param[in] dc drag context
 *  @param[in] time time of the drag
 */

void
DnDUIX11::GtkDestDragLeaveCB(const Glib::RefPtr<Gdk::DragContext> &dc,
                             guint time)
{
   g_debug("%s: enter dc %p, m_dc %p\n", __FUNCTION__,
         dc ? dc->gobj() : NULL, m_dc ? m_dc : NULL);

   /*
    * If we reach here after reset DnD, or we are getting a late
    * DnD drag leave signal (we have started another DnD), then
    * finish the old DnD. Otherwise, Gtk will not reset and a new
    * DnD will not start until Gtk+ times out (which appears to
    * be 5 minutes).
    * See http://bugzilla.eng.vmware.com/show_bug.cgi?id=528320
    */
   if (!m_dc || dc->gobj() != m_dc) {
      g_debug("%s: calling drag_finish\n", __FUNCTION__);
      dc->drag_finish(true, false, time);
   }
}


/*
 * Gtk+ callbacks that are seen when we are a drag source.
 */

/**
 *
 * "drag_begin" signal handler for GTK.
 *
 * @param[in] context drag context
 */

void
DnDUIX11::GtkSourceDragBeginCB(const Glib::RefPtr<Gdk::DragContext>& context)
{
   g_debug("%s: enter dc %p, m_dc %p\n", __FUNCTION__,
         context ? context->gobj() : NULL, m_dc ? m_dc : NULL);
   m_dc = context->gobj();
}


/**
 *
 * "drag_data_get" handler for GTK. We don't send drop until we are done.
 *
 * @param[in] dc drag state
 * @param[in] selection_data buffer for data
 * @param[in] info unused
 * @param[in] time timestamp
 *
 * @note if the drop has occurred, the files are copied from the guest.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::GtkSourceDragDataGetCB(const Glib::RefPtr<Gdk::DragContext> &dc,
                                 Gtk::SelectionData& selection_data,
                                 guint info,
                                 guint time)
{
   size_t index = 0;
   std::string str;
   std::string uriList;
   std::string stagingDirName;
   void *buf;
   size_t sz;
   utf::utf8string hgData;
   DnDFileList fList;
   std::string pre;
   std::string post;

   const utf::string target = selection_data.get_target().c_str();

   selection_data.set(target.c_str(), "");

   g_debug("%s: enter dc %p, m_dc %p with target %s\n", __FUNCTION__,
         dc ? dc->gobj() : NULL, m_dc ? m_dc : NULL,
         target.c_str());

   if (!m_inHGDrag) {
      g_debug("%s: not in drag, return\n", __FUNCTION__);
      return;
   }

   if (target == DRAG_TARGET_NAME_URI_LIST &&
       CPClipboard_GetItem(&m_clipboard, CPFORMAT_FILELIST, &buf, &sz)) {

      /* Provide path within vmblock file system instead of actual path. */
      stagingDirName = GetLastDirName(m_HGStagingDir);
      if (stagingDirName.length() == 0) {
         g_debug("%s: Cannot get staging directory name, stagingDir: %s\n",
               __FUNCTION__, m_HGStagingDir.c_str());
         return;
      }

      if (!fList.FromCPClipboard(buf, sz)) {
         g_debug("%s: Can't get data from clipboard\n", __FUNCTION__);
         return;
      }

      mTotalFileSize = fList.GetFileSize();

      /* Provide URIs for each path in the guest's file list. */
      if (FCP_TARGET_INFO_GNOME_COPIED_FILES == info) {
         pre = FCP_GNOME_LIST_PRE;
         post = FCP_GNOME_LIST_POST;
      } else if (FCP_TARGET_INFO_URI_LIST == info) {
         pre = DND_URI_LIST_PRE_KDE;
         post = DND_URI_LIST_POST;
      } else {
         g_debug("%s: Unknown request target: %s\n", __FUNCTION__,
               selection_data.get_target().c_str());
         return;
      }

      /* Provide path within vmblock file system instead of actual path. */
      hgData = fList.GetRelPathsStr();

      /* Provide URIs for each path in the guest's file list. */
      while ((str = GetNextPath(hgData, index).c_str()).length() != 0) {
         uriList += pre;
         if (DnD_BlockIsReady(m_blockCtrl)) {
            uriList += m_blockCtrl->blockRoot;
            uriList += DIRSEPS + stagingDirName + DIRSEPS + str + post;
         } else {
            uriList += DIRSEPS + m_HGStagingDir + DIRSEPS + str + post;
         }
      }

      /*
       * This seems to be the best place to do the blocking. If we do
       * it in the source drop callback from the DnD layer, we often
       * find ourselves adding the block too late; the user will (in
       * GNOME, in the dest) be told that it could not find the file,
       * and if you click retry, it is there, meaning the block was
       * added too late).
       *
       * We find ourselves in this callback twice for each H->G DnD.
       * We *must* always set the selection data, when called, or else
       * the DnD for that context will fail, but we *must not* add the
       * block twice or else things get confused. So we add a check to
       * see if we are in the right state (no block yet added, and we
       * are in a HG drag still, both must be true) when adding the block.
       * Doing both of these addresses bug
       * http://bugzilla.eng.vmware.com/show_bug.cgi?id=391661.
       */
      if (!m_blockAdded &&
          m_inHGDrag &&
          (m_HGGetFileStatus == DND_FILE_TRANSFER_NOT_STARTED)) {
         m_HGGetFileStatus = DND_FILE_TRANSFER_IN_PROGRESS;
         AddBlock();
      } else {
         g_debug("%s: not calling AddBlock\n", __FUNCTION__);
      }
      selection_data.set(DRAG_TARGET_NAME_URI_LIST, uriList.c_str());
      g_debug("%s: providing uriList [%s]\n", __FUNCTION__, uriList.c_str());
      return;
   }

   if (target == DRAG_TARGET_NAME_URI_LIST &&
       CPClipboard_ItemExists(&m_clipboard, CPFORMAT_FILECONTENTS)) {
      g_debug("%s: Providing uriList [%s] for file contents DnD\n",
            __FUNCTION__, m_HGFileContentsUriList.c_str());

      selection_data.set(DRAG_TARGET_NAME_URI_LIST,
                         m_HGFileContentsUriList.c_str());
      return;
   }

   if ((target == TARGET_NAME_STRING ||
        target == TARGET_NAME_TEXT_PLAIN ||
        target == TARGET_NAME_UTF8_STRING ||
        target == TARGET_NAME_COMPOUND_TEXT) &&
       CPClipboard_GetItem(&m_clipboard, CPFORMAT_TEXT, &buf, &sz)) {
      g_debug("%s: providing plain text, size %"FMTSZ"u\n", __FUNCTION__, sz);
      selection_data.set(target.c_str(), (const char *)buf);
      return;
   }

   if ((target == TARGET_NAME_APPLICATION_RTF ||
        target == TARGET_NAME_TEXT_RICHTEXT) &&
       CPClipboard_GetItem(&m_clipboard, CPFORMAT_RTF, &buf, &sz)) {
      g_debug("%s: providing rtf text, size %"FMTSZ"u\n", __FUNCTION__, sz);
      selection_data.set(target.c_str(), (const char *)buf);
      return;
   }

   /* Can not get any valid data, cancel this HG DnD. */
   g_debug("%s: no valid data for HG DnD\n", __FUNCTION__);
   CommonResetCB();
}


/**
 *
 * "drag_end" handler for GTK. Received by drag source.
 *
 * @param[in] dc drag state
 */

void
DnDUIX11::GtkSourceDragEndCB(const Glib::RefPtr<Gdk::DragContext> &dc)
{
   g_debug("%s: entering dc %p, m_dc %p\n", __FUNCTION__,
         dc ? dc->gobj() : NULL, m_dc ? m_dc : NULL);

   /*
    * We may see a drag end for the previous DnD, but after a new
    * DnD has started. If so, ignore it.
    */
   if (m_dc && dc && (m_dc != dc->gobj())) {
      g_debug("%s: got old dc (new DnD started), ignoring\n", __FUNCTION__);
      return;
   }

   /*
    * If we are a file DnD and file transfer is not done yet, don't call
    * CommonResetCB() here, since we will do so in the fileCopyDoneChanged
    * callback.
    */
   if (DND_FILE_TRANSFER_IN_PROGRESS != m_HGGetFileStatus) {
      CommonResetCB();
   }
   m_inHGDrag = false;
}

/* Gtk+ callbacks seen when we are a drag destination. */

/**
 *
 * "drag_data_received" signal handler for GTK. We requested the drag
 * data earlier from some drag source on the guest; this is the response.
 *
 * This is for G->H DnD.
 *
 * @param[in] dc drag context
 * @param[in] x where the drop happened
 * @param[in] y where the drop happened
 * @param[in] sd the received data
 * @param[in] info the info that has been registered with the target in the
 * target list.
 * @param[in] time the timestamp at which the data was received.
 */

void
DnDUIX11::GtkDestDragDataReceivedCB(const Glib::RefPtr<Gdk::DragContext> &dc,
                                    int x,
                                    int y,
                                    const Gtk::SelectionData& sd,
                                    guint info,
                                    guint time)
{
   g_debug("%s: enter dc %p, m_dc %p\n", __FUNCTION__,
         dc ? dc->gobj() : NULL, m_dc ? m_dc : NULL);
   /* The GH DnD may already finish before we got response. */
   if (!m_GHDnDInProgress) {
      g_debug("%s: not valid\n", __FUNCTION__);
      return;
   }

   /*
    * Try to get data provided from the source.  If we cannot get any data,
    * there is no need to inform the guest of anything. If there is no data,
    * reset, so that the next drag_motion callback that we see will be allowed
    * to request data again.
    */
   if (SetCPClipboardFromGtk(sd) == false) {
      g_debug("%s: Failed to set CP clipboard.\n", __FUNCTION__);
      CommonResetCB();
      return;
   }

   m_numPendingRequest--;
   if (m_numPendingRequest > 0) {
      return;
   }

   if (CPClipboard_IsEmpty(&m_clipboard)) {
      g_debug("%s: Failed getting item.\n", __FUNCTION__);
      CommonResetCB();
      return;
   }

   /*
    * There are two points in the DnD process at which this is called, and both
    * are in response to us calling drag_data_get().  The first occurs on the
    * first "drag_motion" received and is used to start a drag; at that point
    * we need to provide the file list to the guest so we request the data from
    * the target.  The second occurs when the "drag_drop" signal is received
    * and we confirm this data with the target before starting the drop.
    *
    * Note that we prevent against sending multiple "dragStart"s or "drop"s for
    * each DnD.
    */
   if (!m_GHDnDDataReceived) {
      g_debug("%s: Drag entering.\n", __FUNCTION__);
      m_GHDnDDataReceived = true;
      TargetDragEnter();
   } else {
      g_debug("%s: not !m_GHDnDDataReceived\n", __FUNCTION__);
   }
}


/**
 *
 * "drag_drop" signal handler for GTK. Send the drop to the host (by
 * way of the backdoor), then tell the host to get the files.
 *
 * @param[in] dc drag context
 * @param[in] x x location of the drop
 * @param[in] y y location of the drop
 * @param[in] time timestamp for the drop
 *
 * @return true on success, false otherwise.
 */

bool
DnDUIX11::GtkDestDragDropCB(const Glib::RefPtr<Gdk::DragContext> &dc,
                            int x,
                            int y,
                            guint time)
{
   g_debug("%s: enter dc %p, m_dc %p x %d y %d\n", __FUNCTION__,
         (dc ? dc->gobj() : NULL), (m_dc ? m_dc : NULL), x, y);

   Glib::ustring target;

   target = m_detWnd->drag_dest_find_target(dc);
   g_debug("%s: calling drag_finish\n", __FUNCTION__);
   dc->drag_finish(true, false, time);

   if (target == "") {
      g_debug("%s: No valid data on clipboard.\n", __FUNCTION__);
      return false;
   }

   if (CPClipboard_IsEmpty(&m_clipboard)) {
      g_debug("%s: No valid data on m_clipboard.\n", __FUNCTION__);
      return false;
   }

   return true;
}

/* General utility functions */

/**
 *
 * Try to construct cross-platform clipboard data from selection data
 * provided to us by Gtk+.
 *
 * @param[in] sd Gtk selection data to convert to CP clipboard data
 *
 * @return false on failure, true on success
 */

bool
DnDUIX11::SetCPClipboardFromGtk(const Gtk::SelectionData& sd) // IN
{
   char *newPath;
   char *newRelPath;
   size_t newPathLen;
   size_t index = 0;
   DnDFileList fileList;
   DynBuf buf;
   uint64 totalSize = 0;
   int64 size;

   const utf::string target = sd.get_target().c_str();

   /* Try to get file list. */
   if (m_DnD->CheckCapability(DND_CP_CAP_FILE_DND) && target == DRAG_TARGET_NAME_URI_LIST) {
      /*
       * Turn the uri list into two \0  delimited lists. One for full paths and
       * one for just the last path component.
       */
      utf::string source = sd.get_data_as_string().c_str();
      g_debug("%s: Got file list: [%s]\n", __FUNCTION__, source.c_str());

      if (sd.get_data_as_string().length() == 0) {
         g_debug("%s: empty file list!\n", __FUNCTION__);
         return false;
      }

      /*
       * In gnome, before file list there may be a extra line indicating it
       * is a copy or cut.
       */
      if (source.length() >= 5 && source.compare(0, 5, "copy\n") == 0) {
         source = source.erase(0, 5);
      }

      if (source.length() >= 4 && source.compare(0, 4, "cut\n") == 0) {
         source = source.erase(0, 4);
      }

      while (source.length() > 0 &&
             (source[0] == '\n' || source[0] == '\r' || source[0] == ' ')) {
         source = source.erase(0, 1);
      }

      while ((newPath = DnD_UriListGetNextFile(source.c_str(),
                                               &index,
                                               &newPathLen)) != NULL) {
#if defined(linux)
         if (DnD_UriIsNonFileSchemes(newPath)) {
            /* Try to get local file path for non file uri. */
            GFile *file = g_file_new_for_uri(newPath);
            free(newPath);
            if (!file) {
               g_debug("%s: g_file_new_for_uri failed\n", __FUNCTION__);
               return false;
            }
            newPath = g_file_get_path(file);
            g_object_unref(file);
            if (!newPath) {
               g_debug("%s: g_file_get_path failed\n", __FUNCTION__);
               return false;
            }
         }
#endif
         /*
          * Parse relative path.
          */
         newRelPath = Str_Strrchr(newPath, DIRSEPC) + 1; // Point to char after '/'

         /* Keep track of how big the dnd files are. */
         if ((size = File_GetSizeEx(newPath)) >= 0) {
            totalSize += size;
         } else {
            g_debug("%s: unable to get file size for %s\n", __FUNCTION__, newPath);
         }
         g_debug("%s: Adding newPath '%s' newRelPath '%s'\n", __FUNCTION__,
               newPath, newRelPath);
         fileList.AddFile(newPath, newRelPath);
         free(newPath);
      }

      DynBuf_Init(&buf);
      fileList.SetFileSize(totalSize);
      if (fileList.ToCPClipboard(&buf, false)) {
          CPClipboard_SetItem(&m_clipboard, CPFORMAT_FILELIST, DynBuf_Get(&buf),
                              DynBuf_GetSize(&buf));
      }
      DynBuf_Destroy(&buf);
      return true;
   }

   /* Try to get plain text. */
   if (m_DnD->CheckCapability(DND_CP_CAP_PLAIN_TEXT_DND) && (
       target == TARGET_NAME_STRING ||
       target == TARGET_NAME_TEXT_PLAIN ||
       target == TARGET_NAME_UTF8_STRING ||
       target == TARGET_NAME_COMPOUND_TEXT)) {
      std::string source = sd.get_data_as_string();
      if (source.size() > 0 &&
          source.size() < DNDMSG_MAX_ARGSZ &&
          CPClipboard_SetItem(&m_clipboard, CPFORMAT_TEXT, source.c_str(),
                              source.size() + 1)) {
         g_debug("%s: Got text, size %"FMTSZ"u\n", __FUNCTION__, source.size());
      } else {
         g_debug("%s: Failed to get text\n", __FUNCTION__);
         return false;
      }
      return true;
   }

   /* Try to get RTF string. */
   if (m_DnD->CheckCapability(DND_CP_CAP_RTF_DND) && (
       target == TARGET_NAME_APPLICATION_RTF ||
       target == TARGET_NAME_TEXT_RICHTEXT)) {
      std::string source = sd.get_data_as_string();
      if (source.size() > 0 &&
          source.size() < DNDMSG_MAX_ARGSZ &&
          CPClipboard_SetItem(&m_clipboard, CPFORMAT_RTF, source.c_str(),
                              source.size() + 1)) {
         g_debug("%s: Got RTF, size %"FMTSZ"u\n", __FUNCTION__, source.size());
         return true;
      } else {
         g_debug("%s: Failed to get text\n", __FUNCTION__ );
         return false;
      }
   }
   return true;
}


/**
 *
 * Ask for clipboard data from drag source.
 *
 * @param[in] dc   Associated drag context
 * @param[in] time Time of the request
 *
 * @return true if there is any data request, false otherwise.
 */

bool
DnDUIX11::RequestData(const Glib::RefPtr<Gdk::DragContext> &dc,
                      guint time)
{
   Glib::RefPtr<Gtk::TargetList> targets;
   targets = Gtk::TargetList::create(std::list<Gtk::TargetEntry>());

   CPClipboard_Clear(&m_clipboard);
   m_numPendingRequest = 0;

   /*
    * First check file list. If file list is available, all other formats will
    * be ignored.
    */
   targets->add(Glib::ustring(DRAG_TARGET_NAME_URI_LIST));
   Glib::ustring target = m_detWnd->drag_dest_find_target(dc, targets);
   targets->remove(Glib::ustring(DRAG_TARGET_NAME_URI_LIST));
   if (target != "") {
      m_detWnd->drag_get_data(dc, target, time);
      m_numPendingRequest++;
      return true;
   }

   /* Then check plain text. */
   targets->add(Glib::ustring(TARGET_NAME_UTF8_STRING));
   targets->add(Glib::ustring(TARGET_NAME_STRING));
   targets->add(Glib::ustring(TARGET_NAME_TEXT_PLAIN));
   targets->add(Glib::ustring(TARGET_NAME_COMPOUND_TEXT));
   target = m_detWnd->drag_dest_find_target(dc, targets);
   targets->remove(Glib::ustring(TARGET_NAME_STRING));
   targets->remove(Glib::ustring(TARGET_NAME_TEXT_PLAIN));
   targets->remove(Glib::ustring(TARGET_NAME_UTF8_STRING));
   targets->remove(Glib::ustring(TARGET_NAME_COMPOUND_TEXT));
   if (target != "") {
      m_detWnd->drag_get_data(dc, target, time);
      m_numPendingRequest++;
   }

   /* Then check RTF. */
   targets->add(Glib::ustring(TARGET_NAME_APPLICATION_RTF));
   targets->add(Glib::ustring(TARGET_NAME_TEXT_RICHTEXT));
   target = m_detWnd->drag_dest_find_target(dc, targets);
   targets->remove(Glib::ustring(TARGET_NAME_APPLICATION_RTF));
   targets->remove(Glib::ustring(TARGET_NAME_TEXT_RICHTEXT));
   if (target != "") {
      m_detWnd->drag_get_data(dc, target, time);
      m_numPendingRequest++;
   }
   return (m_numPendingRequest > 0);
}


/**
 *
 * Try to get last directory name from a full path name.
 *
 * @param[in] str pathname to process
 *
 * @return last dir name in the full path name if sucess, empty str otherwise
 */

std::string
DnDUIX11::GetLastDirName(const std::string &str)
{
   std::string ret;
   size_t start;
   size_t end;

   end = str.size() - 1;
   if (end >= 0 && DIRSEPC == str[end]) {
      end--;
   }

   if (end <= 0 || str[0] != DIRSEPC) {
      return "";
   }

   start = end;

   while (str[start] != DIRSEPC) {
      start--;
   }

   return str.substr(start + 1, end - start);
}


/**
 *
 * Provide a substring containing the next path from the provided
 * NUL-delimited string starting at the provided index.
 *
 * @param[in] str NUL-delimited path list
 * @param[in] index current index into string
 *
 * @return a string with the next path or "" if there are no more paths.
 */

utf::utf8string
DnDUIX11::GetNextPath(utf::utf8string& str,
                      size_t& index)
{
   utf::utf8string ret;
   size_t start;

   if (index >= str.length()) {
      return "";
   }

   for (start = index; str[index] != '\0' && index < str.length(); index++) {
      /*
       * Escape reserved characters according to RFC 1630.  We'd use
       * Escape_Do() if this wasn't a utf::string, but let's use the same table
       * replacement approach.
       */
      static char const Dec2Hex[] = {
         '0', '1', '2', '3', '4', '5', '6', '7',
         '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
      };

      unsigned char ubyte = str[index];

      if (ubyte == '#' ||   /* Fragment identifier delimiter */
          ubyte == '?' ||   /* Query string delimiter */
          ubyte == '*' ||   /* "Special significance within specific schemes" */
          ubyte == '!' ||   /* "Special significance within specific schemes" */
          ubyte == '%' ||   /* Escape character */
          ubyte >= 0x80) {  /* UTF-8 encoding bytes */
         str.replace(index, 1, "%");
         str.insert(index + 1, 1, Dec2Hex[ubyte >> 4]);
         str.insert(index + 2, 1, Dec2Hex[ubyte & 0xF]);
         index += 2;
      }
   }

   ret = str.substr(start, index - start);
   g_debug("%s: nextpath: %s", __FUNCTION__, ret.c_str());
   index++;
   return ret;
}


/**
 *
 * Issue a fake mouse move event to the detection window. Code stolen from
 * DnD V2 Linux guest implementation, where it was originally defined as a
 * macro.
 *
 * @param[in] x x-coordinate of location to move mouse to.
 * @param[in] y y-coordinate of location to move mouse to.
 *
 * @return true on success, false on failure.
 */

bool
DnDUIX11::SendFakeMouseMove(const int x,
                            const int y)
{
   return SendFakeXEvents(false, false, false, false, true, x, y);
}


/**
 *
 * Fake X mouse events and window movement for the provided Gtk widget.
 *
 * This function will optionally show the widget, move the provided widget
 * to either the provided location or the current mouse position if no
 * coordinates are provided, and cause a button press or release event.
 *
 * @param[in] showWidget       whether to show Gtk widget
 * @param[in] buttonEvent      whether to send a button event
 * @param[in] buttonPress      whether to press or release mouse
 * @param[in] moveWindow:      whether to move our window too
 * @param[in] coordsProvided   whether coordinates provided
 * @param[in] xCoord           x coordinate
 * @param[in] yCoord           y coordinate
 *
 * @note todo this code should be implemented using GDK APIs.
 * @note todo this code should be moved into the detection window class
 *
 * @return true on success, false on failure.
 */

bool
DnDUIX11::SendFakeXEvents(const bool showWidget,
                          const bool buttonEvent,
                          const bool buttonPress,
                          const bool moveWindow,
                          const bool coordsProvided,
                          const int xCoord,
                          const int yCoord)
{
   GtkWidget *widget;
   Window rootWnd;
   bool ret = false;
   Display *dndXDisplay;
   Window dndXWindow;
   Window rootReturn;
   int x;
   int y;
   Window childReturn;
   int rootXReturn;
   int rootYReturn;
   int winXReturn;
   int winYReturn;
   unsigned int maskReturn;

   x = xCoord;
   y = yCoord;

   widget = GetDetWndAsWidget();

   if (!widget) {
      g_debug("%s: unable to get widget\n", __FUNCTION__);
      return false;
   }

   dndXDisplay = GDK_WINDOW_XDISPLAY(widget->window);
   dndXWindow = GDK_WINDOW_XWINDOW(widget->window);
   rootWnd = RootWindow(dndXDisplay, DefaultScreen(dndXDisplay));

   /*
    * Turn on X synchronization in order to ensure that our X events occur in
    * the order called.  In particular, we want the window movement to occur
    * before the mouse movement so that the events we are coercing do in fact
    * happen.
    */
   XSynchronize(dndXDisplay, True);

   if (showWidget) {
      g_debug("%s: showing Gtk widget\n", __FUNCTION__);
      gtk_widget_show(widget);
      gdk_window_show(widget->window);
   }

   /* Get the current location of the mouse if coordinates weren't provided. */
   if (!coordsProvided) {
      if (!XQueryPointer(dndXDisplay, rootWnd, &rootReturn, &childReturn,
                          &rootXReturn, &rootYReturn, &winXReturn, &winYReturn,
                          &maskReturn)) {
         Warning("%s: XQueryPointer() returned False.\n", __FUNCTION__);
         goto exit;
      }

      g_debug("%s: current mouse is at (%d, %d)\n", __FUNCTION__,
            rootXReturn, rootYReturn);

      /*
       * Position away from the edge of the window.
       */
      int width = m_detWnd->GetScreenWidth();
      int height = m_detWnd->GetScreenHeight();
      bool change = false;

      x = rootXReturn;
      y = rootYReturn;

      /*
       * first do left and top edges.
       */

      if (x <= 5) {
         x = 6;
         change = true;
      }

      if (y <= 5) {
         y = 6;
         change = true;
      }

      /*
       * next, move result away from right and bottom edges.
       */
      if (x > width - 5) {
         x = width - 6;
         change = true;
      }
      if (y > height - 5) {
         y = height - 6;
         change = true;
      }

      if (change) {
         g_debug("%s: adjusting mouse position. root %d, %d, adjusted %d, %d\n",
               __FUNCTION__, rootXReturn, rootYReturn, x, y);
      }
   }

   if (moveWindow) {
      /*
       * Make sure the window is at this point and at the top (raised).  The
       * window is resized to be a bit larger than we would like to increase
       * the likelihood that mouse events are attributed to our window -- this
       * is okay since the window is invisible and hidden on cancels and DnD
       * finish.
       */
      XMoveResizeWindow(dndXDisplay, dndXWindow, x - 5 , y - 5, 25, 25);
      XRaiseWindow(dndXDisplay, dndXWindow);
      g_debug("%s: move wnd to (%d, %d, %d, %d)\n", __FUNCTION__, x - 5, y - 5 , x + 25, y + 25);
   }

   /*
    * Generate mouse movements over the window.  The second one makes ungrabs
    * happen more reliably on KDE, but isn't necessary on GNOME.
    */
   XTestFakeMotionEvent(dndXDisplay, -1, x, y, CurrentTime);
   XTestFakeMotionEvent(dndXDisplay, -1, x + 1, y + 1, CurrentTime);
   g_debug("%s: move mouse to (%d, %d) and (%d, %d)\n", __FUNCTION__, x, y, x + 1, y + 1);

   if (buttonEvent) {
      g_debug("%s: faking left mouse button %s\n", __FUNCTION__,
              buttonPress ? "press" : "release");
      XTestFakeButtonEvent(dndXDisplay, 1, buttonPress, CurrentTime);
      XSync(dndXDisplay, False);

      if (!buttonPress) {
         /*
          * The button release simulation may be failed with some distributions
          * like Ubuntu 10.4 and RHEL 6 for guest->host DnD. So first query
          * mouse button status. If some button is still down, we will try
          * mouse device level event simulation. For details please refer
          * to bug 552807.
          */
         if (!XQueryPointer(dndXDisplay, rootWnd, &rootReturn, &childReturn,
                            &rootXReturn, &rootYReturn, &winXReturn,
                            &winYReturn, &maskReturn)) {
            Warning("%s: XQueryPointer returned False.\n", __FUNCTION__);
            goto exit;
         }

         if ((maskReturn & Button1Mask) ||
             (maskReturn & Button2Mask) ||
             (maskReturn & Button3Mask) ||
             (maskReturn & Button4Mask) ||
             (maskReturn & Button5Mask)) {
            Debug("%s: XTestFakeButtonEvent was not working for button "
                  "release, trying XTestFakeDeviceButtonEvent now.\n",
                  __FUNCTION__);
            ret = TryXTestFakeDeviceButtonEvent();
         } else {
            g_debug("%s: XTestFakeButtonEvent was working for button release.\n",
                    __FUNCTION__);
            ret = true;
         }
      } else {
         ret = true;
      }
   }

exit:
   XSynchronize(dndXDisplay, False);
   return ret;
}


/**
 * Fake X mouse events in device level.
 *
 * XXX The function will only be called if XTestFakeButtonEvent does not work
 * for mouse button release. Later on we may only call this one for mouse
 * button simulation if this is more reliable.
 *
 * @return true on success, false on failure.
 */

bool
DnDUIX11::TryXTestFakeDeviceButtonEvent(void)
{
   XDeviceInfo *list = NULL;
   XDeviceInfo *list2 = NULL;
   XDevice *tdev = NULL;
   XDevice *buttonDevice = NULL;
   int numDevices = 0;
   int i = 0;
   int j = 0;
   XInputClassInfo *ip = NULL;
   GtkWidget *widget;
   Display *dndXDisplay;

   widget = GetDetWndAsWidget();

   if (!widget) {
      g_debug("%s: unable to get widget\n", __FUNCTION__);
      return false;
   }

   dndXDisplay = GDK_WINDOW_XDISPLAY(widget->window);

   /* First get list of all input device. */
   if (!(list = XListInputDevices (dndXDisplay, &numDevices))) {
      g_debug("%s: XListInputDevices failed\n", __FUNCTION__);
      return false;
   } else {
      g_debug("%s: XListInputDevices got %d devices\n", __FUNCTION__, numDevices);
   }

   list2 = list;

   for (i = 0; i < numDevices; i++, list++) {
      /* We only care about mouse device. */
      if (list->use != IsXExtensionPointer) {
         continue;
      }

      tdev = XOpenDevice(dndXDisplay, list->id);
      if (!tdev) {
         g_debug("%s: XOpenDevice failed\n", __FUNCTION__);
         continue;
      }

      for (ip = tdev->classes, j = 0; j < tdev->num_classes; j++, ip++) {
         if (ip->input_class == ButtonClass) {
            buttonDevice = tdev;
            break;
         }
      }

      if (buttonDevice) {
         g_debug("%s: calling XTestFakeDeviceButtonEvent for %s\n",
               __FUNCTION__, list->name);
         XTestFakeDeviceButtonEvent(dndXDisplay, buttonDevice, 1, False,
                                    NULL, 0, CurrentTime);
         buttonDevice = NULL;
      }
      XCloseDevice(dndXDisplay, tdev);
   }
   XFreeDeviceList(list2);
   return true;
}


/**
 *
 * Get the GtkWidget pointer for a DragDetWnd object. The X11 Unity
 * implementation requires access to the drag detection window as
 * a GtkWindow pointer, which it uses to show and hide the detection
 * window. This function is also called by the code that issues fake
 * X events to the detection window.
 *
 * @return a pointer to the GtkWidget for the detection window, or NULL
 * on failure.
 */

GtkWidget *
DnDUIX11::GetDetWndAsWidget()
{
   GtkInvisible *window;
   GtkWidget *widget = NULL;

   if (!m_detWnd) {
      return NULL;
   }
   window = m_detWnd->gobj();
   if (window) {
      widget = GTK_WIDGET(window);
   }
   return widget;
}


/**
 *
 * Add a block for the current H->G file transfer. Must be paired with a
 * call to RemoveBlock() on finish or cancellation.
 */

void
DnDUIX11::AddBlock()
{
   g_debug("%s: enter\n", __FUNCTION__);
   if (m_blockAdded) {
      g_debug("%s: block already added\n", __FUNCTION__);
      return;
   }
   g_debug("%s: DnDBlockIsReady %d fd %d\n", __FUNCTION__, DnD_BlockIsReady(m_blockCtrl), m_blockCtrl->fd);
   if (DnD_BlockIsReady(m_blockCtrl) && m_blockCtrl->AddBlock(m_blockCtrl->fd, m_HGStagingDir.c_str())) {
      m_blockAdded = true;
      g_debug("%s: add block for %s.\n", __FUNCTION__, m_HGStagingDir.c_str());
   } else {
      m_blockAdded = false;
      g_debug("%s: unable to add block dir %s.\n", __FUNCTION__, m_HGStagingDir.c_str());
   }
}


/**
 *
 * Remove block for the current H->G file transfer. Must be paired with a
 * call to AddBlock(), but it will only attempt to remove block if one is
 * currently in effect.
 */

void
DnDUIX11::RemoveBlock()
{
   g_debug("%s: enter\n", __FUNCTION__);
   if (m_blockAdded && (DND_FILE_TRANSFER_IN_PROGRESS != m_HGGetFileStatus)) {
      g_debug("%s: removing block for %s\n", __FUNCTION__, m_HGStagingDir.c_str());
      /* We need to make sure block subsystem has not been shut off. */
      if (DnD_BlockIsReady(m_blockCtrl)) {
         m_blockCtrl->RemoveBlock(m_blockCtrl->fd, m_HGStagingDir.c_str());
      }
      m_blockAdded = false;
   } else {
      g_debug("%s: not removing block m_blockAdded %d m_HGGetFileStatus %d\n",
            __FUNCTION__,
            m_blockAdded,
            m_HGGetFileStatus);
   }
}


/**
 *
 * Convert a Gdk::DragAction value to its corresponding DND_DROPEFFECT.
 *
 * @param[in] the Gdk::DragAction value to return.
 *
 * @return the corresponding DND_DROPEFFECT, with DROP_UNKNOWN returned
 * if no mapping is supported.
 *
 * @note DROP_NONE is not mapped in this function.
 */

DND_DROPEFFECT
DnDUIX11::ToDropEffect(Gdk::DragAction action)
{
   DND_DROPEFFECT effect;

   switch(action) {
   case Gdk::ACTION_COPY:
   case Gdk::ACTION_DEFAULT:
      effect = DROP_COPY;
      break;
   case Gdk::ACTION_MOVE:
      effect = DROP_MOVE;
      break;
   case Gdk::ACTION_LINK:
      effect = DROP_LINK;
      break;
   case Gdk::ACTION_PRIVATE:
   case Gdk::ACTION_ASK:
   default:
      effect = DROP_UNKNOWN;
      break;
   }
   return effect;
}


/**
 *
 * Try to extract file contents from m_clipboard. Write all files to a
 * temporary staging directory. Construct uri list.
 *
 * @return true if success, false otherwise.
 */

bool
DnDUIX11::WriteFileContentsToStagingDir(void)
{
   void *buf = NULL;
   size_t sz = 0;
   XDR xdrs;
   CPFileContents fileContents;
   CPFileContentsList *contentsList = NULL;
   size_t nFiles = 0;
   CPFileItem *fileItem = NULL;
   Unicode tempDir = NULL;
   size_t i = 0;
   bool ret = false;

   if (!CPClipboard_GetItem(&m_clipboard, CPFORMAT_FILECONTENTS, &buf, &sz)) {
      return false;
   }

   /* Extract file contents from buf. */
   xdrmem_create(&xdrs, (char *)buf, sz, XDR_DECODE);
   memset(&fileContents, 0, sizeof fileContents);

   if (!xdr_CPFileContents(&xdrs, &fileContents)) {
      g_debug("%s: xdr_CPFileContents failed.\n", __FUNCTION__);
      xdr_destroy(&xdrs);
      return false;
   }
   xdr_destroy(&xdrs);

   contentsList = fileContents.CPFileContents_u.fileContentsV1;
   if (!contentsList) {
      g_debug("%s: invalid contentsList.\n", __FUNCTION__);
      goto exit;
   }

   nFiles = contentsList->fileItem.fileItem_len;
   if (0 == nFiles) {
      g_debug("%s: invalid nFiles.\n", __FUNCTION__);
      goto exit;
   }

   fileItem = contentsList->fileItem.fileItem_val;
   if (!fileItem) {
      g_debug("%s: invalid fileItem.\n", __FUNCTION__);
      goto exit;
   }

   /*
    * Write files into a temporary staging directory. These files will be moved
    * to final destination, or deleted on next reboot.
    */
   tempDir = DnD_CreateStagingDirectory();
   if (!tempDir) {
      g_debug("%s: DnD_CreateStagingDirectory failed.\n", __FUNCTION__);
      goto exit;
   }

   m_HGFileContentsUriList = "";

   for (i = 0; i < nFiles; i++) {
      utf::string fileName;
      utf::string filePathName;
      VmTimeType createTime = -1;
      VmTimeType accessTime = -1;
      VmTimeType writeTime = -1;
      VmTimeType attrChangeTime = -1;

      if (!fileItem[i].cpName.cpName_val ||
          0 == fileItem[i].cpName.cpName_len) {
         g_debug("%s: invalid fileItem[%"FMTSZ"u].cpName.\n", __FUNCTION__, i);
         goto exit;
      }

      /*
       * '\0' is used as directory separator in cross-platform name. Now turn
       * '\0' in data into DIRSEPC.
       *
       * Note that we don't convert the final '\0' into DIRSEPC so the string
       * is NUL terminated.
       */
      CPNameUtil_CharReplace(fileItem[i].cpName.cpName_val,
                             fileItem[i].cpName.cpName_len - 1,
                             '\0',
                             DIRSEPC);
      fileName = fileItem[i].cpName.cpName_val;
      filePathName = tempDir;
      filePathName += DIRSEPS + fileName;

      if (fileItem[i].validFlags & CP_FILE_VALID_TYPE &&
          CP_FILE_TYPE_DIRECTORY == fileItem[i].type) {
         if (!File_CreateDirectory(filePathName.c_str())) {
            goto exit;
         }
         g_debug("%s: created directory [%s].\n",
               __FUNCTION__, filePathName.c_str());
      } else if (fileItem[i].validFlags & CP_FILE_VALID_TYPE &&
                 CP_FILE_TYPE_REGULAR == fileItem[i].type) {
         FileIODescriptor file;
         FileIOResult fileErr;

         FileIO_Invalidate(&file);

         fileErr = FileIO_Open(&file,
                               filePathName.c_str(),
                               FILEIO_ACCESS_WRITE,
                               FILEIO_OPEN_CREATE_EMPTY);
         if (!FileIO_IsSuccess(fileErr)) {
            goto exit;
         }

         fileErr = FileIO_Write(&file,
                                fileItem[i].content.content_val,
                                fileItem[i].content.content_len,
                                NULL);

         FileIO_Close(&file);
         g_debug("%s: created file [%s].\n",
               __FUNCTION__, filePathName.c_str());
      } else {
         /*
          * Right now only Windows can provide CPFORMAT_FILECONTENTS data.
          * Symlink file is not expected. Continue with next file if the
          * type is not valid.
          */
         continue;
      }

      /* Update file time attributes. */
      createTime = fileItem->validFlags & CP_FILE_VALID_CREATE_TIME ?
         fileItem->createTime: -1;
      accessTime = fileItem->validFlags & CP_FILE_VALID_ACCESS_TIME ?
         fileItem->accessTime: -1;
      writeTime = fileItem->validFlags & CP_FILE_VALID_WRITE_TIME ?
         fileItem->writeTime: -1;
      attrChangeTime = fileItem->validFlags & CP_FILE_VALID_CHANGE_TIME ?
         fileItem->attrChangeTime: -1;

      if (!File_SetTimes(filePathName.c_str(),
                         createTime,
                         accessTime,
                         writeTime,
                         attrChangeTime)) {
         /* Not a critical error, only log it. */
         g_debug("%s: File_SetTimes failed with file [%s].\n",
               __FUNCTION__, filePathName.c_str());
      }

      /* Update file permission attributes. */
      if (fileItem->validFlags & CP_FILE_VALID_PERMS) {
         if (Posix_Chmod(filePathName.c_str(),
                         fileItem->permissions) < 0) {
            /* Not a critical error, only log it. */
            g_debug("%s: Posix_Chmod failed with file [%s].\n",
                  __FUNCTION__, filePathName.c_str());
         }
      }

      /*
       * If there is no DIRSEPC inside the fileName, this file/directory is a
       * top level one. We only put top level name into uri list.
       */
      if (fileName.find(DIRSEPS, 0) == utf::string::npos) {
         m_HGFileContentsUriList += "file://" + filePathName + "\r\n";
      }
   }
   g_debug("%s: created uri list [%s].\n",
         __FUNCTION__, m_HGFileContentsUriList.c_str());
   ret = true;

exit:
   xdr_free((xdrproc_t) xdr_CPFileContents, (char *)&fileContents);
   if (tempDir && !ret) {
      DnD_DeleteStagingFiles(tempDir, false);
   }
   free(tempDir);
   return ret;
}


/**
 * Tell host that we are done with HG DnD initialization.
 */

void
DnDUIX11::SourceDragStartDone(void)
{
   g_debug("%s: enter\n", __FUNCTION__);
   m_inHGDrag = true;
   m_DnD->SrcUIDragBeginDone();
}


/**
 * Set block control member.
 *
 * @param[in] block control as setup by vmware-user.
 */

void
DnDUIX11::SetBlockControl(DnDBlockControl *blockCtrl)
{
   m_blockCtrl = blockCtrl;
}


/**
 * Got feedback from our DropSource, send it over to host. Called by
 * drag motion callback.
 *
 * @param[in] effect feedback to send to the UI-independent DnD layer.
 */

void
DnDUIX11::SourceUpdateFeedback(DND_DROPEFFECT effect)
{
   g_debug("%s: entering\n", __FUNCTION__);
   m_DnD->SrcUIUpdateFeedback(effect);
}


/**
 * This is triggered when user drags valid data from guest to host. Try to
 * get clip data and notify host to start GH DnD.
 */

void
DnDUIX11::TargetDragEnter(void)
{
   g_debug("%s: entering\n", __FUNCTION__);

   /* Check if there is valid data with current detection window. */
   if (!CPClipboard_IsEmpty(&m_clipboard)) {
      g_debug("%s: got valid data from detWnd.\n", __FUNCTION__);
      m_DnD->DestUIDragEnter(&m_clipboard);
   }

   /*
    * Show the window, and position it under the current mouse position.
    * This is particularly important for KDE 3.5 guests.
    */
   SendFakeXEvents(true, false, true, true, false, 0, 0);
}


/**
 * Get Unix time in milliseconds. See man 2 gettimeofday for details.
 *
 * @return unix time in milliseconds.
 */

unsigned long
DnDUIX11::GetTimeInMillis(void)
{
   VmTimeType atime;

   Hostinfo_GetTimeOfDay(&atime);
   return((unsigned long)(atime / 1000));
}


/**
 * Update version information in m_DnD.
 *
 * @param[ignored] chan RpcChannel pointer
 * @param[in] version the version negotiated with host.
 */

void
DnDUIX11::VmxDnDVersionChanged(RpcChannel *chan, uint32 version)
{
   ASSERT(m_DnD);
   m_DnD->VmxDnDVersionChanged(version);
}


/**
 * Track enter events on detection window.
 *
 * @param[ignored] event event data (ignored)
 *
 * @return always true.
 */

bool
DnDUIX11::GtkEnterEventCB(GdkEventCrossing *ignored)
{
   g_debug("%s: enter\n", __FUNCTION__);
   return true;
}

/**
 * Track enter events on detection window.
 *
 * @param[ignored] event event data (ignored)
 *
 * @return always true.
 */

bool
DnDUIX11::GtkLeaveEventCB(GdkEventCrossing *ignored)
{
   g_debug("%s: enter\n", __FUNCTION__);
   return true;
}

/**
 * Track enter events on detection window.
 *
 * @param[ignored] event event data (ignored)
 *
 * @return always true.
 */

bool
DnDUIX11::GtkMapEventCB(GdkEventAny *event)
{
   g_debug("%s: enter\n", __FUNCTION__);
   return true;
}


/**
 * Track enter events on detection window.
 *
 * @param[ignored] event event data (ignored)
 *
 * @return always true.
 */

bool
DnDUIX11::GtkUnmapEventCB(GdkEventAny *event)
{
   g_debug("%s: enter\n", __FUNCTION__);
   return true;
}


/**
 * Track realize events on detection window.
 */

void
DnDUIX11::GtkRealizeEventCB()
{
   g_debug("%s: enter\n", __FUNCTION__);
}


/**
 * Track unrealize events on detection window.
 */

void
DnDUIX11::GtkUnrealizeEventCB()
{
   g_debug("%s: enter\n", __FUNCTION__);
}

/**
 * Track motion notify events on detection window.
 *
 * @param[in] event event data
 *
 * @return always true.
 */

bool
DnDUIX11::GtkMotionNotifyEventCB(GdkEventMotion *event)
{
   g_debug("%s: enter x %f y %f state 0x%x\n", __FUNCTION__,
           event->x, event->y, event->state);
   return true;
}


/**
 * Track configure events on detection window.
 *
 * @param[in] event event data
 *
 * @return always true.
 */

bool
DnDUIX11::GtkConfigureEventCB(GdkEventConfigure *event)
{
   g_debug("%s: enter x %d y %d width %d height %d\n",
           __FUNCTION__, event->x, event->y, event->width, event->height);
   return true;
}


/**
 * Track button press events on detection window.
 *
 * @param[ignored] event event data (ignored)
 *
 * @return always true.
 */

bool
DnDUIX11::GtkButtonPressEventCB(GdkEventButton *event)
{
   g_debug("%s: enter\n", __FUNCTION__);
   return true;
}


/**
 * Track button release events on detection window.
 *
 * @param[ignored] event event data (ignored)
 *
 * @return always true.
 */

bool
DnDUIX11::GtkButtonReleaseEventCB(GdkEventButton *event)
{
   g_debug("%s: enter\n", __FUNCTION__);
   return true;
}


