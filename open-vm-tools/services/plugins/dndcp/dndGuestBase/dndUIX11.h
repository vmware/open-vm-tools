/*********************************************************
 * Copyright (C) 2009-2017 VMware, Inc. All rights reserved.
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
 * @file dndUIX11.h
 *
 *    Implement the methods that allow DnD between host and guest for
 *    protocols V3 or greater.
 *
 */

#ifndef __DND_UI_X11_H__
#define __DND_UI_X11_H__

#include "stringxx/string.hh"
#include "dnd.h"
#include "str.h"
#include "util.h"
#include "vmblock.h"
#include "dynbuf.h"
#include "dynxdr.h"
#include "posix.h"

extern "C" {
#include "debug.h"
#include "dndClipboard.h"
#include "../dnd/dndFileContentsUtil.h"
#include "cpNameUtil.h"
#include "vmware/tools/guestrpc.h"
#include "vmware/tools/plugin.h"
}

#include "guestDnD.hh"
#include "dndFileList.hh"
#include "dragDetWndX11.h"

struct DblLnkLst_Links;

/**
 * The DnDUI class implements the UI portion of DnD V3 and greater
 * versions of the protocol.
 */
class DnDUIX11
   : public sigc::trackable
{
public:
   DnDUIX11(ToolsAppCtx *ctx);
   ~DnDUIX11();
   bool Init();
   void VmxDnDVersionChanged(RpcChannel *chan, uint32 version);
   void SetDnDAllowed(bool isDnDAllowed)
      {ASSERT(mDnD); mDnD->SetDnDAllowed(isDnDAllowed);}
   void SetBlockControl(DnDBlockControl *blockCtrl);
   void SetUnityMode(Bool mode)
      {mUnityMode = mode;};

   DragDetWnd *GetFullDetWnd() {return mDetWnd;}
   GtkWidget *GetDetWndAsWidget();

private:
   /*
    * Blocking FS Helper Functions.
    */
   void AddBlock();
   void RemoveBlock();

   bool TryXTestFakeDeviceButtonEvent();

   /*
    * Callbacks from Common DnD layer.
    */
   void ResetUI();
   void OnMoveMouse(int32 x, int32 y);

   /*
    * Source functions for HG DnD.
    */
   void OnSrcDragBegin(const CPClipboard *clip, std::string stagingDir);
   void OnSrcDrop();

   /*
    * Called when HG Dnd is completed.
    */
   void OnSrcCancel();

   /*
    * Called when GH DnD is completed.
    */
   void OnPrivateDrop(int32 x, int32 y);
   void OnDestCancel();

   /*
    * Source functions for file transfer.
    */
   void OnGetFilesDone(bool success);

   /*
    * Callbacks for showing/hiding detection window.
    */
   void OnUpdateDetWnd(bool bShow, int32 x, int32 y);
   void OnUpdateUnityDetWnd(bool bShow, uint32 unityWndId, bool bottom);
   void OnDestMoveDetWndToMousePos();

   /*
    * Gtk+ Callbacks: Drag Destination.
    */
   void OnGtkDragDataReceived(const Glib::RefPtr<Gdk::DragContext> &dc,
                              int x, int y, const Gtk::SelectionData &sd,
                              guint info, guint time);
   bool OnGtkDragDrop(const Glib::RefPtr<Gdk::DragContext> &dc, int x, int y,
                      guint time);
   void OnGtkDragLeave(const Glib::RefPtr<Gdk::DragContext> &dc, guint time);
   bool OnGtkDragMotion(const Glib::RefPtr<Gdk::DragContext> &dc, int x,
                        int y, guint time);

   /*
    * Gtk+ Callbacks: Drag Source.
    */
   void OnGtkDragBegin(const Glib::RefPtr<Gdk::DragContext>& context);
   void OnGtkDragDataGet(const Glib::RefPtr<Gdk::DragContext>& context,
                         Gtk::SelectionData& selection_data, guint info,
                         guint time);
   void OnGtkDragEnd(const Glib::RefPtr<Gdk::DragContext>& context);
   /*
    * Source functions for HG DnD. Makes calls to common layer.
    */
   void SourceDragStartDone();
   void SourceUpdateFeedback(DND_DROPEFFECT effect);

   /*
    * Target function for GH DnD. Makes call to common layer.
    */
   void TargetDragEnter();

   /*
    * Other signal handlers for tracing.
    */

   bool GtkEnterEventCB(GdkEventCrossing *event);
   bool GtkLeaveEventCB(GdkEventCrossing *event);
   bool GtkMapEventCB(GdkEventAny *event);
   bool GtkUnmapEventCB(GdkEventAny *event);
   void GtkRealizeEventCB();
   void GtkUnrealizeEventCB();
   bool GtkMotionNotifyEventCB(GdkEventMotion *event);
   bool GtkConfigureEventCB(GdkEventConfigure *event);
   bool GtkButtonPressEventCB(GdkEventButton *event);
   bool GtkButtonReleaseEventCB(GdkEventButton *event);

   /*
    * Misc methods.
    */
   void InitGtk();

   bool SetCPClipboardFromGtk(const Gtk::SelectionData& sd);
   bool RequestData(const Glib::RefPtr<Gdk::DragContext> &dc,
                    guint timeValue);
   std::string GetLastDirName(const std::string &str);
   utf::utf8string GetNextPath(utf::utf8string &str, size_t& index);

   static DND_DROPEFFECT ToDropEffect(const Gdk::DragAction action);
   static unsigned long GetTimeInMillis();

   bool SendFakeXEvents(const bool showWidget, const bool buttonEvent,
                        const bool buttonPress, const bool moveWindow,
                        const bool coordsProvided,
                        const int xCoord, const int yCoord);
   bool SendFakeMouseMove(const int x, const int y);
   bool WriteFileContentsToStagingDir();

   static inline bool TargetIsPlainText(const utf::string& target) {
      return    target == TARGET_NAME_STRING
             || target == TARGET_NAME_TEXT_PLAIN
             || target == TARGET_NAME_UTF8_STRING
             || target == TARGET_NAME_COMPOUND_TEXT;
   }

   static inline bool TargetIsRichText(const utf::string& target) {
      return    target == TARGET_NAME_APPLICATION_RTF
             || target == TARGET_NAME_TEXT_RICHTEXT
             || target == TARGET_NAME_TEXT_RTF;
   }

   void OnWorkAreaChanged(Glib::RefPtr<Gdk::Screen> screen);

   ToolsAppCtx *mCtx;
   GuestDnDMgr *mDnD;
   std::string mHGStagingDir;
   utf::string mHGFileContentsUriList;
   DragDetWnd *mDetWnd;
   CPClipboard mClipboard;
   DnDBlockControl *mBlockCtrl;
   DND_FILE_TRANSFER_STATUS mHGGetFileStatus;
   int mHGEffect;
   bool mBlockAdded;

   /* State to determine if drag motion is a drag enter. */
   bool mGHDnDInProgress;
   /* Icon updates from the guest. */
   /* Only update mouse when we have clipboard contents from the host. */
   bool mGHDnDDataReceived;
   bool mGHDnDDropOccurred;
   bool mUnityMode;
   bool mInHGDrag;
   DND_DROPEFFECT mEffect;
   int32 mMousePosX;
   int32 mMousePosY;
   GdkDragContext *mDragCtx;
   int mNumPendingRequest;
   unsigned long mDestDropTime;
   uint64 mTotalFileSize;

   /*
    * Upper left corner of our work area, a safe place for us to place
    * our detection window without clashing with a windows parented to the
    * composite overlay window.
    */
   Gdk::Point mOrigin;

   bool mUseUInput;
   int mScreenWidth;
   int mScreenHeight;
};

#endif // __DND_UI_X11_H__
