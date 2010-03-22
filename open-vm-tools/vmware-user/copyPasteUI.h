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
 * @file copyPasteUI.h
 *
 * This class implements the methods that allows Copy/Paste
 * between host and guest using version 3+ of the protocol.
 *
 */

#ifndef COPYPASTE_UI_H
#define COPYPASTE_UI_H

#include "stringxx/string.hh"

extern "C" {
#include "dnd.h"
#include "debug.h"
#include "str.h"
#include "dndClipboard.h"
#include "dynbuf.h"
#include "../dnd/dndFileContentsUtil.h"
#include "dynxdr.h"
#include "cpNameUtil.h"
#include "posix.h"
}

#include "unicodeOperations.h"

#include "copyPaste.hh"

#include <gtkmm.h>
#include <list>
#include <vector>

class CopyPasteUI : public sigc::trackable
{
public:
   CopyPasteUI();
   virtual ~CopyPasteUI();
   bool Init();
   void VmxCopyPasteVersionChanged(struct RpcIn *rpcIn,
                                   uint32 version);
   void SetCopyPasteAllowed(bool isCopyPasteAllowed)
      { mCP.SetCopyPasteAllowed(isCopyPasteAllowed); }
   void Reset(void);
   void Cancel(void);
   void SetBlockControl(DnDBlockControl *blockCtrl)
      { Debug("Setting mBlockCtrl to %p\n", blockCtrl);
        mBlockCtrl = blockCtrl; }

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
   bool GetLocalClipboard(CPClipboard *clip);
   void LocalClipboardTimestampCB(const Gtk::SelectionData& sd);
   void LocalPrimTimestampCB(const Gtk::SelectionData& sd);
   void LocalReceivedFileListCB(const Gtk::SelectionData& selection_data);
   void GetLocalFilesDone(bool success);

   /* Conversion methods. */
   utf::utf8string GetNextPath(utf::utf8string &str, size_t& index);
   utf::string GetLastDirName(const utf::string &str);
   bool LocalPrepareFileContents(const CPClipboard *clip);

   VmTimeType GetCurrentTime(void);

   // Member variables
   CopyPaste mCP;
   bool mClipboardEmpty;
   utf::string mHGStagingDir;
   std::list<Gtk::TargetEntry> mListTargets;
   bool mIsClipboardOwner;
   uint64 mClipTime;
   uint64 mPrimTime;
   GdkAtom mGHSelection;
   CPClipboard mClipboard;

   /* File vars. */
   bool mHGGetFilesInitiated;
   VmTimeType mHGGetListTime;
   utf::string mHGCopiedUriList;
   utf::utf8string mHGFCPData;
   utf::string mHGTextData;
   utf::string mHGRTFData;
   std::vector<utf::string> mHGFileContentsList;
   bool mFileTransferDone;
   bool mBlockAdded;
   DnDBlockControl *mBlockCtrl;
   bool mInited;
};

#endif // COPYPASTE_UI_H
