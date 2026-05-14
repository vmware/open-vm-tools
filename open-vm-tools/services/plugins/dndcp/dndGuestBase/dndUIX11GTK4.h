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
 * @file dndUIX11GTK4.h
 *
 *    Implement the methods that allow DnD between host and guest for
 *    protocols V3 or greater on GTK4 lib.
 *
 */

#ifndef __DND_UI_X11_GTK4_H__
#define __DND_UI_X11_GTK4_H__

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
#include "vmware/tools/guestrpc.h"
#include "vmware/tools/plugin.h"
}

#include "guestDnD.hh"
#include "dndFileList.hh"
#include "dragDetWndX11GTK4.h"

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

private:
   /*
    * Blocking FS Helper Functions.
    */
   void AddBlock();
   void RemoveBlock();
   void
   BuildFileURIList(utf::string& uriList,
                    utf::string pre,
                    utf::string post,
                    utf::string stagingDirName);
   void
   BuildFileContentURIList(utf::string& uriList,
                           utf::string pre,
                           utf::string post);
   void LocalGetFileRequest(utf::string stagingDirName, const char *desktop);
   void LocalGetFileContentsRequest(const char *desktop);
   void LocalSetContentProvider(const char* desktop);

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
   void OnDestMoveDetWndToMousePos();
   void SourceDragStartDone();
   void SourceUpdateFeedback(DND_DROPEFFECT effect);

   /*
    * Gtk+ Callbacks: Drag Source
    */
   static void OnGtkSrcDragBegin(const Glib::RefPtr< Gdk::Drag > &drag, void *ctx);
   static void OnGtkSrcDragEnd(const Glib::RefPtr< Gdk::Drag > &drag, bool delete_data, void *ctx);
   static bool OnGtkSrcDragCancel(const Glib::RefPtr< Gdk::Drag > &drag, Gdk::DragCancelReason reason, void *ctx);

   /*
    * Gtk+ Callbacks: Drag Destination.
    */
   static bool OnGtkDragAccept(const Glib::RefPtr< Gdk::Drop > &drop, void *ctx);
   static Gdk::DragAction OnGtkDragEnter(double x, double y, void *ctx);
   static Gdk::DragAction OnGtkDragMotion(double x, double y, void *ctx);
   static bool OnGtkDragDrop(const Glib::ValueBase& value, double x, double y, void *ctx);
   static void OnGtkDragLeave(void *ctx);
   static void OnGtkDragDataReceived(void *ctx);
   /*
    * Target function for GH DnD. Makes call to common layer.
    */
   void TargetDragEnter();

   /*
    * Misc methods.
    */
   void InitGtk();

   bool SetCPClipboardFromGtk_File(utf::string &source);
   bool SetCPClipboardFromGtk_PlainText(utf::string &source);
   bool SetCPClipboardFromGtk_RTF(utf::string &source);
   bool SetCPClipboardFromGtk(std::string &data, std::string &drop_type);

   utf::string GetLastDirName(const utf::string &str);
   utf::utf8string GetNextPath(utf::utf8string &str, size_t& index);

   static unsigned long GetTimeInMillis();

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

   void UpdateWorkArea();

   ToolsAppCtx *mCtx;
   GuestDnDMgr *mDnD;
   utf::string mHGStagingDir;
   std::vector<utf::string> mHGFileContentsList;
   DragDetWnd *mDetWnd;
   CPClipboard mClipboard;
   DnDBlockControl *mBlockCtrl;
   DND_FILE_TRANSFER_STATUS mHGGetFileStatus;
   bool mBlockAdded;

   /* State to determine if drag motion is a drag enter. */
   bool mGHDnDInProgress;
   /* Icon updates from the guest. */
   /* Only update mouse when we have clipboard contents from the host. */
   bool mGHDnDDataReceived;
   bool mInHGDrag;
   // Flag to indicate if H->G drag has been triggered in guest by simulate mouse press
   bool mHGDragStarted;
   DND_DROPEFFECT mEffect;
   int32 mMousePosX;
   int32 mMousePosY;
   int mNumPendingRequest;
   unsigned long mDestDropTime;
   uint64 mTotalFileSize;

   /* GdkSurface used in dnd */
   Glib::RefPtr< Gdk::Surface > mGdkSurface;
   Glib::RefPtr<Gdk::ContentProvider> mProvider;
   utf::string mHGDndUriList;
   utf::utf8string mHGFCPData;

   /*
    * Upper left corner of our work area, a safe place for us to place
    * our detection window without clashing with a windows parented to the
    * composite overlay window.
    */
   int mOriginX;
   int mOriginY;
};

#endif // __DND_UI_X11_H__
