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
}

#include "unicodeOperations.h"

#include "copyPaste.hh"

#include <gtkmm.h>
#include <list>

class CopyPasteUI : public sigc::trackable
{
public:
   CopyPasteUI();
   virtual ~CopyPasteUI();
   void VmxCopyPasteVersionChanged(struct RpcIn *rpcIn,
                                   uint32 version);
   void SetCopyPasteAllowed(bool isCopyPasteAllowed)
      { mCP.SetCopyPasteAllowed(isCopyPasteAllowed); }
   void Reset(void);
   bool GetLocalFiles(void);

private:

   /* hg */
   void GetRemoteClipboardCB(const CPClipboard *clip);
   void RemoteGetFilesDone(void);
   void LocalGetFileRequestCB(Gtk::SelectionData& selection_data, guint info);
   void LocalClearClipboardCB(void);

   /* gh */
   bool GetLocalClipboard(CPClipboard *clip);
   void LocalClipboardTimestampCB(const Gtk::SelectionData& sd);
   void LocalPrimTimestampCB(const Gtk::SelectionData& sd);
   void LocalReceivedTargetsCB(const Glib::StringArrayHandle& targets_array);
   void LocalReceivedFileListCB(const Gtk::SelectionData& selection_data);
   void LocalReceivedTextCB(const Glib::ustring& text);
   void GetLocalFilesDone(bool success);

   /* Conversion methods. */
   void LocalGetSelectionFileList(const Gtk::SelectionData& sd);
   utf::utf8string GetNextPath(utf::utf8string &str, size_t& index);
   utf::string GetLastDirName(const utf::string &str);

   VmTimeType GetCurrentTime(void);

   // Member variables
   CopyPaste mCP;
   bool mClipboardEmpty;
   std::string mHGStagingDir;
   std::list<Gtk::TargetEntry> mListTargets;
   bool mIsClipboardOwner;
   uint64 mClipTime;
   uint64 mPrimTime;
   uint64 mClipTimePrev;
   uint64 mPrimTimePrev;
   GdkAtom mGHSelection;
   CPClipboard mClipboard;

   /* File vars. */
   bool mHGGetFilesInitated;
   VmTimeType mHGGetListTime;
   utf::string mHGCopiedUriList;
   utf::utf8string mHGFCPData;
};

#endif // COPYPASTE_UI_H
