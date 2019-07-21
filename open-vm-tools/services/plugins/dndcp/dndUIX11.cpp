/*********************************************************
 * Copyright (C) 2009-2019 VMware, Inc. All rights reserved.
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

#include "xutils/xutils.hh"

#include "dndUIX11.h"
#include "guestDnDCPMgr.hh"
#include "tracer.hh"

extern "C" {
#include <X11/extensions/XTest.h>       /* for XTest*() */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include "vmware/guestrpc/tclodefs.h"

#include "copyPasteCompat.h"
#include "cpName.h"
#include "cpNameUtil.h"
#include "dndClipboard.h"
#include "hgfsUri.h"
#include "rpcout.h"
}

#ifdef USE_UINPUT
#include "fakeMouseWayland/fakeMouseWayland.h"
#endif
#include "dnd.h"
#include "dndMsg.h"
#include "hostinfo.h"
#include "file.h"
#include "vmblock.h"

/* IsXExtensionPointer may be not defined with old Xorg. */
#ifndef IsXExtensionPointer
#define IsXExtensionPointer 4
#endif

#include "copyPasteDnDWrapper.h"


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::DnDUIX11 --
 *
 *      Constructor.
 *
 *-----------------------------------------------------------------------------
 */

DnDUIX11::DnDUIX11(ToolsAppCtx *ctx)
    : mCtx(ctx),
      mDnD(NULL),
      mDetWnd(NULL),
      mBlockCtrl(NULL),
      mHGGetFileStatus(DND_FILE_TRANSFER_NOT_STARTED),
      mBlockAdded(false),
      mGHDnDInProgress(false),
      mGHDnDDataReceived(false),
      mUnityMode(false),
      mInHGDrag(false),
      mEffect(DROP_NONE),
      mMousePosX(0),
      mMousePosY(0),
      mDragCtx(NULL),
      mNumPendingRequest(0),
      mDestDropTime(0),
      mTotalFileSize(0),
      mOrigin(0, 0),
      mUseUInput(false)
{
   TRACE_CALL();

   xutils::Init();
   xutils::workAreaChanged.connect(sigc::mem_fun(this, &DnDUIX11::OnWorkAreaChanged));

   /*
    * XXX Hard coded use of default screen means this doesn't work in dual-
    * headed setups (e.g. DISPLAY=:0.1).  However, the number of people running
    * such setups in VMs is expected to be, like, hella small, so I'mma cut
    * corners for now.
    */
   OnWorkAreaChanged(Gdk::Screen::get_default());

#ifdef USE_UINPUT
   //Initialize the uinput device if available
   if (ctx->uinputFD != -1) {
      Screen * scrn = DefaultScreenOfDisplay(XOpenDisplay(NULL));
      if (FakeMouse_Init(ctx->uinputFD, scrn->width, scrn->height)) {
         mUseUInput = true;
         mScreenWidth = scrn->width;
         mScreenHeight = scrn->height;
      }
   }
#endif

   g_debug("%s: Use UInput? %d.\n", __FUNCTION__, mUseUInput);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::~DnDUIX11 --
 *
 *      Destructor.
 *
 *-----------------------------------------------------------------------------
 */

DnDUIX11::~DnDUIX11()
{
   TRACE_CALL();

   if (mDetWnd) {
      delete mDetWnd;
   }
   CPClipboard_Destroy(&mClipboard);
   /* Any files from last unfinished file transfer should be deleted. */
   if (   DND_FILE_TRANSFER_IN_PROGRESS == mHGGetFileStatus
       && !mHGStagingDir.empty()) {
      uint64 totalSize = File_GetSizeEx(mHGStagingDir.c_str());
      if (mTotalFileSize != totalSize) {
         g_debug("%s: deleting %s, expecting %" FMT64 "u, finished %" FMT64 "u\n",
                 __FUNCTION__, mHGStagingDir.c_str(),
                 mTotalFileSize, totalSize);
         DnD_DeleteStagingFiles(mHGStagingDir.c_str(), FALSE);
      } else {
         g_debug("%s: file size match %s\n",
                 __FUNCTION__, mHGStagingDir.c_str());
      }
   }
   ResetUI();
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::Init --
 *
 *      Initialize DnDUIX11 object.
 *
 * Results:
 *      Returns true on success and false on failure.
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDUIX11::Init()
{
   TRACE_CALL();
   bool ret = true;

   CPClipboard_Init(&mClipboard);

   GuestDnDCPMgr *p = GuestDnDCPMgr::GetInstance();
   ASSERT(p);
   mDnD = p->GetDnDMgr();
   ASSERT(mDnD);

   mDetWnd = new DragDetWnd();
   if (!mDetWnd) {
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
   mDetWnd->SetAttributes();
#endif

   InitGtk();

#define CONNECT_SIGNAL(_obj, _sig, _cb) \
   _obj->_sig.connect(sigc::mem_fun(this, &DnDUIX11::_cb))

   /* Set common layer callbacks. */
   CONNECT_SIGNAL(mDnD, srcDragBeginChanged,   OnSrcDragBegin);
   CONNECT_SIGNAL(mDnD, srcDropChanged,        OnSrcDrop);
   CONNECT_SIGNAL(mDnD, srcCancelChanged,      OnSrcCancel);
   CONNECT_SIGNAL(mDnD, destCancelChanged,     OnDestCancel);
   CONNECT_SIGNAL(mDnD, destMoveDetWndToMousePosChanged, OnDestMoveDetWndToMousePos);
   CONNECT_SIGNAL(mDnD, getFilesDoneChanged,   OnGetFilesDone);
   CONNECT_SIGNAL(mDnD, moveMouseChanged,      OnMoveMouse);
   CONNECT_SIGNAL(mDnD, privDropChanged,       OnPrivateDrop);
   CONNECT_SIGNAL(mDnD, updateDetWndChanged,   OnUpdateDetWnd);
   CONNECT_SIGNAL(mDnD, updateUnityDetWndChanged, OnUpdateUnityDetWnd);

   /* Set Gtk+ callbacks for source. */
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_drag_begin(),        OnGtkDragBegin);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_drag_data_get(),     OnGtkDragDataGet);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_drag_end(),          OnGtkDragEnd);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_enter_notify_event(), GtkEnterEventCB);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_leave_notify_event(), GtkLeaveEventCB);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_map_event(),         GtkMapEventCB);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_unmap_event(),       GtkUnmapEventCB);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_realize(),           GtkRealizeEventCB);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_unrealize(),         GtkUnrealizeEventCB);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_motion_notify_event(), GtkMotionNotifyEventCB);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_configure_event(),   GtkConfigureEventCB);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_button_press_event(), GtkButtonPressEventCB);
   CONNECT_SIGNAL(mDetWnd->GetWnd(), signal_button_release_event(), GtkButtonReleaseEventCB);

#undef CONNECT_SIGNAL

   OnUpdateDetWnd(false, 0, 0);
   OnUpdateUnityDetWnd(false, 0, false);
   goto out;
fail:
   ret = false;
   if (mDnD) {
      delete mDnD;
      mDnD = NULL;
   }
   if (mDetWnd) {
      delete mDetWnd;
      mDetWnd = NULL;
   }
out:
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::InitGtk --
 *
 *      Register supported DND target types and signal handlers with GTK+.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::InitGtk()
{
   TRACE_CALL();

   /* Construct supported target list for HG DnD. */
   std::vector<Gtk::TargetEntry> targets;

   /* File DnD. */
   targets.push_back(Gtk::TargetEntry(DRAG_TARGET_NAME_URI_LIST));

   /* RTF text DnD. */
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_APPLICATION_RTF));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_TEXT_RICHTEXT));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_TEXT_RTF));

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
   mDetWnd->GetWnd()->drag_dest_set(targets, Gtk::DEST_DEFAULT_MOTION,
                                    Gdk::ACTION_COPY | Gdk::ACTION_MOVE);

   mDetWnd->GetWnd()->signal_drag_leave().connect(
      sigc::mem_fun(this, &DnDUIX11::OnGtkDragLeave));
   mDetWnd->GetWnd()->signal_drag_motion().connect(
      sigc::mem_fun(this, &DnDUIX11::OnGtkDragMotion));
   mDetWnd->GetWnd()->signal_drag_drop().connect(
      sigc::mem_fun(this, &DnDUIX11::OnGtkDragDrop));
   mDetWnd->GetWnd()->signal_drag_data_received().connect(
      sigc::mem_fun(this, &DnDUIX11::OnGtkDragDataReceived));
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::ResetUI --
 *
 *      Reset UI state variables.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May remove a vmblock blocking entry.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::ResetUI()
{
   TRACE_CALL();
   mGHDnDDataReceived = false;
   mHGGetFileStatus = DND_FILE_TRANSFER_NOT_STARTED;
   mGHDnDInProgress = false;
   mEffect = DROP_NONE;
   mInHGDrag = false;
   mDragCtx = NULL;
   RemoveBlock();
}


/* Source functions for HG DnD. */


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnSrcDragBegin --
 *
 *      Called when host successfully detected a pending HG drag.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Calls mDetWnd->drag_begin().
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnSrcDragBegin(const CPClipboard *clip,       // IN
                         const std::string stagingDir)  // IN
{
   Glib::RefPtr<Gtk::TargetList> targets;
   Gdk::DragAction actions;
   GdkEventMotion event;
   int mouseX = mOrigin.get_x() + DRAG_DET_WINDOW_WIDTH / 2;
   int mouseY = mOrigin.get_y() + DRAG_DET_WINDOW_WIDTH / 2;

   TRACE_CALL();

   CPClipboard_Clear(&mClipboard);
   CPClipboard_Copy(&mClipboard, clip);

#ifdef USE_UINPUT
   if (mUseUInput) {
      /*
       * Check if the screen size changes, if so then update the
       * uinput device.
       */
      Screen * scrn = DefaultScreenOfDisplay(XOpenDisplay(NULL));
      if (   (scrn->width != mScreenWidth)
          || (scrn->height != mScreenHeight)) {
         g_debug("%s: Update uinput device. prew:%d, preh:%d, w:%d, h:%d\n",
                 __FUNCTION__,
                 mScreenWidth,
                 mScreenHeight,
                 scrn->width,
                 scrn->height);
         mScreenWidth = scrn->width;
         mScreenHeight = scrn->height;
         FakeMouse_Update(mScreenWidth, mScreenHeight);
      }
   }
#endif

   /*
    * Before the DnD, we should make sure that the mouse is released
    * otherwise it may be another DnD, not ours. Send a release, then
    * a press here to cover this case.
    */

   SendFakeXEvents(true, true, false, true, true, mouseX, mouseY);
   SendFakeXEvents(false, true, true, false, true, mouseX, mouseY);

   /*
    * Construct the target and action list, as well as a fake motion notify
    * event that's consistent with one that would typically start a drag.
    */
   targets = Gtk::TargetList::create(std::vector<Gtk::TargetEntry>());

   if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_FILELIST)) {
      mHGStagingDir = stagingDir;
      if (!mHGStagingDir.empty()) {
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

   if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_FILECONTENTS)) {
      if (WriteFileContentsToStagingDir()) {
         targets->add(Glib::ustring(DRAG_TARGET_NAME_URI_LIST));
      }
   }

   if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_TEXT)) {
      targets->add(Glib::ustring(TARGET_NAME_STRING));
      targets->add(Glib::ustring(TARGET_NAME_TEXT_PLAIN));
      targets->add(Glib::ustring(TARGET_NAME_UTF8_STRING));
      targets->add(Glib::ustring(TARGET_NAME_COMPOUND_TEXT));
   }

   if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_RTF)) {
      targets->add(Glib::ustring(TARGET_NAME_APPLICATION_RTF));
      targets->add(Glib::ustring(TARGET_NAME_TEXT_RICHTEXT));
      targets->add(Glib::ustring(TARGET_NAME_TEXT_RTF));
   }

   actions = Gdk::ACTION_COPY | Gdk::ACTION_MOVE;

   /* TODO set the x/y coords to the actual drag initialization point. */
   event.type = GDK_MOTION_NOTIFY;
   event.window = mDetWnd->GetWnd()->get_window()->gobj();
   event.send_event = false;
   event.time = GDK_CURRENT_TIME;
   event.x = 10;
   event.y = 10;
   event.axes = NULL;
   event.state = GDK_BUTTON1_MASK;
   event.is_hint = 0;
#ifndef GTK3
   event.device = gdk_device_get_core_pointer();
#else
   GdkDeviceManager* manager = gdk_display_get_device_manager(gdk_window_get_display(event.window));
   event.device = gdk_device_manager_get_client_pointer(manager);
#endif
   event.x_root = mOrigin.get_x();
   event.y_root = mOrigin.get_y();

   /* Tell Gtk that a drag should be started from this widget. */
   mDetWnd->GetWnd()->drag_begin(targets, actions, 1, (GdkEvent *)&event);
   mBlockAdded = false;
   mHGGetFileStatus = DND_FILE_TRANSFER_NOT_STARTED;
   SourceDragStartDone();
   /* Initialize host hide feedback to DROP_NONE. */
   mEffect = DROP_NONE;
   SourceUpdateFeedback(mEffect);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnSrcCancel --
 *
 *      Handler for when host cancels HG drag.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Detection window and fake mouse events.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnSrcCancel()
{
   TRACE_CALL();

   /*
    * Force the window to show, position the mouse over it, and release.
    * Seems like moving the window to 0, 0 eliminates frequently observed
    * flybacks when we cancel as user moves mouse in and out of destination
    * window in a H->G DnD.
    */
   OnUpdateDetWnd(true, mOrigin.get_x(), mOrigin.get_y());
   SendFakeXEvents(true, true, false, true, true,
                   mOrigin.get_x() + DRAG_DET_WINDOW_WIDTH / 2,
                   mOrigin.get_y() + DRAG_DET_WINDOW_WIDTH / 2);
   OnUpdateDetWnd(false, 0, 0);
   SendFakeXEvents(false, false, false, false, true,
                   mMousePosX,
                   mMousePosY);
   mInHGDrag = false;
   mHGGetFileStatus = DND_FILE_TRANSFER_NOT_STARTED;
   mEffect = DROP_NONE;
   RemoveBlock();
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnPrivateDrop --
 *
 *      Handler for private drop event.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Releases mouse button at current position.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnPrivateDrop(int32 x,        // UNUSED
                        int32 y)        // UNUSED
{
   TRACE_CALL();

   /* Unity manager in host side may already send the drop into guest. */
   if (mGHDnDInProgress) {

      /*
       * Release the mouse button.
       */
      SendFakeXEvents(false, true, false, false, false, 0, 0);
   }
   ResetUI();
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnDestCancel --
 *
 *      Handler for GH drag cancellation.
 *
 *      Note: This event fires as part of the complete guest-to-host sequence,
 *      not just error or user cancellation.
 *
 * Results:
 *      Uses detection window and fake mouse events to intercept drop.
 *
 * Side effects:
 *      Reinitializes UI state.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnDestCancel()
{
   TRACE_CALL();

   /* Unity manager in host side may already send the drop into guest. */
   if (mGHDnDInProgress) {
      /*
       * Show the window, move it to the mouse position, and release the
       * mouse button.
       */
      SendFakeXEvents(true, true, false, true, false, mOrigin.get_x(),
                      mOrigin.get_y());
   }
   mDestDropTime = GetTimeInMillis();
   ResetUI();
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnSrcDrop --
 *
 *      Callback when host signals drop.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Release the mouse button in the detection window.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnSrcDrop()
{
   TRACE_CALL();
   OnUpdateDetWnd(true, mOrigin.get_x(), mOrigin.get_y());

   /*
    * Move the mouse to the saved coordinates, and release the mouse button.
    */
   SendFakeXEvents(false, true, false, false, true, mMousePosX, mMousePosY);
   OnUpdateDetWnd(false, 0, 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGetFilesDone --
 *
 *      Callback when HG file transfer completes.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Releases vmblock blocking entry.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnGetFilesDone(bool success)  // IN: true if transfer succeeded
{
   g_debug("%s: %s\n", __FUNCTION__, success ? "success" : "failed");

   /*
    * If hg drag is not done yet, only remove block. OnGtkDragEnd will
    * call ResetUI(). Otherwise destination may miss the data because
     * we are already reset.
    */

   mHGGetFileStatus = DND_FILE_TRANSFER_FINISHED;

   if (!mInHGDrag) {
      ResetUI();
   } else {
      RemoveBlock();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnUpdateDetWnd --
 *
 *      Callback to show/hide drag detection window.
 *
 * Results:
 *      Shows/hides and moves detection window.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnUpdateDetWnd(bool show,     // IN: show (true) or hide (false)
                         int32 x,       // IN: detection window's destination x-coord
                         int32 y)       // IN: detection window's destination y-coord
{
   g_debug("%s: enter 0x%lx show %d x %d y %d\n",
         __FUNCTION__,
         (unsigned long) mDetWnd->GetWnd()->get_window()->gobj(), show, x, y);

   /* If the window is being shown, move it to the right place. */
   if (show) {
      x = MAX(x - DRAG_DET_WINDOW_WIDTH / 2, mOrigin.get_x());
      y = MAX(y - DRAG_DET_WINDOW_WIDTH / 2, mOrigin.get_y());

      mDetWnd->Show();
      mDetWnd->Raise();
      mDetWnd->SetGeometry(x, y, DRAG_DET_WINDOW_WIDTH * 2, DRAG_DET_WINDOW_WIDTH * 2);
      g_debug("%s: show at (%d, %d, %d, %d)\n", __FUNCTION__, x, y, DRAG_DET_WINDOW_WIDTH * 2, DRAG_DET_WINDOW_WIDTH * 2);
      /*
       * Wiggle the mouse here. Especially for G->H DnD, this improves
       * reliability of making the drag escape the guest window immensly.
       * Stolen from the legacy V2 DnD code.
       */

      SendFakeMouseMove(x + 2, y + 2);
      mDetWnd->SetIsVisible(true);
   } else {
      g_debug("%s: hide\n", __FUNCTION__);
      mDetWnd->Hide();
      mDetWnd->SetIsVisible(false);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnUpdateUnityDetWnd --
 *
 *      Callback to show/hide fullscreen Unity drag detection window.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Detection window shown, hidden.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnUpdateUnityDetWnd(bool show,         // IN: show (true) or hide (false)
                              uint32 unityWndId, // IN: XXX ?
                              bool bottom)       // IN: place window at bottom of stack?
{
   g_debug("%s: enter 0x%lx unityID 0x%x\n",
         __FUNCTION__,
         (unsigned long) mDetWnd->GetWnd()->get_window()->gobj(),
         unityWndId);

   if (show && ((unityWndId > 0) || bottom)) {
      int width = mDetWnd->GetScreenWidth();
      int height = mDetWnd->GetScreenHeight();
      mDetWnd->SetGeometry(0, 0, width, height);
      mDetWnd->Show();
      if (bottom) {
         mDetWnd->Lower();
      }

      g_debug("%s: show, (0, 0, %d, %d)\n", __FUNCTION__, width, height);
   } else {
      if (mDetWnd->GetIsVisible() == true) {
         if (mUnityMode) {

            /*
             * Show and move detection window to current mouse position
             * and resize.
             */
            SendFakeXEvents(true, false, true, true, false, 0, 0);
         }
      } else {
         mDetWnd->Hide();
         g_debug("%s: hide\n", __FUNCTION__);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnDestMoveDetWndToMousePos --
 *
 *      Callback to move detection window to current moue position.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Detection window is moved, shown.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnDestMoveDetWndToMousePos()
{
   SendFakeXEvents(true, false, true, true, false, 0, 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnMoveMouse --
 *
 *      Callback to update mouse position.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Moves mouse.  Duh.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnMoveMouse(int32 x,  // IN: Pointer x-coord
                      int32 y)  // IN: Pointer y-coord
{
   // Position the pointer, and record its position.

   SendFakeXEvents(false, false, false, false, true, x, y);
   mMousePosX = x;
   mMousePosY = y;

   if (mDragCtx && !mGHDnDInProgress) {

      // If we are the context of a DnD, send DnD feedback to the source.

      DND_DROPEFFECT effect;
#ifndef GTK3
      effect = ToDropEffect((Gdk::DragAction)(mDragCtx->action));
#else
      effect = ToDropEffect((Gdk::DragAction)(gdk_drag_context_get_selected_action(mDragCtx)));
#endif
      if (effect != mEffect) {
         mEffect = effect;
         g_debug("%s: Updating feedback\n", __FUNCTION__);
         SourceUpdateFeedback(mEffect);
      }
   }
}


/*
 ****************************************************************************
 * BEGIN GTK+ Callbacks (dndcp as drag source: host-to-guest)
 */


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkDragMotion --
 *
 *      GTK "drag_motion" signal handler.
 *
 *      We should respond by setting drag status. Note that there is no drag
 *      enter signal. We need to figure out if a new drag is happening on
 *      our own. Also, we don't respond with a "allowed" drag status right
 *      away, we start a new drag operation with the host (which tries to
 *      notify the host of the new operation). Once the host has responded),
 *      we respond with a proper drag status.
 *
 *      Note: You may see this callback during DnD when detection window
 *      is acting as a source. In that case it will be ignored. In a future
 *      refactoring, we will try and avoid this.
 *
 * Results:
 *      Returns true unless we don't recognize the types offered.
 *
 * Side effects:
 *      Via RequestData issues a Gtk::Widget::drag_get_data.
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDUIX11::OnGtkDragMotion(
   const Glib::RefPtr<Gdk::DragContext> &dc,    // IN: GTK drag context
   int x,                                       // IN: drag motion x-coord
   int y,                                       // IN: drag motion y-coord
   guint timeValue)                             // IN: event timestamp
{
   /*
    * If this is a Host to Guest drag, we are done here, so return.
    */
   unsigned long curTime = GetTimeInMillis();
   g_debug("%s: enter dc %p, mDragCtx %p\n", __FUNCTION__,
         dc ? dc->gobj() : NULL, mDragCtx ? mDragCtx : NULL);
   if (curTime - mDestDropTime <= 1000) {
      g_debug("%s: ignored %ld %ld %ld\n", __FUNCTION__,
            curTime, mDestDropTime, curTime - mDestDropTime);
      return true;
   }

   g_debug("%s: not ignored %ld %ld %ld\n", __FUNCTION__,
         curTime, mDestDropTime, curTime - mDestDropTime);

   if (mInHGDrag || (mHGGetFileStatus != DND_FILE_TRANSFER_NOT_STARTED)) {
      g_debug("%s: ignored not in hg drag or not getting hg data\n", __FUNCTION__);
      return true;
   }

   Gdk::DragAction srcActions;
   Gdk::DragAction suggestedAction;
   Gdk::DragAction dndAction = (Gdk::DragAction)0;
   Glib::ustring target = mDetWnd->GetWnd()->drag_dest_find_target(dc);

   if (!mDnD->IsDnDAllowed()) {
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
#ifndef GTK3
   Glib::ListHandle<std::string, Gdk::AtomStringTraits> targets(
      dc->gobj()->targets, Glib::OWNERSHIP_NONE);
#else
   Glib::ListHandle<std::string, Gdk::AtomStringTraits> targets(
      gdk_drag_context_list_targets(dc->gobj()), Glib::OWNERSHIP_NONE);
#endif

   std::vector<Glib::ustring> as = targets;
   std::vector<Glib::ustring>::iterator result;
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

   mDragCtx = dc->gobj();

   if (target != Gdk::AtomString::to_cpp_type(GDK_NONE)) {
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
      if (!mGHDnDInProgress) {
         g_debug("%s: new drag, need to get data for host\n", __FUNCTION__);
         /*
          * This is a new drag operation. We need to start a drag thru the
          * backdoor, and to the host. Before we can tell the host, we have to
          * retrieve the drop data.
          */
         mGHDnDInProgress = true;
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


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkDragLeave --
 *
 *      GTK+ "drag_leave" signal handler.
 *
 *      Logs event.  Acknowledges, finishes outdated sequence if drag context
 *      is not the same as we're currently interested in (i.e. != mDragCtx).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May cancel dnd associated with this drag context.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnGtkDragLeave(
   const Glib::RefPtr<Gdk::DragContext> &dc,    // IN: GTK drag context
   guint time)                                  // IN: event timestamp
{
   g_debug("%s: enter dc %p, mDragCtx %p\n", __FUNCTION__,
         dc ? dc->gobj() : NULL, mDragCtx ? mDragCtx : NULL);

   /*
    * If we reach here after reset DnD, or we are getting a late
    * DnD drag leave signal (we have started another DnD), then
    * finish the old DnD. Otherwise, Gtk will not reset and a new
    * DnD will not start until Gtk+ times out (which appears to
    * be 5 minutes).
    * See http://bugzilla.eng.vmware.com/show_bug.cgi?id=528320
    */
   if (!mDragCtx || dc->gobj() != mDragCtx) {
      g_debug("%s: calling drag_finish\n", __FUNCTION__);
      dc->drag_finish(true, false, time);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkDragBegin --
 *
 *      GTK+ "drag_begin" signal handler.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Records drag context for later use.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnGtkDragBegin(
   const Glib::RefPtr<Gdk::DragContext>& context)       // IN: GTK drag context
{
   g_debug("%s: enter dc %p, mDragCtx %p\n", __FUNCTION__,
         context ? context->gobj() : NULL, mDragCtx ? mDragCtx : NULL);
   mDragCtx = context->gobj();
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkDragDataGet --
 *
 *      GTK+ "drag_data_get" signal handler.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May insert vmblock blocking entry and request host-to-guest file transfer from
 *      host.
 *
 *      If unable to obtain drag information, may instead cancel the DND
 *      operation.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnGtkDragDataGet(
   const Glib::RefPtr<Gdk::DragContext> &dc,    // IN: GTK drag context
   Gtk::SelectionData& selection_data,          // IN: drag details
   guint info,                                  // UNUSED
   guint time)                                  // IN: event timestamp
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

   g_debug("%s: enter dc %p, mDragCtx %p with target %s\n", __FUNCTION__,
           dc ? dc->gobj() : NULL, mDragCtx ? mDragCtx : NULL,
           target.c_str());

   if (!mInHGDrag) {
      g_debug("%s: not in drag, return\n", __FUNCTION__);
      return;
   }

   if (   target == DRAG_TARGET_NAME_URI_LIST
       && CPClipboard_GetItem(&mClipboard, CPFORMAT_FILELIST, &buf, &sz)) {

      /* Provide path within vmblock file system instead of actual path. */
      stagingDirName = GetLastDirName(mHGStagingDir);
      if (stagingDirName.length() == 0) {
         g_debug("%s: Cannot get staging directory name, stagingDir: %s\n",
                 __FUNCTION__, mHGStagingDir.c_str());
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
         if (DnD_BlockIsReady(mBlockCtrl)) {
            uriList += mBlockCtrl->blockRoot;
            uriList += DIRSEPS + stagingDirName + DIRSEPS + str + post;
         } else {
            uriList += DIRSEPS + mHGStagingDir + DIRSEPS + str + post;
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
      if (   !mBlockAdded
          &&  mInHGDrag
          && (mHGGetFileStatus == DND_FILE_TRANSFER_NOT_STARTED)) {
         mHGGetFileStatus = DND_FILE_TRANSFER_IN_PROGRESS;
         AddBlock();
      } else {
         g_debug("%s: not calling AddBlock\n", __FUNCTION__);
      }
      selection_data.set(DRAG_TARGET_NAME_URI_LIST, uriList.c_str());
      g_debug("%s: providing uriList [%s]\n", __FUNCTION__, uriList.c_str());
      return;
   }

   if (   target == DRAG_TARGET_NAME_URI_LIST
       && CPClipboard_ItemExists(&mClipboard, CPFORMAT_FILECONTENTS)) {
      g_debug("%s: Providing uriList [%s] for file contents DnD\n",
            __FUNCTION__, mHGFileContentsUriList.c_str());

      selection_data.set(DRAG_TARGET_NAME_URI_LIST,
                         mHGFileContentsUriList.c_str());
      return;
   }

   if (   TargetIsPlainText(target)
       && CPClipboard_GetItem(&mClipboard, CPFORMAT_TEXT, &buf, &sz)) {
      g_debug("%s: providing plain text, size %" FMTSZ "u\n", __FUNCTION__, sz);
      selection_data.set(target.c_str(), (const char *)buf);
      return;
   }

   if (   TargetIsRichText(target)
       && CPClipboard_GetItem(&mClipboard, CPFORMAT_RTF, &buf, &sz)) {
      g_debug("%s: providing rtf text, size %" FMTSZ "u\n", __FUNCTION__, sz);
      selection_data.set(target.c_str(), (const char *)buf);
      return;
   }

   /* Can not get any valid data, cancel this HG DnD. */
   g_debug("%s: no valid data for HG DnD\n", __FUNCTION__);
   ResetUI();
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkDragEnd --
 *
 *      GTK+ "drag_end" signal handler.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May reset UI state.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnGtkDragEnd(
   const Glib::RefPtr<Gdk::DragContext> &dc)    // IN: GTK drag context
{
   g_debug("%s: entering dc %p, mDragCtx %p\n", __FUNCTION__,
         dc ? dc->gobj() : NULL, mDragCtx ? mDragCtx : NULL);

   /*
    * We may see a drag end for the previous DnD, but after a new
    * DnD has started. If so, ignore it.
    */
   if (mDragCtx && dc && (mDragCtx != dc->gobj())) {
      g_debug("%s: got old dc (new DnD started), ignoring\n", __FUNCTION__);
      return;
   }

   /*
    * If we are a file DnD and file transfer is not done yet, don't call
    * ResetUI() here, since we will do so in the fileCopyDoneChanged
    * callback.
    */
   if (DND_FILE_TRANSFER_IN_PROGRESS != mHGGetFileStatus) {
      ResetUI();
   }
   mInHGDrag = false;
}


/*
 * END GTK+ Callbacks (dndcp as drag source: host-to-guest)
 ****************************************************************************
 */


/*
 ****************************************************************************
 * BEGIN GTK+ Callbacks (dndcp as drag destination: guest-to-host)
 */


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkDragDataReceived --
 *
 *      GTK+ "drag_data_received" signal handler.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May signal to host beginning of guest-to-host DND.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnGtkDragDataReceived(
   const Glib::RefPtr<Gdk::DragContext> &dc,    // IN: GTK drag context
   int x,                                       // IN: drop location x-coord
   int y,                                       // IN: drop location y-coord
   const Gtk::SelectionData& sd,                // IN: drag content details
   guint info,                                  // UNUSED
   guint time)                                  // IN: event timestamp
{
   g_debug("%s: enter dc %p, mDragCtx %p\n", __FUNCTION__,
         dc ? dc->gobj() : NULL, mDragCtx ? mDragCtx : NULL);
   /* The GH DnD may already finish before we got response. */
   if (!mGHDnDInProgress) {
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
      ResetUI();
      return;
   }

   mNumPendingRequest--;
   if (mNumPendingRequest > 0) {
      return;
   }

   if (CPClipboard_IsEmpty(&mClipboard)) {
      g_debug("%s: Failed getting item.\n", __FUNCTION__);
      ResetUI();
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
   if (!mGHDnDDataReceived) {
      g_debug("%s: Drag entering.\n", __FUNCTION__);
      mGHDnDDataReceived = true;
      TargetDragEnter();
   } else {
      g_debug("%s: not !mGHDnDDataReceived\n", __FUNCTION__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkDragDrop --
 *
 *      GTK+ "drag_drop" signal handler.
 *
 * Results:
 *      Returns true so long as drag target and data are (at one point)
 *      provided (i.e. not a spurious event).
 *
 * Side effects:
 *      Signals to drag source that drop is finished.
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDUIX11::OnGtkDragDrop(
   const Glib::RefPtr<Gdk::DragContext> &dc,    // IN: GTK drag context
   int x,                                       // IN: drop location x-coord
   int y,                                       // IN: drop location y-coord
   guint time)                                  // IN: motion event timestamp
{
   g_debug("%s: enter dc %p, mDragCtx %p x %d y %d\n", __FUNCTION__,
         (dc ? dc->gobj() : NULL), (mDragCtx ? mDragCtx : NULL), x, y);

   Glib::ustring target;

   target = mDetWnd->GetWnd()->drag_dest_find_target(dc);
   g_debug("%s: calling drag_finish\n", __FUNCTION__);
   dc->drag_finish(true, false, time);

   if (target == Gdk::AtomString::to_cpp_type(GDK_NONE)) {
      g_debug("%s: No valid data on clipboard.\n", __FUNCTION__);
      return false;
   }

   if (CPClipboard_IsEmpty(&mClipboard)) {
      g_debug("%s: No valid data on mClipboard.\n", __FUNCTION__);
      return false;
   }

   return true;
}


/*
 * END GTK+ Callbacks (dndcp as drag destination: guest-to-host)
 ****************************************************************************
 */


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::SetCPClipboardFromGtk --
 *
 *      Construct cross-platform clipboard from GTK+ selection_data.
 *
 * Results:
 *      Returns true if conversion succeeded, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
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
   if (mDnD->CheckCapability(DND_CP_CAP_FILE_DND) && target == DRAG_TARGET_NAME_URI_LIST) {
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
#if defined(__linux__)
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
#if defined(__linux__)
         char *newUri = HgfsUri_ConvertFromPathToHgfsUri(newPath, false);
         fileList.AddFileUri(newUri);
         free(newUri);
#endif
         free(newPath);
      }

      DynBuf_Init(&buf);
      fileList.SetFileSize(totalSize);
      if (fileList.ToCPClipboard(&buf, false)) {
          CPClipboard_SetItem(&mClipboard, CPFORMAT_FILELIST, DynBuf_Get(&buf),
                              DynBuf_GetSize(&buf));
      }
      DynBuf_Destroy(&buf);
#if defined(__linux__)
      if (fileList.ToUriClipboard(&buf)) {
         CPClipboard_SetItem(&mClipboard, CPFORMAT_FILELIST_URI, DynBuf_Get(&buf),
                             DynBuf_GetSize(&buf));
      }
      DynBuf_Destroy(&buf);
#endif
      return true;
   }

   /* Try to get plain text. */
   if (   mDnD->CheckCapability(DND_CP_CAP_PLAIN_TEXT_DND)
       && TargetIsPlainText(target)) {
      std::string source = sd.get_data_as_string();
      if (   source.size() > 0
          && source.size() < DNDMSG_MAX_ARGSZ
          && CPClipboard_SetItem(&mClipboard, CPFORMAT_TEXT, source.c_str(),
                                 source.size() + 1)) {
         g_debug("%s: Got text, size %" FMTSZ "u\n", __FUNCTION__, source.size());
      } else {
         g_debug("%s: Failed to get text\n", __FUNCTION__);
         return false;
      }
      return true;
   }

   /* Try to get RTF string. */
   if (   mDnD->CheckCapability(DND_CP_CAP_RTF_DND)
       && TargetIsRichText(target)) {
      std::string source = sd.get_data_as_string();
      if (   source.size() > 0
          && source.size() < DNDMSG_MAX_ARGSZ
          && CPClipboard_SetItem(&mClipboard, CPFORMAT_RTF, source.c_str(),
                                 source.size() + 1)) {
         g_debug("%s: Got RTF, size %" FMTSZ "u\n", __FUNCTION__, source.size());
         return true;
      } else {
         g_debug("%s: Failed to get text\n", __FUNCTION__ );
         return false;
      }
   }
   return true;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::RequestData --
 *
 *      Requests clipboard data from a drag source.
 *
 *      Evaluates targets (think MIME types) offered by the drag source, and
 *      if we support any, requests the contents.
 *
 * Results:
 *      Returns true if we found a supported type.
 *
 * Side effects:
 *      May call drag_get_data.
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDUIX11::RequestData(
   const Glib::RefPtr<Gdk::DragContext> &dc, // IN: GTK drag context
   guint time)                               // IN: event timestamp
{
   Glib::RefPtr<Gtk::TargetList> targets;
   targets = Gtk::TargetList::create(std::vector<Gtk::TargetEntry>());

   CPClipboard_Clear(&mClipboard);
   mNumPendingRequest = 0;

   Glib::ustring noneType = Gdk::AtomString::to_cpp_type(GDK_NONE);

   /*
    * First check file list. If file list is available, all other formats will
    * be ignored.
    */
   targets->add(Glib::ustring(DRAG_TARGET_NAME_URI_LIST));
   Glib::ustring target = mDetWnd->GetWnd()->drag_dest_find_target(dc, targets);
   targets->remove(Glib::ustring(DRAG_TARGET_NAME_URI_LIST));
   if (target != noneType) {
      mDetWnd->GetWnd()->drag_get_data(dc, target, time);
      mNumPendingRequest++;
      return true;
   }

   /* Then check plain text. */
   targets->add(Glib::ustring(TARGET_NAME_UTF8_STRING));
   targets->add(Glib::ustring(TARGET_NAME_STRING));
   targets->add(Glib::ustring(TARGET_NAME_TEXT_PLAIN));
   targets->add(Glib::ustring(TARGET_NAME_COMPOUND_TEXT));
   target = mDetWnd->GetWnd()->drag_dest_find_target(dc, targets);
   targets->remove(Glib::ustring(TARGET_NAME_STRING));
   targets->remove(Glib::ustring(TARGET_NAME_TEXT_PLAIN));
   targets->remove(Glib::ustring(TARGET_NAME_UTF8_STRING));
   targets->remove(Glib::ustring(TARGET_NAME_COMPOUND_TEXT));
   if (target != noneType) {
      mDetWnd->GetWnd()->drag_get_data(dc, target, time);
      mNumPendingRequest++;
   }

   /* Then check RTF. */
   targets->add(Glib::ustring(TARGET_NAME_APPLICATION_RTF));
   targets->add(Glib::ustring(TARGET_NAME_TEXT_RICHTEXT));
   targets->add(Glib::ustring(TARGET_NAME_TEXT_RTF));
   target = mDetWnd->GetWnd()->drag_dest_find_target(dc, targets);
   targets->remove(Glib::ustring(TARGET_NAME_APPLICATION_RTF));
   targets->remove(Glib::ustring(TARGET_NAME_TEXT_RICHTEXT));
   targets->remove(Glib::ustring(TARGET_NAME_TEXT_RTF));
   if (target != noneType) {
      mDetWnd->GetWnd()->drag_get_data(dc, target, time);
      mNumPendingRequest++;
   }
   return (mNumPendingRequest > 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::GetLastDirName --
 *
 *      Try to get last directory name from a full path name.
 *
 *      What this really means is to get the basename of the parent's directory
 *      name, intended to isolate an individual DND operation's staging directory
 *      name.
 *
 *         E.g. /tmp/VMwareDnD/abcd137/foo → abcd137
 *
 * Results:
 *      Returns session directory name on success, empty string otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
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


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::GetNextPath --
 *
 *      Convoluted somethingerother.
 *
 *      XXX Something here involves URI parsing and encoding.  Get to the bottom
 *      of this and use shared URI code.
 *
 *      Original description:
 *         Provide a substring containing the next path from the provided
 *         NUL-delimited string starting at the provided index.
 *
 * Results:
 *      Returns "a string with the next path or empty string if there are no
 *      more paths".
 *
 * Side effects:
 *      Updates index.
 *
 *-----------------------------------------------------------------------------
 */

utf::utf8string
DnDUIX11::GetNextPath(utf::utf8string& str,     // IN: NUL-delimited path list
                      size_t& index)            // IN/OUT: index into string
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


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::SendFakeMouseMove --
 *
 *      Issue a fake mouse move event to the detection window.
 *
 * Results:
 *      Returns true on success, false on failure.
 *
 * Side effects:
 *      Generates mouse events.
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDUIX11::SendFakeMouseMove(const int x,        // IN: x-coord
                            const int y)        // IN: y-coord
{
   return SendFakeXEvents(false, false, false, false, true, x, y);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::SendFakeXEvents --
 *
 *      Fake X mouse events and window movement for the detection window.
 *
 *      This function shows the detection window and generates button
 *      press/release and pointer motion events.
 *
 *      XXX This code should be implemented using GDK APIs.
 *          (gdk_display_warp_pointer?)
 *
 *      XXX This code should be moved into the detection window class
 *
 * Results:
 *      Returns true if generated X events, false on failure.
 *
 * Side effects:
 *      A ton of things.
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDUIX11::SendFakeXEvents(
   const bool showWidget,       // IN: whether to show widget
   const bool buttonEvent,      // IN: whether to send a button event
   const bool buttonPress,      // IN: whether button event is press or release
   const bool moveWindow,       // IN: whether to move detection window
   const bool coordsProvided,   // IN: whether coords provided, else will
                                //     query current mouse position
   const int xCoord,            // IN: destination x-coord
   const int yCoord)            // IN: destination y-coord
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

   TRACE_CALL();

   x = xCoord;
   y = yCoord;

   widget = GetDetWndAsWidget();

   if (!widget) {
      g_debug("%s: unable to get widget\n", __FUNCTION__);
      return false;
   }
#ifndef GTK3
   dndXDisplay = GDK_WINDOW_XDISPLAY(widget->window);
   dndXWindow = GDK_WINDOW_XWINDOW(widget->window);
#else
   dndXDisplay = GDK_WINDOW_XDISPLAY(gtk_widget_get_window(widget));
   dndXWindow = GDK_WINDOW_XID(gtk_widget_get_window(widget));
#endif
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
#ifndef GTK3
      gdk_window_show(widget->window);
#else
      gdk_window_show(gtk_widget_get_window(widget));
#endif
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
      int width = mDetWnd->GetScreenWidth();
      int height = mDetWnd->GetScreenHeight();
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
      XMoveResizeWindow(dndXDisplay,
                        dndXWindow,
                        x - DRAG_DET_WINDOW_WIDTH / 2 ,
                        y - DRAG_DET_WINDOW_WIDTH / 2,
                        DRAG_DET_WINDOW_WIDTH,
                        DRAG_DET_WINDOW_WIDTH);
      XRaiseWindow(dndXDisplay, dndXWindow);
      g_debug("%s: move wnd to (%d, %d, %d, %d)\n",
              __FUNCTION__,
              x - DRAG_DET_WINDOW_WIDTH / 2 ,
              y - DRAG_DET_WINDOW_WIDTH / 2,
              DRAG_DET_WINDOW_WIDTH,
              DRAG_DET_WINDOW_WIDTH);
   }

   /*
    * Generate mouse movements over the window.  The second one makes ungrabs
    * happen more reliably on KDE, but isn't necessary on GNOME.
    */
   if (mUseUInput) {
#ifdef USE_UINPUT
      FakeMouse_Move(x, y);
      FakeMouse_Move(x + 1, y + 1);
#endif
   } else {
      XTestFakeMotionEvent(dndXDisplay, -1, x, y, CurrentTime);
      XTestFakeMotionEvent(dndXDisplay, -1, x + 1, y + 1, CurrentTime);
   }
   g_debug("%s: move mouse to (%d, %d) and (%d, %d)\n", __FUNCTION__, x, y, x + 1, y + 1);

   if (buttonEvent) {
      g_debug("%s: faking left mouse button %s\n", __FUNCTION__,
              buttonPress ? "press" : "release");
      if (mUseUInput) {
#ifdef USE_UINPUT
         FakeMouse_Click(buttonPress);
#endif
      } else {
         XTestFakeButtonEvent(dndXDisplay, 1, buttonPress, CurrentTime);
         XSync(dndXDisplay, False);
      }

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

         if (   (maskReturn & Button1Mask)
             || (maskReturn & Button2Mask)
             || (maskReturn & Button3Mask)
             || (maskReturn & Button4Mask)
             || (maskReturn & Button5Mask)) {
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


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::TryXTestFakeDeviceButtonEvent --
 *
 *      Fake X mouse events in device level.
 *
 *      XXX The function will only be called if XTestFakeButtonEvent does
 *      not work for mouse button release. Later on we may only call this
 *      one for mouse button simulation if this is more reliable.
 *
 * Results:
 *      Returns true on success, false on failure.
 *
 * Side effects:
 *      Generates mouse events.
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDUIX11::TryXTestFakeDeviceButtonEvent()
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
#ifndef GTK3
   dndXDisplay = GDK_WINDOW_XDISPLAY(widget->window);
#else
   dndXDisplay = GDK_WINDOW_XDISPLAY(gtk_widget_get_window(widget));
#endif

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


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::GetDetWndAsWidget --
 *
 *      Get the GtkWidget pointer for a DragDetWnd object.
 *
 *      The X11 Unity implementation requires access to the drag detection
 *      window as a GtkWindow pointer, which it uses to show and hide the
 *      detection window.
 *
 *      This function is also called by the code that issues fake X events
 *      to the detection window.
 *
 * Results:
 *      A GtkWidget* on success or NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget *
DnDUIX11::GetDetWndAsWidget()
{
   GtkWidget *widget = NULL;

   if (!mDetWnd) {
      return NULL;
   }

   if (mDetWnd->GetWnd()->gobj()) {
      widget = GTK_WIDGET(mDetWnd->GetWnd()->gobj());
   }
   return widget;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::AddBlock --
 *
 *      Insert a vmblock blocking entry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Caller must pair with RemoveBlock() upon dnd completion/cancellation.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::AddBlock()
{
   TRACE_CALL();

   if (mBlockAdded) {
      g_debug("%s: block already added\n", __FUNCTION__);
      return;
   }

   g_debug("%s: DnDBlockIsReady %d fd %d\n", __FUNCTION__,
           DnD_BlockIsReady(mBlockCtrl), mBlockCtrl->fd);

   if (   DnD_BlockIsReady(mBlockCtrl)
       && mBlockCtrl->AddBlock(mBlockCtrl->fd, mHGStagingDir.c_str())) {
      mBlockAdded = true;
      g_debug("%s: add block for %s.\n", __FUNCTION__, mHGStagingDir.c_str());
   } else {
      mBlockAdded = false;
      g_debug("%s: unable to add block dir %s.\n", __FUNCTION__, mHGStagingDir.c_str());
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::RemoveBlock --
 *
 *      Remove a vmblock blocking entry.
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
DnDUIX11::RemoveBlock()
{
   TRACE_CALL();

   if (mBlockAdded && (DND_FILE_TRANSFER_IN_PROGRESS != mHGGetFileStatus)) {
      g_debug("%s: removing block for %s\n", __FUNCTION__, mHGStagingDir.c_str());
      /* We need to make sure block subsystem has not been shut off. */
      if (DnD_BlockIsReady(mBlockCtrl)) {
         mBlockCtrl->RemoveBlock(mBlockCtrl->fd, mHGStagingDir.c_str());
      }
      mBlockAdded = false;
   } else {
      g_debug("%s: not removing block mBlockAdded %d mHGGetFileStatus %d\n",
            __FUNCTION__,
            mBlockAdded,
            mHGGetFileStatus);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::ToDropEffect --
 *
 *      Convert a Gdk::DragAction value to its corresponding DND_DROPEFFECT.
 *
 * Results:
 *      Returns corresponding DND_DROPEFFECT or DROP_UNKNOWN if a match isn't
 *      found.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

/* static */ DND_DROPEFFECT
DnDUIX11::ToDropEffect(const Gdk::DragAction action)
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


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::WriteFileContentsToStagingDir --
 *
 *      Try to extract file contents from mClipboard. Write all files to a
 *      temporary staging directory. Construct uri list.
 *
 * Results:
 *      Returns true on success, false on failure.
 *
 * Side effects:
 *      Hm?
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDUIX11::WriteFileContentsToStagingDir()
{
   void *buf = NULL;
   size_t sz = 0;
   XDR xdrs;
   CPFileContents fileContents;
   CPFileContentsList *contentsList = NULL;
   size_t nFiles = 0;
   CPFileItem *fileItem = NULL;
   char *tempDir = NULL;
   size_t i = 0;
   bool ret = false;

   if (!CPClipboard_GetItem(&mClipboard, CPFORMAT_FILECONTENTS, &buf, &sz)) {
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

   mHGFileContentsUriList = "";

   for (i = 0; i < nFiles; i++) {
      utf::string fileName;
      utf::string filePathName;
      VmTimeType createTime = -1;
      VmTimeType accessTime = -1;
      VmTimeType writeTime = -1;
      VmTimeType attrChangeTime = -1;

      if (!fileItem[i].cpName.cpName_val ||
          0 == fileItem[i].cpName.cpName_len) {
         g_debug("%s: invalid fileItem[%" FMTSZ "u].cpName.\n", __FUNCTION__, i);
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

      if (   fileItem[i].validFlags & CP_FILE_VALID_TYPE
          && CP_FILE_TYPE_DIRECTORY == fileItem[i].type) {
         if (!File_CreateDirectory(filePathName.c_str())) {
            goto exit;
         }
         g_debug("%s: created directory [%s].\n", __FUNCTION__, filePathName.c_str());
      } else if (   fileItem[i].validFlags & CP_FILE_VALID_TYPE
                 && CP_FILE_TYPE_REGULAR == fileItem[i].type) {
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
         g_debug("%s: created file [%s].\n", __FUNCTION__, filePathName.c_str());
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
         g_debug("%s: File_SetTimes failed with file [%s].\n", __FUNCTION__,
                 filePathName.c_str());
      }

      /* Update file permission attributes. */
      if (fileItem->validFlags & CP_FILE_VALID_PERMS) {
         if (Posix_Chmod(filePathName.c_str(), fileItem->permissions) < 0) {
            /* Not a critical error, only log it. */
            g_debug("%s: Posix_Chmod failed with file [%s].\n", __FUNCTION__,
                    filePathName.c_str());
         }
      }

      /*
       * If there is no DIRSEPC inside the fileName, this file/directory is a
       * top level one. We only put top level name into uri list.
       */
      if (fileName.find(DIRSEPS, 0) == utf::string::npos) {
         mHGFileContentsUriList += "file://" + filePathName + "\r\n";
      }
   }
   g_debug("%s: created uri list [%s].\n", __FUNCTION__, mHGFileContentsUriList.c_str());
   ret = true;

exit:
   xdr_free((xdrproc_t) xdr_CPFileContents, (char *)&fileContents);
   if (tempDir && !ret) {
      DnD_DeleteStagingFiles(tempDir, false);
   }
   free(tempDir);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::SourceDragStartDone --
 *
 *      Tell host that we're done with host-to-guest drag-and-drop
 *      initialization.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::SourceDragStartDone()
{
   TRACE_CALL();
   mInHGDrag = true;
   mDnD->SrcUIDragBeginDone();
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::SetBlockControl --
 *
 *      Set block control member.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::SetBlockControl(DnDBlockControl *blockCtrl)   // IN
{
   mBlockCtrl = blockCtrl;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::SourceUpdateFeedback --
 *
 *      Forward feedback from our drop source to the host.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::SourceUpdateFeedback(
   DND_DROPEFFECT effect) // IN: feedback to send to the UI-independent DnD layer.
{
   TRACE_CALL();
   mDnD->SrcUIUpdateFeedback(effect);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::TargetDragEnter --
 *
 *      With the source's drag selection data on the clipboard, signal to
 *      host to begin guest-to-host drag-and-drop.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::TargetDragEnter()
{
   TRACE_CALL();

   /* Check if there is valid data with current detection window. */
   if (!CPClipboard_IsEmpty(&mClipboard)) {
      g_debug("%s: got valid data from detWnd.\n", __FUNCTION__);
      mDnD->DestUIDragEnter(&mClipboard);
   }

   /*
    * Show the window, and position it under the current mouse position.
    * This is particularly important for KDE 3.5 guests.
    */
   SendFakeXEvents(true, false, true, true, false, 0, 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::GetTimeInMillis --
 *
 *      Get Unix time in milliseconds.
 *
 *-----------------------------------------------------------------------------
 */

/* static */ unsigned long
DnDUIX11::GetTimeInMillis()
{
   VmTimeType atime;

   Hostinfo_GetTimeOfDay(&atime);
   return((unsigned long)(atime / 1000));
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::VmXDnDVersionChanged --
 *
 *      Update version information in mDnD.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::VmxDnDVersionChanged(RpcChannel *chan,        // IN
                               uint32 version)          // IN
{
   ASSERT(mDnD);
   mDnD->VmxDnDVersionChanged(version);
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
   TRACE_CALL();
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
   TRACE_CALL();
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
   TRACE_CALL();
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
   TRACE_CALL();
   return true;
}


/**
 * Track realize events on detection window.
 */

void
DnDUIX11::GtkRealizeEventCB()
{
   TRACE_CALL();
}


/**
 * Track unrealize events on detection window.
 */

void
DnDUIX11::GtkUnrealizeEventCB()
{
   TRACE_CALL();
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
   TRACE_CALL();
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
   TRACE_CALL();
   return true;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnWorkAreaChanged --
 *
 *      Updates mOrigin in response to changes to _NET_workArea.
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
DnDUIX11::OnWorkAreaChanged(Glib::RefPtr<Gdk::Screen> screen)    // IN
{
   TRACE_CALL();

   std::vector<unsigned long> values;
   if (   xutils::GetCardinalList(screen->get_root_window(), "_NET_WORKAREA", values)
       && values.size() > 0
       && values.size() % 4 == 0) {
      /*
       * wm-spec: _NET_WORKAREA, x, y, width, height CARDINAL[][4]/32
       *
       * For the purposes of drag-and-drop, we're okay with using the screen-
       * agnostic _NET_WORKAREA atom, as the guest VM really deals with only
       * one logical monitor.
       */
      unsigned long desktop = 0;
      xutils::GetCardinal(screen->get_root_window(), "_NET_CURRENT_DESKTOP", desktop);

      mOrigin.set_x(values[0 + 4 * desktop]);
      mOrigin.set_y(values[1 + 4 * desktop]);
   } else {
      mOrigin.set_x(0);
      mOrigin.set_y(0);
   }

   g_debug("%s: new origin at (%d, %d)\n", __FUNCTION__, mOrigin.get_x(),
           mOrigin.get_y());
}
