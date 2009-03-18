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

/*
 * copyPasteUI.cpp --
 *
 *    This class implements the methods that allows CopyPaste between host
 *    and guest.
 *
 *    For a perspective on X copy/paste, see
 *    http://www.jwz.org/doc/x-cut-and-paste.html
 */

#include <sys/time.h>
#include <time.h>
#include "copyPasteUI.h"
#include "dndFileList.hh"

extern "C" {
   #include "vmwareuserInt.h"
   #include "vmblock.h"
   #include "vm_app.h"
   #include "file.h"
   #include "dnd.h"
   #include "dndMsg.h"
   #include "dndClipboard.h"
   #include "cpName.h"
   #include "debug.h"
   #include "cpNameUtil.h"
   #include "rpcout.h"
   #include "eventManager.h"
}

/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::CopyPasteUI --
 *
 *    Constructor.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

CopyPasteUI::CopyPasteUI()
 : mClipboardEmpty(true),
   mHGStagingDir(""),
   mIsClipboardOwner(false),
   mClipTimePrev(0),
   mPrimTimePrev(0),
   mHGGetFilesInitiated(false),
   mFileTransferDone(false),
   mBlockAdded(false),
   mBlockFd(-1),
   mInited(false)
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::Init --
 *
 *    Initialize copy paste UI class and register for V3 or greater copy
 *    paste.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUI::Init()
{

   if (mInited) {
      return;
   }

   mInited = true;
   CPClipboard_Init(&mClipboard);

   Gtk::TargetEntry gnome(FCP_TARGET_NAME_GNOME_COPIED_FILES);
   Gtk::TargetEntry kde(FCP_TARGET_NAME_URI_LIST);
   gnome.set_info(FCP_TARGET_INFO_GNOME_COPIED_FILES);
   kde.set_info(FCP_TARGET_INFO_URI_LIST);

   mListTargets.push_back(gnome);
   mListTargets.push_back(kde);

   /* Tell the VMX about the copyPaste version we support. */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.copypaste_version 3")) {
      Debug("%s: could not set guest copypaste version capability\n",
            __FUNCTION__);
      return;
   }
   Debug("%s: set copypaste version 3\n", __FUNCTION__);

   mCP.newClipboard.connect(
      sigc::mem_fun(this, &CopyPasteUI::GetRemoteClipboardCB));
   mCP.localGetClipboard.connect(
      sigc::mem_fun(this, &CopyPasteUI::GetLocalClipboard));
   mCP.localGetFilesDoneChanged.connect(
      sigc::mem_fun(this, &CopyPasteUI::GetLocalFilesDone));
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::~CopyPaste --
 *
 *    Destructor.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

CopyPasteUI::~CopyPasteUI()
{
   CPClipboard_Destroy(&mClipboard);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::VmxCopyPasteVersionChanged --
 *
 *      Update version information in mCP.
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
CopyPasteUI::VmxCopyPasteVersionChanged(struct RpcIn *rpcIn, // IN
                                        uint32 version)      // IN
{
   Debug("%s: new version is %d\n", __FUNCTION__, version);
   mCP.VmxCopyPasteVersionChanged(rpcIn, version);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::GetLocalClipboard --
 *
 *    Retrives the data from local clipboard and sends it to host. Send empty
 *    data back if there is no data or can not get data successfully. For 
 *    guest->host copy/paste.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

bool
CopyPasteUI::GetLocalClipboard(CPClipboard *clip) // OUT
{
   Debug("%s: enter.\n", __FUNCTION__);

   if (mIsClipboardOwner) {
      Debug("%s: is clipboard owner, set changed to false and return.\n", __FUNCTION__);
      CPClipboard_SetChanged(clip, FALSE);
      return true;
   }

   if (!mCP.IsCopyPasteAllowed()) {
      Debug("%s: copyPaste is not allowed\n", __FUNCTION__);
      return true;
   }

   Glib::RefPtr<Gtk::Clipboard> refClipboard =
      Gtk::Clipboard::get(GDK_SELECTION_CLIPBOARD);

   mClipTime = 0;
   mPrimTime = 0;
   mGHSelection = GDK_SELECTION_CLIPBOARD;
   Debug("%s: retrieving timestamps\n", __FUNCTION__);
   refClipboard->request_contents(TARGET_NAME_TIMESTAMP,
      sigc::mem_fun(this, &CopyPasteUI::LocalClipboardTimestampCB));
   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::GetCurrentTime --
 *
 *    Get current time in microseconds. 
 *
 * Results:
 *    Time in microseconds.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

VmTimeType
CopyPasteUI::GetCurrentTime(void)
{
   struct timeval tv;
   VmTimeType curTime;

   if (gettimeofday(&tv, NULL) != 0) {
      Debug("%s: gettimeofday failed!\n", __FUNCTION__);
      return (VmTimeType) 0;
   }
   curTime = (tv.tv_sec * 1000000 + tv.tv_usec);
   return curTime;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::LocalGetFileRequestCB --
 *
 *      Callback from a file paste request from another guest application.
 *      Begins copying the files from host to guest and return the file list.
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
CopyPasteUI::LocalGetFileRequestCB(Gtk::SelectionData& sd,        // IN:
                                   guint info)                    // IN:
{
   Debug("%s: enter.\n", __FUNCTION__);
   mHGCopiedUriList = "";
   VmTimeType curTime;
   mBlockAdded = false;

   curTime = GetCurrentTime();

   /*
    * Some applications may ask for clipboard contents right after clipboard
    * owner changed. So HG FCP will return nothing for some time after switch
    * from guest OS to host OS.
    */
   if ((curTime - mHGGetListTime) < FCP_COPY_DELAY) {
      Debug("%s: time delta less than FCP_COPY_DELAY, returning.\n",
            __FUNCTION__);
      return;
   }

   if (!mIsClipboardOwner || !mCP.IsCopyPasteAllowed()) {
      Debug("%s: not clipboard ownder, or copy paste not allowed, returning.\n",
            __FUNCTION__);
      return;
   }

   Debug("%s: Got paste request, target is %s\n", __FUNCTION__,
         sd.get_target().c_str());

   /* Copy the files. */
   if (!mHGGetFilesInitiated) {
      utf::string str;
      utf::string hgStagingDir;
      utf::string stagingDirName;
      utf::string pre;
      utf::string post;
      size_t index = 0;
      mFileTransferDone = false;

      hgStagingDir = static_cast<utf::string>(mCP.GetFiles());
      Debug("%s: Getting files. Staging dir: %s", __FUNCTION__,
            hgStagingDir.c_str());

      if (0 == hgStagingDir.bytes()) {
         Debug("%s: Can not create staging directory\n", __FUNCTION__);
         return;
      }
      mHGGetFilesInitiated = true;

      if (DnD_AddBlock(mBlockFd, hgStagingDir.c_str())) {
         Debug("%s: add block mBlockFd %d.\n", __FUNCTION__, mBlockFd);
         mHGStagingDir = hgStagingDir;
         mBlockAdded = true;
      } else {
         Debug("%s: unable to add block fd %d dir %s.\n",
                __FUNCTION__, mBlockFd, hgStagingDir.c_str());
      }

      /* Provide URIs for each path in the guest's file list. */
      if (FCP_TARGET_INFO_GNOME_COPIED_FILES == info) {
         mHGCopiedUriList = "copy\n";
         pre = FCP_GNOME_LIST_PRE;
         post = FCP_GNOME_LIST_POST;
      } else if (FCP_TARGET_INFO_URI_LIST == info) {
         pre = DND_URI_LIST_PRE_KDE;
         post = DND_URI_LIST_POST;
      } else {
         Debug("%s: Unknown request target: %s\n", __FUNCTION__,
               sd.get_target().c_str());
         return;
      }

      /* Provide path within vmblock file system instead of actual path. */
      stagingDirName = GetLastDirName(hgStagingDir);
      if (0 == stagingDirName.bytes()) {
         Debug("%s: Can not get staging directory name\n", __FUNCTION__);
         return;
      }

      while ((str = GetNextPath(mHGFCPData, index).c_str()).bytes() != 0) {
         Debug("%s: Path: %s", __FUNCTION__, str.c_str());
         mHGCopiedUriList += pre;
         mHGCopiedUriList += VMBLOCK_MOUNT_POINT;
         mHGCopiedUriList += DIRSEPS + stagingDirName + DIRSEPS + str + post;
      }

      /* Nautilus does not expect FCP_GNOME_LIST_POST after the last uri. See bug 143147. */
      if (FCP_TARGET_INFO_GNOME_COPIED_FILES == info) {
         mHGCopiedUriList.erase(mHGCopiedUriList.size() - 1, 1);
      }
   }

   if (0 == mHGCopiedUriList.bytes()) {
      Debug("%s: Can not get uri list\n", __FUNCTION__);
      return;
   }

   if (!mBlockAdded) {
      /*
       * If there is no blocking driver, wait here till file copy is done.
       * 2 reasons to keep this:
       * 1. If run vmware-user stand-alone as non-root, blocking driver can
       *    not be opened. Debug purpose only.
       * 2. Other platforms (Solaris, etc) may also use this code,
       *    and there is no blocking driver yet.
       *
       * Polling here will not be sufficient for large files (experiments
       * showed it was sufficient for a 256MB file, and failed for a 1GB
       * file, but those numbers are of course context-sensitive and so YMMV).
       * The reason is we are executing in the context of gtkmm callback, and
       * apparently it only has so much patience regarding how quickly we
       * return.
       */
      Debug("%s no blocking driver, waiting for "
            "HG file copy done ...\n", __FUNCTION__);
      while (mFileTransferDone == false) {
         struct timeval tv;
         int nr;

         tv.tv_sec = 0;
         nr = EventManager_ProcessNext(gEventQueue, (uint64 *)&tv.tv_usec);
         if (nr != 1) {
            Debug("%s: unexpected end of loop: returned "
                  "value is %d.\n", __FUNCTION__, nr);
            return;
         }
         if (select(0, NULL, NULL, NULL, &tv) == -1) {
            Debug("%s: error in select (%s).\n", __FUNCTION__,
                  strerror(errno));
            return;
         }
      }
      Debug("%s: file transfer done!\n", __FUNCTION__);
   }

   Debug("%s: providing file list [%s]\n", __FUNCTION__,
         mHGCopiedUriList.c_str());

   sd.set(sd.get_target().c_str(), mHGCopiedUriList.c_str());
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste::LocalClearClipboardCB --
 *
 *      Clear clipboard request from another host application.
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
CopyPasteUI::LocalClearClipboardCB(void)
{
   Debug("%s: got clear callback\n", __FUNCTION__);
   mIsClipboardOwner = FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteUI::LocalClipboardTimestampCB --
 *
 *      Got the local clipboard timestamp. Ask for the primary timestamp.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
CopyPasteUI::LocalClipboardTimestampCB(const Gtk::SelectionData& sd)  // IN
{
   int length = sd.get_length();
   Debug("%s: enter sd.get_length() %d.\n", __FUNCTION__,
         length);
   if (length == 4) {
      mClipTime = ((uint32*) sd.get_data())[0];
      Debug("%s: mClipTime: %"FMT64"u.", __FUNCTION__, mClipTime);
   } else if (length == 8) {
      mClipTime = ((uint64*) sd.get_data())[0];
      Debug("%s: mClipTime: %"FMT64"u.", __FUNCTION__, mClipTime);
   } else {
      Debug("%s: Unable to get mClipTime.", __FUNCTION__);
   }

   Glib::RefPtr<Gtk::Clipboard> refClipboard
      = Gtk::Clipboard::get(GDK_SELECTION_PRIMARY);
   refClipboard->request_contents(TARGET_NAME_TIMESTAMP,
      sigc::mem_fun(this, &CopyPasteUI::LocalPrimTimestampCB));
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteUI::LocalPrimTimestampCB --
 *
 *      Got the local primary timestamp. Choose the most recently changed
 *      clipboard and get the selection from it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
CopyPasteUI::LocalPrimTimestampCB(const Gtk::SelectionData& sd)  // IN
{
   bool unchanged = FALSE;
   int length = sd.get_length();
   Debug("%s: enter sd.get_length() is %d.\n", __FUNCTION__, length);
   if (length == 4) {
      mPrimTime = ((uint32*) sd.get_data())[0];
      Debug("%s: mPrimTime: %"FMT64"u.", __FUNCTION__, mPrimTime);
   } else if (length == 8) {
      mPrimTime = ((uint64*) sd.get_data())[0];
      Debug("%s: mPrimTime: %"FMT64"u.", __FUNCTION__, mPrimTime);
   } else {
      Debug("%s: Unable to get mPrimTime.", __FUNCTION__);
   }

   /* After got both timestamp, choose latest one as active selection. */
   mGHSelection = GDK_SELECTION_PRIMARY;
   if (mClipTime > mPrimTime) {
      mGHSelection = GDK_SELECTION_CLIPBOARD;
      Debug("%s: mClipTimePrev: %"FMT64"u.", __FUNCTION__, mClipTimePrev);
      unchanged = mClipTime <= mClipTimePrev;
   } else {
      Debug("%s: mPrimTimePrev: %"FMT64"u.", __FUNCTION__, mPrimTimePrev);
      unchanged = mPrimTime <= mPrimTimePrev;
   }

   if (unchanged) {
      Debug("%s: clipboard is unchanged, get out\n", __FUNCTION__);
      CPClipboard_SetChanged(&mClipboard, FALSE);
      return;
   }

   mPrimTimePrev = mPrimTime;
   mClipTimePrev = mClipTime;

   /* Ask for available targets from active selection. */
   Glib::RefPtr<Gtk::Clipboard> refClipboard =
      Gtk::Clipboard::get(mGHSelection);

   refClipboard->request_targets(
      sigc::mem_fun(this, &CopyPasteUI::LocalReceivedTargetsCB));
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteUI::LocalReceivedTargetsCB --
 *
 *      Received targets. Check if any desired target available. If so,
 *      ask for all supported contents.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Clipboard owner may send us file list.
 *
 *----------------------------------------------------------------------
 */

void
CopyPasteUI::LocalReceivedTargetsCB(const Glib::StringArrayHandle& targetsArray)  // IN
{
   std::list<utf::string> targets = targetsArray;
   Glib::RefPtr<Gtk::Clipboard> refClipboard = Gtk::Clipboard::get(mGHSelection);

   Debug("%s: enter", __FUNCTION__);
   CPClipboard_Clear(&mClipboard);

   if (std::find(targets.begin(),
                 targets.end(),
                 FCP_TARGET_NAME_GNOME_COPIED_FILES) != targets.end()) {
      Debug("%s: gnome copy file list available\n", __FUNCTION__);
      refClipboard->request_contents(FCP_TARGET_NAME_GNOME_COPIED_FILES,
                                     sigc::mem_fun(this,
                                                   &CopyPasteUI::LocalReceivedFileListCB));
      return;
   }

   if (std::find(targets.begin(),
                 targets.end(),
                 FCP_TARGET_NAME_URI_LIST) != targets.end()) {
      Debug("%s: KDE copy file list available\n", __FUNCTION__);
      refClipboard->request_contents(FCP_TARGET_NAME_URI_LIST,
                                     sigc::mem_fun(this,
                                                   &CopyPasteUI::LocalReceivedFileListCB));
      return;
   }

   Debug("%s: ask for text\n", __FUNCTION__);
   refClipboard->request_text(sigc::mem_fun(this,
                                            &CopyPasteUI::LocalReceivedTextCB));
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteUI::LocalReceivedFileListCB --
 *
 *      Got clipboard or primary selection file list. Parse it and add
 *      it to the crossplaform clipboard. Send clipboard to the host.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
CopyPasteUI::LocalReceivedFileListCB(const Gtk::SelectionData& sd)        // IN
{
   Debug("%s: enter", __FUNCTION__);
   const utf::string target = sd.get_target().c_str();

   if (target == FCP_TARGET_NAME_GNOME_COPIED_FILES ||
       target == FCP_TARGET_NAME_URI_LIST) {
      LocalGetSelectionFileList(sd);
      mCP.SetRemoteClipboard(&mClipboard);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteUI::LocalReceivedTextCB --
 *
 *      Got clipboard (or primary selection) text, add to local clipboard
 *      cache and send clipboard to host.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
CopyPasteUI::LocalReceivedTextCB(const Glib::ustring& text)        // IN
{
   Debug("%s: enter", __FUNCTION__);
   utf::string source = text;

   /*
    * With 'cut' operation OpenOffice will put data into clipboard but
    * set same timestamp for both clipboard and primary selection.
    * If primary timestamp is same as clipboard timestamp, we should try
    * clipboard again if primary selection is empty. For details please
    * refer to bug 300780.
    */
   if (0 == source.bytes() &&
       mClipTime == mPrimTime &&
       mClipTime != 0) {
      mClipTime = 0;
      mPrimTime = 0;
      mGHSelection = GDK_SELECTION_CLIPBOARD;

      /* Ask for available targets from active selection. */
      Glib::RefPtr<Gtk::Clipboard> refClipboard =
         Gtk::Clipboard::get(mGHSelection);

      refClipboard->request_targets(
         sigc::mem_fun(this, &CopyPasteUI::LocalReceivedTargetsCB));
      return;
   }

   Debug("%s: Got text: %s", __FUNCTION__, source.c_str());

   /* Add NUL terminator. */
   CPClipboard_SetItem(&mClipboard, CPFORMAT_TEXT, source.c_str(),
                       source.bytes() + 1);
   mCP.SetRemoteClipboard(&mClipboard);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::LocalGetSelectionFileList --
 *
 *      Construct local file list and remote file list from selection data.
 *      Called by both DnD and FCP.
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
CopyPasteUI::LocalGetSelectionFileList(const Gtk::SelectionData& sd)      // IN
{
   utf::string source;
   char *newPath;
   char *newRelPath;
   size_t newPathLen;
   size_t index = 0;
   DnDFileList fileList;
   DynBuf buf;
   uint64 totalSize = 0;
   int64 size;

   /*
    * Turn the uri list into two \0  delimited lists. One for full paths and
    * one for just the last path component.
    */
   source = sd.get_data_as_string().c_str();
   Debug("%s: Got file list: [%s]\n", __FUNCTION__, source.c_str());

   /*
    * In gnome, before file list there may be a extra line indicating it
    * is a copy or cut.
    */
   if (source.startsWith("copy\n")) {
      source = source.erase(0, 5);
   }

   if (source.startsWith("cut\n")) {
      source = source.erase(0, 4);
   }

   while (source.bytes() > 0 &&
          (source[0] == '\n' || source[0] == '\r' || source[0] == ' ')) {
      source = source.erase(0, 1);
   }

   while ((newPath = DnD_UriListGetNextFile(source.c_str(),
                                            &index,
                                            &newPathLen)) != NULL) {

      /*
       * Parse relative path.
       */
      newRelPath = Str_Strrchr(newPath, DIRSEPC) + 1; // Point to char after '/'

      /*
       * XXX For directory, value is -1, so if there is any directory,
       * total size is not accurate.
       */
      if ((size = File_GetSize(newPath)) >= 0) {
         totalSize += size;
      } else {
         Debug("%s: Unable to get file size for %s\n", __FUNCTION__, newPath);
      }

      Debug("%s: Adding newPath '%s' newRelPath '%s'\n", __FUNCTION__,
            newPath, newRelPath);
      fileList.AddFile(newPath, newRelPath);
      free(newPath);
   }

   DynBuf_Init(&buf);
   fileList.SetFileSize(totalSize);
   Debug("%s: totalSize is %"FMT64"u\n", __FUNCTION__, totalSize);
   fileList.ToCPClipboard(&buf, false);
   CPClipboard_SetItem(&mClipboard, CPFORMAT_FILELIST, DynBuf_Get(&buf),
                       DynBuf_GetSize(&buf));
   DynBuf_Destroy(&buf);
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPasteUI::GetNextPath --
 *
 *      Provides a substring containing the next path from the provided
 *      NUL-delimited string starting at the provided index.
 *
 * Results:
 *      A string with the next path or "" if there are no more paths.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

utf::utf8string
CopyPasteUI::GetNextPath(utf::utf8string& str,    // IN: NUL-delimited path list
                         size_t& index)           // IN/OUT: current index into string
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
   Debug("%s: nextpath: %s", __FUNCTION__, ret.c_str());
   index++;
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::GetLastDirName --
 *
 *      Try to get last directory name from a full path name.
 *
 * Results:
 *      Last dir name in the full path name if sucess, empty str otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

utf::string
CopyPasteUI::GetLastDirName(const utf::string &str)     // IN
{
   utf::string ret;
   size_t start;
   size_t end;

   end = str.bytes() - 1;
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
 * CopyPasteUI::GetRemoteClipboardCB --
 *
 *    Invoked when got data from host. Update the internal data to get the file
 *    names or the text that needs to be transferred.
 *
 *    Method for copy and paste from host to guest.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUI::GetRemoteClipboardCB(const CPClipboard *clip) // IN
{
   Glib::RefPtr<Gtk::Clipboard> refClipboard =
      Gtk::Clipboard::get(GDK_SELECTION_CLIPBOARD);
   Glib::RefPtr<Gtk::Clipboard> refPrimary =
      Gtk::Clipboard::get(GDK_SELECTION_PRIMARY);
   void *buf;
   size_t sz;

   Debug("%s: enter\n", __FUNCTION__);
   if (!clip) {
      Debug("%s: No clipboard contents.", __FUNCTION__);
      return;
   }

   /* Clear the clipboard contents if we are the owner. */
   if (mIsClipboardOwner) {
      refClipboard->clear();
      refPrimary->clear();
      mIsClipboardOwner = FALSE;
      Debug("%s: Cleared local clipboard", __FUNCTION__);
   }

   if (CPClipboard_GetItem(clip, CPFORMAT_TEXT, &buf, &sz)) {
      Debug("%s: Text data: %s.\n", __FUNCTION__, (char *)buf);
      refClipboard->set_text((const char *) buf);
      refPrimary->set_text((const char *) buf);
   }
   if (CPClipboard_GetItem(clip, CPFORMAT_FILELIST, &buf, &sz)) {
      Debug("%s: File data.\n", __FUNCTION__);
      DnDFileList flist;
      flist.FromCPClipboard(buf, sz);
      mHGFCPData = flist.GetRelPathsStr();

      refClipboard->set(mListTargets,
                        sigc::mem_fun(this, &CopyPasteUI::LocalGetFileRequestCB),
                        sigc::mem_fun(this, &CopyPasteUI::LocalClearClipboardCB));
      refPrimary->set(mListTargets,
                      sigc::mem_fun(this, &CopyPasteUI::LocalGetFileRequestCB),
                      sigc::mem_fun(this, &CopyPasteUI::LocalClearClipboardCB));

      mIsClipboardOwner = TRUE;
      mHGGetListTime = GetCurrentTime();
      mHGGetFilesInitiated = false;
      mHGCopiedUriList = "";
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::GetLocalFilesDone --
 *
 *    Callback when CopyPasteUI::GetLocalFiles is done, which finishes the file
 *    copying from host to guest staging directory. This function notifies
 *    the Copy/Paste data object and end its waiting state in order to continue
 *    the file copying from local staging directory to local target directory.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUI::GetLocalFilesDone(bool success)
{
   Debug("%s: enter success %d\n", __FUNCTION__, success);

   if (mBlockAdded) {
      Debug("%s: removing block mBlockFd %d\n", __FUNCTION__, mBlockFd);
      DnD_RemoveBlock(mBlockFd, mHGStagingDir.c_str());
      mBlockAdded = false;
   }

   mFileTransferDone = true;
   if (success) {
      /*
       * Mark current staging dir to be deleted on next reboot for FCP. The
       * file will not be deleted after reboot if it is moved to another
       * location by target application.
       */
      DnD_DeleteStagingFiles(mHGStagingDir.c_str(), TRUE);
   } else {
      /* Copied files are already removed in common layer. */
      mHGStagingDir.clear();
   }
   mHGGetFilesInitiated = false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::Reset --
 *
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUI::Reset(void)
{
   Debug("%s: enter\n", __FUNCTION__);
   /* Cancel any pending file transfer. */
}

