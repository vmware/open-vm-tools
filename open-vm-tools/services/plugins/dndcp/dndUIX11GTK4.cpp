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

/**
 * @file dndUIX11GTK4.cpp --
 *
 * This class implements stubs for the methods that allow DnD between
 * host and guest on GTK4 lib.
 */

#define G_LOG_DOMAIN "dndcp"

#include "xutils/xutilsGTK4.hh"
#include "copyPasteDnDUtil.h"
#include "dndUIX11GTK4.h"
#include "guestDnDCPMgr.hh"
#include "tracer.hh"
#include "fakeMouseWayland/fakeMouseWayland.h"

extern "C" {
#include "vmware/guestrpc/tclodefs.h"

#include "copyPasteCompat.h"
#include "cpName.h"
#include "cpNameUtil.h"
#include "dndClipboard.h"
#include "hgfsUri.h"
#include "rpcout.h"
}

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
      mClipboard(),
      mBlockCtrl(NULL),
      mHGGetFileStatus(DND_FILE_TRANSFER_NOT_STARTED),
      mBlockAdded(false),
      mGHDnDInProgress(false),
      mGHDnDDataReceived(false),
      mInHGDrag(false),
      mEffect(DROP_NONE),
      mMousePosX(0),
      mMousePosY(0),
      mNumPendingRequest(0),
      mDestDropTime(0),
      mTotalFileSize(0),
      mOriginX(0),
      mOriginY(0),
      mGdkSurface(NULL)
{
   TRACE_CALL();


   /*
    * XXX Hard coded use of default screen means this doesn't work in dual-
    * headed setups (e.g. DISPLAY=:0.1).  However, the number of people running
    * such setups in VMs is expected to be, like, hella small, so I'mma cut
    * corners for now.
    */
   Glib::RefPtr<Gdk::Display> display = Gdk::Display::get_default();
   GdkSurface *gdkSurface = gdk_x11_display_get_default_group(display->gobj());
   mGdkSurface = Glib::wrap(gdkSurface);
   UpdateWorkArea();
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
      mDetWnd = NULL;
   }
   CPClipboard_Destroy(&mClipboard);
   /* Any files from last unfinished file transfer should be deleted. */
   if (DND_FILE_TRANSFER_IN_PROGRESS == mHGGetFileStatus
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

   mDetWnd = new DragDetWnd(mCtx->uinputFD);
   if (!mDetWnd) {
      g_debug("%s: unable to allocate DragDetWnd object\n", __FUNCTION__);
      goto fail;
   }

   InitGtk();

#define CONNECT_SIGNAL(_obj, _sig, _cb) \
   _obj->_sig.connect(sigc::mem_fun(this, &DnDUIX11::_cb))
   /* Set common layer callbacks. */
   CONNECT_SIGNAL(mDnD, srcDragBeginChanged,   OnSrcDragBegin);
   CONNECT_SIGNAL(mDnD, srcCancelChanged,      OnSrcCancel);
   CONNECT_SIGNAL(mDnD, getFilesDoneChanged,   OnGetFilesDone);
   CONNECT_SIGNAL(mDnD, moveMouseChanged,      OnMoveMouse);
   CONNECT_SIGNAL(mDnD, srcDropChanged,        OnSrcDrop);
   CONNECT_SIGNAL(mDnD, destCancelChanged,     OnDestCancel);
   CONNECT_SIGNAL(mDnD, destMoveDetWndToMousePosChanged, OnDestMoveDetWndToMousePos);
   CONNECT_SIGNAL(mDnD, privDropChanged,       OnPrivateDrop);
   CONNECT_SIGNAL(mDnD, updateDetWndChanged,   OnUpdateDetWnd);
   /* TODO: Unity related functions are deprecated, remove it later */
   CONNECT_SIGNAL(mDnD, updateUnityDetWndChanged, OnUpdateUnityDetWnd);
#undef CONNECT_SIGNAL

   mDetWnd->UpdateDetWnd(false, 0, 0);
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

   mDetWnd->register_source_drag_begin_handler(OnGtkSrcDragBegin, (void *)this);
   mDetWnd->register_source_drag_end_handler(OnGtkSrcDragEnd, (void *)this);
   mDetWnd->register_source_drag_cancel_handler(OnGtkSrcDragCancel, (void *)this);
   mDetWnd->register_target_drag_accept_handler(OnGtkDragAccept, (void *)this);
   mDetWnd->register_target_drag_enter_handler(OnGtkDragEnter, (void *)this);
   mDetWnd->register_target_drag_drop_handler(OnGtkDragDrop, (void *)this);
   mDetWnd->register_target_drag_leave_handler(OnGtkDragLeave, (void *)this);
   mDetWnd->register_target_drag_motion_handler(OnGtkDragMotion, (void *)this);
   mDetWnd->register_target_value_handler(OnGtkDragDataReceived, (void *)this);
   mDetWnd->EnableDND();
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
   RemoveBlock();
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
              __FUNCTION__, mBlockAdded, mHGGetFileStatus);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::LocalSetContentProvider --
 *
 *      Set content provider based on different desktop type.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      mProvider is chanaged.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::LocalSetContentProvider(const char* desktop)   // IN
{
   std::vector<Glib::RefPtr<Gdk::ContentProvider>> providerVec;
   Glib::RefPtr<Glib::Bytes> fileListGnome;
   Glib::RefPtr<Glib::Bytes> fileListKde;
   void *buf;
   size_t sz;

   g_debug("%s: enter.\n", __FUNCTION__);

   if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_FILELIST) ||
       CPClipboard_ItemExists(&mClipboard, CPFORMAT_FILECONTENTS)) {
      if (desktop && strstr(desktop, "GNOME")) {
         // Set the Gnome file list to GDKContentProvider
         fileListGnome = Glib::Bytes::create(mHGDndUriList.c_str(), mHGDndUriList.bytes());
         providerVec.push_back(Gdk::ContentProvider::create(DRAG_TARGET_NAME_URI_LIST, fileListGnome));
      } else if (desktop && strstr(desktop, "KDE")) {
         // Set the Kde file list to GDKContentProvider
         fileListKde = Glib::Bytes::create(mHGDndUriList.c_str(), mHGDndUriList.bytes());
         providerVec.push_back(Gdk::ContentProvider::create(DRAG_TARGET_NAME_URI_LIST, fileListKde));
      } else {
         g_warning("%s: Unsupported Desktop Manager %s. \n", __FUNCTION__, desktop);
         return;
      }
   } else if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_RTF)) {
      if (CPClipboard_GetItem(&mClipboard, CPFORMAT_RTF, &buf, &sz)) {
         g_debug("%s: RTF data, size %" FMTSZ "u.\n", __FUNCTION__, sz);
         Glib::RefPtr<Glib::Bytes> rtfData = Glib::Bytes::create(buf, sz);
         providerVec.push_back(Gdk::ContentProvider::create(TARGET_NAME_APPLICATION_RTF, rtfData));
         providerVec.push_back(Gdk::ContentProvider::create(TARGET_NAME_TEXT_RICHTEXT, rtfData));
         providerVec.push_back(Gdk::ContentProvider::create(TARGET_NAME_TEXT_RTF, rtfData));
      }
   } else if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_TEXT)) {
      if (CPClipboard_GetItem(&mClipboard, CPFORMAT_TEXT, &buf, &sz)) {
         g_debug("%s: Text data, size %" FMTSZ "u.\n", __FUNCTION__, sz);
         Glib::RefPtr<Glib::Bytes> textData = Glib::Bytes::create(buf, sz);
         providerVec.push_back(Gdk::ContentProvider::create(TARGET_NAME_TEXT_PLAIN, textData));
         providerVec.push_back(Gdk::ContentProvider::create(TARGET_NAME_TEXT_PLAIN_UTF8, textData));
      }
   }
   mProvider = Gdk::ContentProvider::create(providerVec);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::BuildFileURIList --
 *
 *      Build URI file list with proper pre/post str.
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
DnDUIX11::BuildFileURIList(utf::string& uriList,         // IN/OUT
                           utf::string pre,              // IN
                           utf::string post,             // IN
                           utf::string stagingDirName)   // IN
{
   size_t pos  = 0;
   utf::string str;

   while ((str = GetNextPath(mHGFCPData, pos).c_str()).bytes() != 0) {
      g_debug("%s: Path: %s", __FUNCTION__, str.c_str());
      uriList += pre;
      if (mBlockAdded) {
         uriList += mBlockCtrl->blockRoot;
         uriList += DIRSEPS + stagingDirName + DIRSEPS + str + post;
      } else {
         uriList += DIRSEPS + mHGStagingDir + DIRSEPS + str + post;
      }
   }
   g_debug("%s: providing uriList [%s]\n", __FUNCTION__, uriList.c_str());
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::BuildFileContentURIList --
 *
 *      Used to populate the file content URI lists for GNOME, Nautilus, and KDE.
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
DnDUIX11::BuildFileContentURIList(utf::string& uriList,         // IN/OUT
                                        utf::string pre,              // IN
                                        utf::string post)             // IN
{
   for (const auto& path : mHGFileContentsList) {
      uriList += pre + path + post;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::LocalGetFileRequest --
 *
 *      Build URI file list with proper pre/post str.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      mHGDndUriList may be changed.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::LocalGetFileRequest(utf::string stagingDirName,
                              const char *desktop)
{
   mHGDndUriList = "";
   if (desktop && strstr(desktop, "GNOME")) {
      /* Setting Gnome URIs for each path in the guest's file list. */
      BuildFileURIList(mHGDndUriList, FCP_GNOME_LIST_PRE, FCP_GNOME_LIST_POST, stagingDirName);
      /* Nautilus on Gnome does not expect FCP_GNOME_LIST_POST after the last uri. See bug 143147. */
      mHGDndUriList.erase(mHGDndUriList.size() - strlen(FCP_GNOME_LIST_POST));
      g_debug("%s: providing file list [%s]\n", __FUNCTION__, mHGDndUriList.c_str());
   } else if (desktop && strstr(desktop, "KDE")) {
      /* Setting KDE URIs for each path in the guest's file list. */
      BuildFileURIList(mHGDndUriList, DND_URI_LIST_PRE_KDE, DND_URI_LIST_POST, stagingDirName);
      g_debug("%s: providing file list [%s]\n", __FUNCTION__, mHGDndUriList.c_str());
   } else {
      g_warning("%s: Unsupported Desktop Manager %s. \n", __FUNCTION__, desktop);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DndUIX11::LocalGetFileContentsRequest --
 *
 *      Build the files content uri list when host dnd with file content.
 *      H->G only.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      mHGDndUriList may be changed.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::LocalGetFileContentsRequest(const char *desktop)
{
   mHGDndUriList = "";
   if (desktop && strstr(desktop, "GNOME")) {
      /* Setting Gnome URIs for each path in the guest's file list. */
      BuildFileContentURIList(mHGDndUriList, FCP_GNOME_LIST_PRE, FCP_GNOME_LIST_POST);
      /* Nautilus on Gnome does not expect FCP_GNOME_LIST_POST after the last uri. See bug 143147. */
      mHGDndUriList.erase(mHGDndUriList.size() - strlen(FCP_GNOME_LIST_POST));
      g_debug("%s: providing file list [%s]\n", __FUNCTION__, mHGDndUriList.c_str());
   } else if (desktop && strstr(desktop, "KDE")) {
      /* Setting KDE URIs for each path in the guest's file list. */
      BuildFileContentURIList(mHGDndUriList, DND_URI_LIST_PRE_KDE, DND_URI_LIST_POST);
      g_debug("%s: providing file list [%s]\n", __FUNCTION__, mHGDndUriList.c_str());
   } else {
      g_warning("%s: Unsupported Desktop Manager %s. \n", __FUNCTION__, desktop);
   }
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
   int mouseX = mOriginX + DRAG_DET_WINDOW_WIDTH / 2;
   int mouseY = mOriginY + DRAG_DET_WINDOW_WIDTH / 2;
   const char *desktop = getenv("XDG_CURRENT_DESKTOP");
   bool hasContent = false;
   mHGDragStarted = false;

   /* Move detect window to the mouse in position */
   OnUpdateDetWnd(true, mOriginX, mOriginY);

   TRACE_CALL();

   CPClipboard_Clear(&mClipboard);
   CPClipboard_Copy(&mClipboard, clip);

   /*
    * Construct the target and action list, as well as a fake motion notify
    * event that's consistent with one that would typically start a drag.
    */
   utf::string stagingDirName;
   DnDFileList fList;
   void *buf;
   size_t sz;

   mHGFCPData.clear();

   if (CPClipboard_GetItem(&mClipboard, CPFORMAT_FILELIST, &buf, &sz)) {
      mHGStagingDir = utf::string(stagingDir.c_str());

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
      // Provide path within vmblock file system instead of actual path.
      mHGFCPData = fList.GetRelPathsStr();

      if (!mBlockAdded) {
         AddBlock();
      } else {
         g_debug("%s: not calling AddBlock\n", __FUNCTION__);
      }
      if (mHGGetFileStatus != DND_FILE_TRANSFER_IN_PROGRESS) {
         mHGGetFileStatus = DND_FILE_TRANSFER_IN_PROGRESS;
      }

      LocalGetFileRequest(stagingDirName, desktop);
      hasContent = true;
   }

   if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_FILECONTENTS)) {
      g_debug("%s: Get file content from host clipboard", __FUNCTION__);
      LocalGetFileContentsRequest(desktop);
      hasContent = true;
   }

   if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_TEXT) ||
       CPClipboard_ItemExists(&mClipboard, CPFORMAT_RTF)) {
      g_debug("%s: Get text/rtfText content from host clipboard", __FUNCTION__);
      hasContent = true;
   }

   /* finally set the guest clipboard */
   if (hasContent) {
      LocalSetContentProvider(desktop);
   }

   SourceDragStartDone();
   /* Initialize host hide feedback to DROP_NONE. */
   mEffect = DROP_NONE;
   SourceUpdateFeedback(mEffect);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnMoveMouse --
 *
 *      simulate mouse move for h->g dnd.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnMoveMouse(int32 x,  // IN: Pointer x-coord
                      int32 y)  // IN: Pointer y-coord
{
   // Position the pointer, and record its position.
   mDetWnd->SimulateXEvents(false, false, false, false, true, x, y);
   mMousePosX = x;
   mMousePosY = y;

   if (mInHGDrag) {
      if (!mHGDragStarted) {
         /*
          * We want to simulate the mouse press and hold to trigger drag_begin
          * event for GtkDragSource, do it here rather than SrcDragBegin as
          * we want to get the mouse position.
          *
          * Before the DnD, we should make sure that the mouse is released
          * otherwise it may be another DnD, not ours. Send a release, then
          * a press here to cover this case.
          */
         mDetWnd->SimulateXEvents(true, true, false, true, true, x, y);
         // simulate drag mouse press
         mDetWnd->SimulateXEvents(false, true, true, false, true, x, y);
         // Tell Gtk that a drag should be started from this widget.
         mDetWnd->StartDrag(mProvider, x, y);
         mHGDragStarted = true;
      }
      if (mEffect != DROP_MOVE) {
         mEffect = DROP_MOVE;
         g_debug("%s: Updating feedback\n", __FUNCTION__);
         SourceUpdateFeedback(mEffect);
      }
   }
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
   OnUpdateDetWnd(true, mOriginX, mOriginY);
   mDetWnd->SimulateXEvents(true, true, false, true, true,
                            mOriginX + DRAG_DET_WINDOW_WIDTH / 2,
                            mOriginY + DRAG_DET_WINDOW_WIDTH / 2);
   OnUpdateDetWnd(false, 0, 0);
   mDetWnd->SimulateXEvents(false, false, false, false, true,
                           mMousePosX, mMousePosY);
   mInHGDrag = false;
   mHGGetFileStatus = DND_FILE_TRANSFER_NOT_STARTED;
   mEffect = DROP_NONE;
   RemoveBlock();
}


/* Source functions for GH DnD. */


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
      mDetWnd->SimulateXEvents(false, true, false, false, false, 0, 0);
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
      Glib::RefPtr<Gdk::Display> display = Gdk::Display::get_default();
      GdkSurface *gdkSurface = gdk_x11_display_get_default_group(display->gobj());
      UpdateWorkArea();

     /*
       * Show the window, move it to the mouse position, and release the
       * mouse button.
       */
      mDetWnd->SimulateXEvents(true, true, false, true, false,
                               mOriginX, mOriginY);
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
   OnUpdateDetWnd(true, mOriginX, mOriginY);

   /*
    * Move the mouse to the saved coordinates, and release the mouse button.
    */
   mDetWnd->SimulateXEvents(false, true, false, false, true,
                            mMousePosX, mMousePosY);
   OnUpdateDetWnd(false, 0, 0);
   /* We've successful or failure for HG drag, set the flag to false anyway */
   mInHGDrag = false;
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
    * If H->G drag is not done yet, only remove block. Otherwise destination
    * may miss the data because we are already reset.
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
           mDetWnd->Get_Current_Drop_ctx(), show, x, y);

   /* If the window is being shown, move it to the right place. */
   if (show) {
      UpdateWorkArea();
      x = MAX(x - DRAG_DET_WINDOW_WIDTH / 2, mOriginX);
      y = MAX(y - DRAG_DET_WINDOW_WIDTH / 2, mOriginY);
      mDetWnd->UpdateDetWnd(show, x, y);
   } else {
      g_debug("%s: hide\n", __FUNCTION__);
      mDetWnd->UpdateDetWnd(show, x, y);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnUpdateUnityDetWnd --
 *
 *      Callback to show/hide fullscreen Unity drag detection window.
 *
 *      TODO: Unity related functions are deprecated, remove it later once
 *            backend RPC functions are removed.
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
   NOT_IMPLEMENTED();
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
   mDetWnd->SimulateXEvents(true, false, true, true, false, 0, 0);
}


/*
 ****************************************************************************
 * BEGIN GTK+ Callbacks (dndcp as drag source: host-to-guest)
 */


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkSrcDragBegin --
 *
 *      GTK4 h->g drag begin handler
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Signals to drag source that drag is started.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnGtkSrcDragBegin(const Glib::RefPtr< Gdk::Drag > &drag, void *ctx)
{
   DnDUIX11 *this_instance = (DnDUIX11 *)ctx;
   g_debug("%s: DND drag begin with ctx %ld\n", __FUNCTION__,
           this_instance->mDetWnd->Get_Current_Drag_ctx());
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkSrcDragEnd --
 *
 *      GTK4 h->g drag end handler
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Signals to drag source that drag is started.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUIX11::OnGtkSrcDragEnd(const Glib::RefPtr< Gdk::Drag > &drag,
                          bool delete_data,
                          void *ctx)
{
   DnDUIX11 *this_instance = (DnDUIX11 *)ctx;
   g_debug("%s: DND drag done with ctx %ld\n", __FUNCTION__,
           this_instance->mDetWnd->Get_Current_Drag_ctx());
   if (this_instance->mHGGetFileStatus != DND_FILE_TRANSFER_IN_PROGRESS) {
      g_debug("%s: reset UI\n", __FUNCTION__);
      this_instance->ResetUI();
   }
   this_instance->mInHGDrag = false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkSrcDragCancel --
 *
 *      GTK+ DragSource "drag_leave" signal handler.
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

bool
DnDUIX11::OnGtkSrcDragCancel(const Glib::RefPtr< Gdk::Drag > &drag,
                             Gdk::DragCancelReason reason, void *ctx)
{
   DnDUIX11 *this_instance = (DnDUIX11 *)ctx;
   g_debug("%s: DND cancel for drag %ld with reason %d\n", __FUNCTION__,
           this_instance->mDetWnd->Get_Current_Drag_ctx(), (int)reason);
   this_instance->ResetUI();
   return true;
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
 * DnDUIX11::OnGtkDragAccept --
 *
 *      GTK4 "signal_accept" signal handler for DropTarget.
 *
 * Results:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDUIX11::OnGtkDragAccept(const Glib::RefPtr< Gdk::Drop > &drop, void *ctx)
{
   DnDUIX11 *this_instance = (DnDUIX11 *)ctx;
   if (!this_instance->mDnD->IsDnDAllowed()) {
      g_warning("%s: No dnd allowed!\n", __FUNCTION__);
      return false;
   }

   if (!this_instance->mDetWnd->Is_Drop_Supported(drop)) {
      g_warning("%s: No Unsupported DND type\n", __FUNCTION__);
      return false;
   }

   if (this_instance->mInHGDrag) {
      g_warning("%s: In H->G dnd process rather than G->H, exit\n", __FUNCTION__);
      return false;
   }

   if (this_instance->mGHDnDInProgress) {
      /*
       * Errors in the GTK layer can lead to dropped information without
       * notification. Therefore, a cleanup process is necessary.
       */
      g_warning("%s: Previous drag failed\n", __FUNCTION__);
      this_instance->ResetUI();
   }

   g_debug("%s: new drag, need to get data for host\n", __FUNCTION__);
   /*
    * This is a new drag operation. We need to start a drag thru the
    * RPC, and to the host. Before we can tell the host, we have to
    * retrieve the drop data.
    */
   this_instance->mGHDnDInProgress = true;
   /* only begin drag enter after we get the data */
   /* Need to grab all of the data. */
   CPClipboard_Clear(&this_instance->mClipboard);
   this_instance->mNumPendingRequest = 0;
   return true;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::OnGtkDragEnter --
 *
 *       GTK4 "signal_enter" signal handler for DropTarget.
 *
 * Results:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Gdk::DragAction
DnDUIX11::OnGtkDragEnter(double x, // IN: drag motion x-coord
                         double y, // IN: drag motion y-coord
                         void *ctx)
{
   DnDUIX11 *this_instance = (DnDUIX11 *)ctx;
   Gdk::DragAction action;

   if (this_instance->mDetWnd->Is_Current_Drop_Supported()) {
      action = Gdk::DragAction::COPY;
   } else {
      g_warning("%s: No dnd allowed @ (%f, %f)!\n", __FUNCTION__, x, y);
   }

   return action;
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
DnDUIX11::OnGtkDragDrop(const Glib::ValueBase& value,
                        double x,
                        double y,
                        void *ctx)
{
   DnDUIX11 *this_instance = (DnDUIX11 *)ctx;
   g_debug("%s: Ctx %ld, drop at x %f y %f\n", __FUNCTION__,
           this_instance->mDetWnd->Get_Current_Drop_ctx(), x, y);

   this_instance->ResetUI();

   if (CPClipboard_IsEmpty(&this_instance->mClipboard)) {
      g_debug("%s: No valid data on mClipboard.\n", __FUNCTION__);
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
 DnDUIX11::OnGtkDragLeave(void *ctx)
 {
   DnDUIX11 *this_instance = (DnDUIX11 *)ctx;

   g_debug("%s: DND leave for drop %ld\n", __FUNCTION__,
           this_instance->mDetWnd->Get_Current_Drop_ctx());

   if (this_instance->mGHDnDInProgress) {
      g_debug("%s: reset UI\n", __FUNCTION__);
      this_instance->ResetUI();
   }
 }


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

Gdk::DragAction
DnDUIX11::OnGtkDragMotion(double x, // IN: drag motion x-coord
                          double y, // IN: drag motion y-coord
                          void *ctx)
{
   DnDUIX11 *this_instance = (DnDUIX11 *)ctx;
   Gdk::DragAction action = Gdk::DragAction::COPY;
   /*
    * If this is a Host to Guest drag, we are done here, so return.
    */
   unsigned long curTime = GetTimeInMillis();
   g_debug("%s: enter dnd ctx %ld\n", __FUNCTION__,
           this_instance->mDetWnd->Get_Current_Drop_ctx());
   if (curTime - this_instance->mDestDropTime <= 1000) {
      g_debug("%s: ignored %ld %ld %ld\n", __FUNCTION__,
              curTime, this_instance->mDestDropTime,
              curTime - this_instance->mDestDropTime);
      goto out;
   }

   g_debug("%s: not ignored %ld %ld %ld\n", __FUNCTION__,
           curTime, this_instance->mDestDropTime,
           curTime - this_instance->mDestDropTime);

   if (this_instance->mInHGDrag ||
       (this_instance->mHGGetFileStatus != DND_FILE_TRANSFER_NOT_STARTED)) {
      g_debug("%s: ignored not in hg drag or not getting hg data\n",
              __FUNCTION__);
      goto out;
   }
   if (!this_instance->mGHDnDInProgress) {
      g_debug("%s: new drag, need to get data for host\n", __FUNCTION__);
      /*
       * This is a new drag operation. We need to start a drag thru the
       * backdoor, and to the host. Before we can tell the host, we have to
       * retrieve the drop data.
       */
      this_instance->mGHDnDInProgress = true;
      /* only begin drag enter after we get the data */
      /* Need to grab all of the data. */
      CPClipboard_Clear(&this_instance->mClipboard);
      this_instance->mNumPendingRequest = 0;
   } else {
      g_debug("%s: Multiple drag motions before gh data has been received.\n",
              __FUNCTION__);
   }
out:
   return action;
}


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
DnDUIX11::OnGtkDragDataReceived(void *ctx)
{
   DnDUIX11 *this_instance = (DnDUIX11 *)ctx;
   std::string value;
   std::string value_type;

   g_debug("%s: enter dnd ctx %ld\n", __FUNCTION__,
           this_instance->mDetWnd->Get_Current_Drop_ctx());
   /* The GH DnD may already finish before we got response. */
   if (!this_instance->mGHDnDInProgress) {
      g_debug("%s: not valid\n", __FUNCTION__);
      return;
   }

   if (this_instance->mDetWnd->Is_DropValue_Ready()) {
      if (this_instance->mDetWnd->Get_Drop_Value(value, value_type)) {
         if (this_instance->SetCPClipboardFromGtk(value, value_type)) {
            this_instance->mNumPendingRequest--;
            if (this_instance->mNumPendingRequest > 0) {
               return;
            }
            if (CPClipboard_IsEmpty(&this_instance->mClipboard)) {
               g_debug("%s: Failed getting item.\n", __FUNCTION__);
               this_instance->ResetUI();
               return;
            }
            if (!this_instance->mGHDnDDataReceived) {
               g_debug("%s: Drag entering.\n", __FUNCTION__);
               this_instance->mGHDnDDataReceived = true;
               this_instance->TargetDragEnter();
            }
         } else {
            g_debug("%s: Failed to set CP clipboard.\n", __FUNCTION__);
            this_instance->ResetUI();
         }
      } else {
         g_debug("%s: DND data is not ready, Could be a drag leave.\n",
                 __FUNCTION__);
      }
   }
}


/*
 * END GTK+ Callbacks (dndcp as drag destination: guest-to-host)
 ****************************************************************************
 */


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::SetCPClipboardFromGtk_File --
 *
 *      Extract file list from GTK4.
 *      And construct cross-platform clipboard.
 *
 * Results:
 *      Returns true if conversion succeeded, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool DnDUIX11::SetCPClipboardFromGtk_File(utf::string &source)
{
   DnDFileList fileList;
   DynBuf buf;
   size_t index = 0;
   char *newPath;
   size_t newPathLen;
   uint64 totalSize;
   int64 size;

   g_debug("%s: Got file list: [%s]\n", __FUNCTION__, source.c_str());

   if (source.length() == 0) {
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
      char *newRelPath;

      /*TODO: refactor compiler option segments and merge them together*/
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


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::SetCPClipboardFromGtk_PlainText --
 *
 *      Extract text from GTK4. And construct cross-platform clipboard.
 *
 * Results:
 *      Returns true if conversion succeeded, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool DnDUIX11::SetCPClipboardFromGtk_PlainText(utf::string &source)
{
   if (source.size() > 0 &&
       source.size() < DNDMSG_MAX_ARGSZ &&
       CPClipboard_SetItem(&mClipboard, CPFORMAT_TEXT, source.c_str(), source.size() + 1)) {
      g_debug("%s: Got text, size %" FMTSZ "u\n", __FUNCTION__, source.size());
   } else {
      g_warning("%s: Failed to get text\n", __FUNCTION__);
      return false;
   }
   return true;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::SetCPClipboardFromGtk_RTF --
 *
 *      Extract RTF from GTK4. And construct cross-platform clipboard.
 *
 * Results:
 *      Returns true if conversion succeeded, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool DnDUIX11::SetCPClipboardFromGtk_RTF(utf::string &source)
{
   if (source.size() > 0 &&
       source.size() < DNDMSG_MAX_ARGSZ &&
       CPClipboard_SetItem(&mClipboard, CPFORMAT_RTF, source.c_str(), source.size() + 1)) {
      g_debug("%s: Got RTF, size %" FMTSZ "u\n", __FUNCTION__, source.size());
      return true;
   } else {
      g_warning("%s: Failed to get RTF\n", __FUNCTION__);
   }

   return false;
}


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
DnDUIX11::SetCPClipboardFromGtk(std::string &data, std::string &drop_type) // IN
{
   DnDFileList fileList;
   DynBuf buf;
   uint64 totalSize = 0;
   int64 size;
   bool ret = false;

   utf::string source = utf::string(data);
   utf::string type = utf::string(drop_type);

   /* Try to get file list. */
   if (mDnD->CheckCapability(DND_CP_CAP_FILE_DND) && type == DRAG_TARGET_NAME_URI_LIST) {
      ret = SetCPClipboardFromGtk_File(source);
   }
   /* Try to get plain text. */
   if (mDnD->CheckCapability(DND_CP_CAP_PLAIN_TEXT_DND) && TargetIsPlainText(type)) {
      ret = SetCPClipboardFromGtk_PlainText(source);
   }
   /* Try to get RTF string. */
   if (mDnD->CheckCapability(DND_CP_CAP_RTF_DND) && TargetIsRichText(type)) {
      ret = SetCPClipboardFromGtk_RTF(source);
   }

   return ret;
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
 *         E.g. /tmp/VMwareDnD/abcd137 → abcd137
 *
 * Results:
 *      Returns session directory name on success, empty string otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

utf::string
DnDUIX11::GetLastDirName(const utf::string &str)
{
   char *baseName;
   utf::string stripSlash;
   char *path = File_StripSlashes(str.c_str());
   if (path) {
      stripSlash = utf::CopyAndFree(path);
   }

   File_GetPathName(stripSlash.c_str(), NULL, &baseName);
   if (baseName) {
      return utf::CopyAndFree(baseName);
   } else {
      return utf::string();
   }
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
   mDetWnd->SimulateXEvents(true, false, true, true, false, 0, 0);
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
DnDUIX11::SourceUpdateFeedback(DND_DROPEFFECT effect) // IN: feedback to send to the UI-independent DnD layer.
{
   TRACE_CALL();
   mDnD->SrcUIUpdateFeedback(effect);
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

unsigned long
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


/*
 *-----------------------------------------------------------------------------
 *
 * DnDUIX11::UpdateWorkArea --
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
DnDUIX11::UpdateWorkArea()
{
   TRACE_CALL();

   std::vector<unsigned long> values;
   if (xutils::GetCardinalList(mGdkSurface, "_NET_WORKAREA", values)
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
      xutils::GetCardinal(mGdkSurface, "_NET_CURRENT_DESKTOP", desktop);

      mOriginX = values[0 + 4 * desktop];
      mOriginY = values[1 + 4 * desktop];
   } else {
      mOriginX = 0;
      mOriginY = 0;
   }

   g_debug("%s: new origin at (%d, %d)\n", __FUNCTION__, mOriginX, mOriginY);
}
