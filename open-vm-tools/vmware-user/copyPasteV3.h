/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * copyPaste.h --
 *
 *     Handles the linux specific copy paste operations.
 *
 *     TODO: Data conversion out into another class to be shared between dnd
 *     and copypaste.
 */

#ifndef LUI_COPYPASTE_HH
#define LUI_COPYPASTE_HH

#include "cui/dnd/copyPaste.hh"
#include "cui/vm.hh"

extern "C" {
#include "dnd.h"
#include "../dnd/dndClipboard.h"
}

namespace lui {
   class CopyPaste
      : public sigc::trackable
   {
   public:
      CopyPaste(cui::dnd::CopyPaste *cp);
      ~CopyPaste(void);

      void UpdateLocalClipboard(void);
      void UpdateRemoteClipboard(void);

   private:
      /* gh */
      void GetRemoteClipboardCB(const CPClipboard *clip);
      void RemoteGetFilesDone(void);
      /* clipboard stuff */
      void LocalGetFileRequestCB(Gtk::SelectionData& selection_data, guint info);
      void LocalClearClipboardCB(void);

      /* hg */
      /* Clipboard stuff */
      void LocalClipboardTimestampCB(const Gtk::SelectionData& sd);
      void LocalPrimTimestampCB(const Gtk::SelectionData& sd);
      void LocalReceivedTargetsCB(const Glib::StringArrayHandle& targets_array);
      void LocalReceivedFileListCB(const Gtk::SelectionData& selection_data);
      void LocalReceivedTextCB(const Glib::ustring& text);

      /* Convertion methods. */
      void LocalGetSelectionFileList(const Gtk::SelectionData& sd);
      utf::utf8string GetNextPath(utf::utf8string &str, size_t& index);
      utf::string GetLastDirName(const utf::string &str);

      cui::dnd::CopyPaste * const mCP;

      CPClipboard mClipboard;
      bool mIsClipboardOwner;
      uint64 mClipTime;
      uint64 mPrimTime;
      GdkAtom mHGSelection;

      /* File vars. */
      bool mGHGetFilesInitated;
      VmTimeType mGHGetListTime;
      utf::string mGHCopiedUriList;
      utf::utf8string mGHFCPData;

      std::list<Gtk::TargetEntry> mListTargets;
   };

} //namespace lui

#endif // LUI_COPYPASTE_HH
