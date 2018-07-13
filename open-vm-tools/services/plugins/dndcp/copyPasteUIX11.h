/*********************************************************
 * Copyright (C) 2009-2018 VMware, Inc. All rights reserved.
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
 * @file copyPasteUIX11.h
 *
 * This class implements the methods that allows Copy/Paste
 * between host and guest using version 3+ of the protocol.
 *
 */

#ifndef __COPYPASTE_UI_X11_H__
#define __COPYPASTE_UI_X11_H__

#include "stringxx/string.hh"
#include "dnd.h"
#include "str.h"
#include "dynbuf.h"

extern "C" {
#include "debug.h"
#include "dndClipboard.h"
#include "../dnd/dndFileContentsUtil.h"
#include "cpNameUtil.h"
#include "vmware/tools/guestrpc.h"
}

#include "dynxdr.h"
#include "posix.h"
#include "unicodeOperations.h"
#include "guestCopyPaste.hh"

/*
 * Make sure exception types are public and therefore shared between libg*mm
 * and this plugin. 
 *
 * See
 * http://gcc.gnu.org/wiki/Visibility#Problems_with_C.2B-.2B-_exceptions_.28please_read.21.29
 */
#pragma GCC visibility push(default)
#include <gtkmm.h>
#pragma GCC visibility pop

#include <list>
#include <vector>

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "vmware/guestrpc/tclodefs.h"

class CopyPasteUIX11;

struct ThreadParams
{
   pthread_mutex_t fileBlockMutex;
   pthread_cond_t fileBlockCond;
   bool fileBlockCondExit;
   CopyPasteUIX11 *cp;
   utf::string fileBlockName;
};

class CopyPasteUIX11 : public sigc::trackable
{
public:
   CopyPasteUIX11();
   virtual ~CopyPasteUIX11();
   bool Init();
   void VmxCopyPasteVersionChanged(RpcChannel *chan,
                                   uint32 version);
   void SetCopyPasteAllowed(bool isCopyPasteAllowed)
   { mCP->SetCopyPasteAllowed(isCopyPasteAllowed); }
   void Reset(void);
   void SetBlockControl(DnDBlockControl *blockCtrl)
      { Debug("Setting mBlockCtrl to %p\n", blockCtrl);
        mBlockCtrl = blockCtrl; }
   bool IsBlockAdded() const
   { return mBlockAdded; }
   void RequestFiles()
   { mCP->SrcUIRequestFiles(); }

private:

   /* hg */
   void GetRemoteClipboardCB(const CPClipboard *clip);
   void RemoteGetFilesDone(void);
   void LocalGetFileRequestCB(Gtk::SelectionData& selection_data, guint info);
   void LocalGetTextOrRTFRequestCB(Gtk::SelectionData& sd, guint info);
   void LocalGetSelectionFileList(const Gtk::SelectionData& sd);
   void LocalGetFileContentsRequestCB(Gtk::SelectionData& sd, guint info);
   void LocalClearClipboardCB(void);

   /* gh */
   void GetLocalClipboard(void);
   void LocalClipboardTimestampCB(const Gtk::SelectionData& sd);
   void LocalPrimTimestampCB(const Gtk::SelectionData& sd);
   void LocalReceivedFileListCB(const Gtk::SelectionData& selection_data);
   void GetLocalFilesDone(bool success);
   void SendClipNotChanged(void);

   /* Conversion methods. */
   utf::utf8string GetNextPath(utf::utf8string &str, size_t& index);
   utf::string GetLastDirName(const utf::string &str);
   bool LocalPrepareFileContents(const CPClipboard *clip);

   VmTimeType GetCurrentTime(void);

   static void* FileBlockMonitorThread(void *arg);
   void TerminateThread();

   // Member variables
   GuestCopyPasteMgr *mCP;
   bool mClipboardEmpty;
   utf::string mHGStagingDir;
   std::vector<Gtk::TargetEntry> mListTargets;
   bool mIsClipboardOwner;
   uint64 mClipTime;
   uint64 mPrimTime;
   uint64 mLastTimestamp;
   GdkAtom mGHSelection;
   CPClipboard mClipboard;
   ThreadParams mThreadParams;
   pthread_t mThread;

   /* File vars. */
   VmTimeType mHGGetListTime;
   utf::string mHGCopiedUriList;
   utf::utf8string mHGFCPData;
   utf::string mHGTextData;
   std::string mHGRTFData;
   std::vector<utf::string> mHGFileContentsList;
   DND_FILE_TRANSFER_STATUS mHGGetFileStatus;
   bool mBlockAdded;
   DnDBlockControl *mBlockCtrl;
   bool mInited;
   uint64 mTotalFileSize;
   bool mGetTimestampOnly;
};

#endif // __COPYPASTE_UI_X11_H__
