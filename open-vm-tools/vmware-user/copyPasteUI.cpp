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
   #include "file.h"
   #include "dnd.h"
   #include "dndMsg.h"
   #include "dndClipboard.h"
   #include "cpName.h"
   #include "debug.h"
   #include "cpNameUtil.h"
   #include "rpcout.h"
   #include "eventManager.h"
   #include "vmware/guestrpc/tclodefs.h"
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
   mHGGetFilesInitiated(false),
   mFileTransferDone(false),
   mBlockAdded(false),
   mBlockCtrl(0),
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

bool
CopyPasteUI::Init()
{
   if (mInited) {
      return true;
   }

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
      return false;
   }
   Debug("%s: set copypaste version 3\n", __FUNCTION__);

   mCP.newClipboard.connect(
      sigc::mem_fun(this, &CopyPasteUI::GetRemoteClipboardCB));
   mCP.localGetClipboard.connect(
      sigc::mem_fun(this, &CopyPasteUI::GetLocalClipboard));
   mCP.localGetFilesDoneChanged.connect(
      sigc::mem_fun(this, &CopyPasteUI::GetLocalFilesDone));
   mInited = true;
   return true;
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
 * CopyPasteUI::Cancel --
 *
 *    Cancel file transfer and remove block.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUI::Cancel()
{
   Debug("%s: enter\n", __FUNCTION__);
   if (mBlockAdded) {
      DnD_DeleteStagingFiles(mHGStagingDir.c_str(), FALSE);
      Debug("%s: removing block for %s\n", __FUNCTION__, mHGStagingDir.c_str());
      mBlockCtrl->RemoveBlock(mBlockCtrl->fd, mHGStagingDir.c_str());
      mBlockAdded = false;
   }

   mFileTransferDone = true;
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

   sd.set(sd.get_target().c_str(), "");

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

      if (DnD_BlockIsReady(mBlockCtrl) && mBlockCtrl->AddBlock(mBlockCtrl->fd, hgStagingDir.c_str())) {
         Debug("%s: add block for %s.\n",
               __FUNCTION__, hgStagingDir.c_str());
         mBlockAdded = true;
      } else {
         Debug("%s: unable to add block for %s.\n",
               __FUNCTION__, hgStagingDir.c_str());
      }

      mHGStagingDir = hgStagingDir;

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
         if (mBlockAdded) {
            mHGCopiedUriList += mBlockCtrl->blockRoot;
            mHGCopiedUriList += DIRSEPS + stagingDirName + DIRSEPS + str + post;
         } else {
            mHGCopiedUriList += DIRSEPS + hgStagingDir + DIRSEPS + str + post;
         }
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
            "HG file copy done ... mFileTransferDone is %d\n", __FUNCTION__,
            (int) mFileTransferDone);
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
 * CopyPasteUI::LocalGetTextOrRTFRequestCB --
 *
 *      Callback from a text or RTF paste request from another guest application.
 *      H->G copy paste only.
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
CopyPasteUI::LocalGetTextOrRTFRequestCB(Gtk::SelectionData& sd, // IN/OUT
                                        guint info)             // Ignored
{
   sd.set(sd.get_target().c_str(), "");

   if (!mCP.IsCopyPasteAllowed()) {
      return;
   }

   const utf::string target = sd.get_target().c_str();

   Debug("%s: Got paste request, target is %s\n",
         __FUNCTION__, target.c_str());

   if (target == TARGET_NAME_APPLICATION_RTF ||
       target == TARGET_NAME_TEXT_RICHTEXT) {
      if (0 == mHGRTFData.bytes()) {
         Debug("%s: Can not get valid RTF data\n", __FUNCTION__);
         return;
      }

      Debug("%s: providing RTF data, size %"FMTSZ"u\n",
            __FUNCTION__, mHGRTFData.bytes());

      sd.set(target.c_str(), mHGRTFData.c_str());
   }

   if (target == TARGET_NAME_STRING ||
       target == TARGET_NAME_TEXT_PLAIN ||
       target == TARGET_NAME_UTF8_STRING ||
       target == TARGET_NAME_COMPOUND_TEXT) {
      if (0 == mHGTextData.bytes()) {
         Debug("%s: Can not get valid text data\n", __FUNCTION__);
         return;
      }
      Debug("%s: providing plain text, size %"FMTSZ"u\n",
            __FUNCTION__, mHGTextData.bytes());

      sd.set(target.c_str(), mHGTextData.c_str());
   }
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
   }

   Glib::RefPtr<Gtk::Clipboard> refClipboard;
   bool flipped = false;
again:
   bool validDataInClip = false;
   refClipboard = Gtk::Clipboard::get(mGHSelection);

   Debug("%s: trying %s selection.\n", __FUNCTION__,
          mGHSelection == GDK_SELECTION_PRIMARY ? "Primary" : "Clip");

   CPClipboard_Clear(&mClipboard);

   /* First check for URIs. This must always be done first */
   bool haveURIs = false;
   std::string format;
   if (refClipboard->wait_is_target_available(FCP_TARGET_NAME_GNOME_COPIED_FILES)) {
      format = FCP_TARGET_NAME_GNOME_COPIED_FILES;
      haveURIs = true;
   } else if (refClipboard->wait_is_target_available(FCP_TARGET_NAME_URI_LIST)) {
      format = FCP_TARGET_NAME_URI_LIST;
      haveURIs = true;
   }

   if (haveURIs) {
      refClipboard->request_contents(format,
                                     sigc::mem_fun(this,
                                                   &CopyPasteUI::LocalReceivedFileListCB));
      return;
   }

   /* Try to get image data from clipboard. */
   Glib::RefPtr<Gdk::Pixbuf> img = refClipboard->wait_for_image();
   gsize bufSize;
   if (img) {
      gchar *buf = NULL;

      img->save_to_buffer(buf, bufSize, Glib::ustring("png"));
      if (bufSize > 0  &&
          bufSize <= (int)CPCLIPITEM_MAX_SIZE_V3 &&
          CPClipboard_SetItem(&mClipboard, CPFORMAT_IMG_PNG,
                              buf, bufSize)) {
         mCP.SetRemoteClipboard(&mClipboard);
         Debug("%s: Got PNG: %"FMTSZ"u\n", __FUNCTION__, bufSize);
      } else {
         Debug("%s: Failed to get PNG\n", __FUNCTION__);
      }
      g_free(buf);
      return;
   }

   /* Try to get RTF data from clipboard. */
   bool haveRTF = false;
   if (refClipboard->wait_is_target_available(TARGET_NAME_APPLICATION_RTF)) {
      Debug("%s: RTF is available\n", __FUNCTION__);
      format = TARGET_NAME_APPLICATION_RTF;
      haveRTF = true;
   }
   if (refClipboard->wait_is_target_available(TARGET_NAME_TEXT_RICHTEXT)) {
      Debug("%s: RICHTEXT is available\n", __FUNCTION__);
      format = TARGET_NAME_TEXT_RICHTEXT;
      haveRTF = true;
   }

   if (haveRTF) {
      /*
       * There is a function for waiting for rtf data, but that was leading
       * to crashes. It's use required we instantiate a class that implements
       * Gtk::TextBuffer and then query that class for a reference to it's
       * TextBuffer instance. This all compiled fine but crashed in testing
       * so we opt to use the more generic API here which seemed more stable.
       */
      Gtk::SelectionData sdata = refClipboard->wait_for_contents(format);
      bufSize = sdata.get_length();
      if (bufSize > 0  &&
          bufSize <= (int)CPCLIPITEM_MAX_SIZE_V3 &&
          CPClipboard_SetItem(&mClipboard, CPFORMAT_RTF,
                              (const void *)sdata.get_data(), bufSize + 1)) {
         validDataInClip = true;
         Debug("%s: Got RTF\n", __FUNCTION__);
      } else {
         Debug("%s: Failed to get RTF size %d max %d\n",
               __FUNCTION__, (int) bufSize, (int)CPCLIPITEM_MAX_SIZE_V3);
      }
   }

   /* Try to get Text data from clipboard. */
   if (refClipboard->wait_is_text_available()) {
      Debug("%s: ask for text\n", __FUNCTION__);
      Glib::ustring str = refClipboard->wait_for_text();
      bufSize = str.bytes();
      if (bufSize > 0  &&
          bufSize <= (int)CPCLIPITEM_MAX_SIZE_V3 &&
          CPClipboard_SetItem(&mClipboard, CPFORMAT_TEXT,
                              (const void *)str.data(), bufSize + 1)) {
         validDataInClip = true;
         Debug("%s: Got TEXT: %"FMTSZ"u\n", __FUNCTION__, bufSize);
      } else {
         Debug("%s: Failed to get TEXT\n", __FUNCTION__);
      }
   }

   if (validDataInClip) {
      /*
       * RTF or text data (or both) in the clipboard.
       */
      mCP.SetRemoteClipboard(&mClipboard);
   } else if (!flipped) {
      /*
       * If we get here, we got nothing (no image, URI, text) so
       * try the other selection.
       */
      Debug("%s: got nothing for this selection, try the other.\n",
            __FUNCTION__);
      mGHSelection = mGHSelection == GDK_SELECTION_PRIMARY ?
                     GDK_SELECTION_CLIPBOARD : GDK_SELECTION_PRIMARY;
      flipped = true;
      goto again;
   }
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
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUI::LocalGetFileContentsRequestCB --
 *
 *      Callback from a file paste request from another guest application.
 *      Return the file list.
 *
 *      H->G only.
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
CopyPasteUI::LocalGetFileContentsRequestCB(Gtk::SelectionData& sd, // IN
                                           guint info)             // IN
{
   std::vector<utf::string>::const_iterator iter;
   utf::string uriList = "";
   utf::string pre;
   utf::string post;

   sd.set(sd.get_target().c_str(), "");

   /* Provide URIs for each path in the guest's file list. */
   if (FCP_TARGET_INFO_GNOME_COPIED_FILES == info) {
      uriList = "copy\n";
      pre = FCP_GNOME_LIST_PRE;
      post = FCP_GNOME_LIST_POST;
   } else if (FCP_TARGET_INFO_URI_LIST == info) {
      pre = DND_URI_LIST_PRE_KDE;
      post = DND_URI_LIST_POST;
   } else {
      Debug("%s: Unknown request target: %s\n",
            __FUNCTION__, sd.get_target().c_str());
      return;
   }

   for (iter = mHGFileContentsList.begin();
        iter != mHGFileContentsList.end();
        iter++) {
      uriList += pre + *iter + post;
   }

   /* Nautilus does not expect FCP_GNOME_LIST_POST after the last uri. See bug 143147. */
   if (FCP_TARGET_INFO_GNOME_COPIED_FILES == info) {
      uriList.erase(uriList.size() - 1, 1);
   }

   if (0 == uriList.bytes()) {
      Debug("%s: Can not get uri list\n", __FUNCTION__);
      return;
   }

   Debug("%s: providing file list [%s]\n", __FUNCTION__, uriList.c_str());

   sd.set(sd.get_target().c_str(), uriList.c_str());
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

   mHGTextData.clear();
   mHGRTFData.clear();
   mHGFCPData.clear();

   if (CPClipboard_ItemExists(clip, CPFORMAT_TEXT) ||
       CPClipboard_ItemExists(clip, CPFORMAT_RTF)) {
      std::list<Gtk::TargetEntry> targets;

      if (CPClipboard_GetItem(clip, CPFORMAT_TEXT, &buf, &sz)) {
         Gtk::TargetEntry stringText(TARGET_NAME_STRING);
         Gtk::TargetEntry plainText(TARGET_NAME_TEXT_PLAIN);
         Gtk::TargetEntry utf8Text(TARGET_NAME_UTF8_STRING);
         Gtk::TargetEntry compountText(TARGET_NAME_COMPOUND_TEXT);

         Debug("%s: Text data, size %"FMTSZ"u.\n", __FUNCTION__, sz);
         targets.push_back(stringText);
         targets.push_back(plainText);
         targets.push_back(utf8Text);
         targets.push_back(compountText);
         mHGTextData = utf::string((const char *)buf, STRING_ENCODING_UTF8);

         mIsClipboardOwner = TRUE;
      }

      if (CPClipboard_GetItem(clip, CPFORMAT_RTF, &buf, &sz)) {
         Debug("%s: RTF data, size %"FMTSZ"u.\n", __FUNCTION__, sz);
         Gtk::TargetEntry appRtf(TARGET_NAME_APPLICATION_RTF);
         Gtk::TargetEntry textRtf(TARGET_NAME_TEXT_RICHTEXT);

         targets.push_back(appRtf);
         targets.push_back(textRtf);
         mHGRTFData = utf::string((const char *)buf, STRING_ENCODING_UTF8);

         mIsClipboardOwner = TRUE;
      }

      refClipboard->set(targets,
                        sigc::mem_fun(this, &CopyPasteUI::LocalGetTextOrRTFRequestCB),
                        sigc::mem_fun(this, &CopyPasteUI::LocalClearClipboardCB));
      refPrimary->set(targets,
                      sigc::mem_fun(this, &CopyPasteUI::LocalGetTextOrRTFRequestCB),
                      sigc::mem_fun(this, &CopyPasteUI::LocalClearClipboardCB));
      return;
   }

   if (CPClipboard_GetItem(clip, CPFORMAT_IMG_PNG, &buf, &sz)) {
      Debug("%s: PNG data, size %"FMTSZ"u.\n", __FUNCTION__, sz);
      /* Try to load buf into pixbuf, and write to local clipboard. */
      Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create();

      if (loader) {
         loader->write((const guint8 *)buf, sz);
         loader->close();

         refClipboard->set_image(loader->get_pixbuf());
         refPrimary->set_image(loader->get_pixbuf());
      }
      return;
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

   if (CPClipboard_ItemExists(clip, CPFORMAT_FILECONTENTS)) {
      Debug("%s: File contents data\n", __FUNCTION__);
      if (LocalPrepareFileContents(clip)) {
         refClipboard->set(mListTargets,
                           sigc::mem_fun(this, &CopyPasteUI::LocalGetFileContentsRequestCB),
                           sigc::mem_fun(this, &CopyPasteUI::LocalClearClipboardCB));
         refPrimary->set(mListTargets,
                         sigc::mem_fun(this, &CopyPasteUI::LocalGetFileContentsRequestCB),
                         sigc::mem_fun(this, &CopyPasteUI::LocalClearClipboardCB));
         mIsClipboardOwner = TRUE;
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPasteUI::LocalPrepareFileContents --
 *
 *      Try to extract file contents from mClipboard. Write all files to a
 *      temporary staging directory. Construct uri list.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

bool
CopyPasteUI::LocalPrepareFileContents(const CPClipboard *clip) // IN
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

   if (!CPClipboard_GetItem(clip, CPFORMAT_FILECONTENTS, &buf, &sz)) {
      Debug("%s: CPClipboard_GetItem failed\n", __FUNCTION__);
      return false;
   }

   /* Extract file contents from buf. */
   xdrmem_create(&xdrs, (char *)buf, sz, XDR_DECODE);
   memset(&fileContents, 0, sizeof fileContents);

   if (!xdr_CPFileContents(&xdrs, &fileContents)) {
      Debug("%s: xdr_CPFileContents failed.\n", __FUNCTION__);
      xdr_destroy(&xdrs);
      return false;
   }
   xdr_destroy(&xdrs);

   contentsList = fileContents.CPFileContents_u.fileContentsV1;
   if (!contentsList) {
      Debug("%s: invalid contentsList.\n", __FUNCTION__);
      goto exit;
   }

   nFiles = contentsList->fileItem.fileItem_len;
   if (0 == nFiles) {
      Debug("%s: invalid nFiles.\n", __FUNCTION__);
      goto exit;
   }

   fileItem = contentsList->fileItem.fileItem_val;
   if (!fileItem) {
      Debug("%s: invalid fileItem.\n", __FUNCTION__);
      goto exit;
   }

   /*
    * Write files into a temporary staging directory. These files will be moved
    * to final destination, or deleted on next reboot.
    */
   tempDir = DnD_CreateStagingDirectory();
   if (!tempDir) {
      Debug("%s: DnD_CreateStagingDirectory failed.\n", __FUNCTION__);
      goto exit;
   }

   mHGFileContentsList.clear();

   for (i = 0; i < nFiles; i++) {
      utf::string fileName;
      utf::string filePathName;
      VmTimeType createTime = -1;
      VmTimeType accessTime = -1;
      VmTimeType writeTime = -1;
      VmTimeType attrChangeTime = -1;

      if (!fileItem[i].cpName.cpName_val ||
          0 == fileItem[i].cpName.cpName_len) {
         Debug("%s: invalid fileItem[%"FMTSZ"u].cpName.\n", __FUNCTION__, i);
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
         Debug("%s: created directory [%s].\n",
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
         Debug("%s: created file [%s].\n", __FUNCTION__, filePathName.c_str());
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
         Debug("%s: File_SetTimes failed with file [%s].\n",
               __FUNCTION__, filePathName.c_str());
      }

      /* Update file permission attributes. */
      if (fileItem->validFlags & CP_FILE_VALID_PERMS) {
         if (Posix_Chmod(filePathName.c_str(),
                         fileItem->permissions) < 0) {
            /* Not a critical error, only log it. */
            Debug("%s: Posix_Chmod failed with file [%s].\n",
                  __FUNCTION__, filePathName.c_str());
         }
      }

      /*
       * If there is no DIRSEPC inside the fileName, this file/directory is a
       * top level one. We only put top level name into uri list.
       */
      if (fileName.find(DIRSEPS, 0) == utf::string::npos) {
         mHGFileContentsList.push_back(filePathName);
      }
   }
   Debug("%s: created uri list\n", __FUNCTION__);
   ret = true;

exit:
   xdr_free((xdrproc_t)xdr_CPFileContents, (char *)&fileContents);
   if (tempDir && !ret) {
      DnD_DeleteStagingFiles(tempDir, FALSE);
   }
   free(tempDir);
   return ret;
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
      Debug("%s: removing block for %s\n", __FUNCTION__, mHGStagingDir.c_str());
      mBlockCtrl->RemoveBlock(mBlockCtrl->fd, mHGStagingDir.c_str());
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

