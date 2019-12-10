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

/*
 * copyPasteUIX11.cpp --
 *
 *    This class implements the methods that allows CopyPaste between host
 *    and guest.
 *
 *    For a perspective on X copy/paste, see
 *    http://www.jwz.org/doc/x-cut-and-paste.html
 */


/*
 * A Word on Selection Timestamps
 *
 * ICCCM §2.6.2 Target Atoms
 *    The TIMESTAMP property is an INTEGER.
 *
 * ICCCM §2.7 Use of Selection Properties
 *    The format of INTEGER is 32.
 *
 * XGetWindowProperty(3)
 *    “If the returned format is 32, the property will be stored as an
 *    array of longs (which in a 64-bit application will be 64-bit values
 *    that are padded in the upper 4 bytes).”
 *
 * For all intents and purposes, on x86 and x86_64 X selection timestamps
 * are a 32-bit quantity. (X11/Xproto.h's xSetSelectionOwnerReq defines the
 * “time” member as the lower 32 bits of type Time.) X clients, on the
 * other hand, operate on Time as either a CARD32 (uint32) or an unsigned
 * long (i.e., on a 64-bit machine Time may occupy 8 bytes).
 *
 * Breaking it down:
 *   · When Gtk+ provides an X11 selection via Gtk::SelectionData, on a
 *     32-bit machine we'll have 4 bytes of raw data.  Everything's copacetic.
 *   · On a 64-bit machine, even if the source client provides 32 bits of
 *     timestamp data, Gtk+ will decode as an unsigned long and provide 8
 *     bytes of raw data.
 *   · On a 64-bit machine with a wacky application which actually tries
 *     to record a full 64 bits of timestamp data, Gtk+ will provide 16 bytes:
 *     <low 32 bits> <32 bits of 0s> <high 32 bits> <32 bits of 0s>.  (See
 *     PR 882322, mrxvt.)
 *
 *   In all instances, we're interested in _only_ the lowest 32 bits, so we'll
 *   ignore everything else.
 */


#define G_LOG_DOMAIN "dndcp"

#include <sys/time.h>
#include <time.h>
#include <cxxabi.h>
#include "copyPasteDnDWrapper.h"
#include "copyPasteUIX11.h"
#include "dndFileList.hh"
#include "guestDnDCPMgr.hh"
#include "tracer.hh"
#include "vmblock.h"
#include "fcntl.h"
#include "file.h"
#include "dnd.h"
#include "dndMsg.h"

extern "C" {
   #include "dndClipboard.h"
   #include "cpName.h"
   #include "cpNameUtil.h"
   #include "rpcout.h"
   #include "vmware/guestrpc/tclodefs.h"
}

/*
 * Gtk 1.2 doesn't know about the CLIPBOARD selection, but that doesn't
 * matter, we just create the atom we need directly in main().
 *
 * This is for V1 text copy paste only!
 */
#ifndef GDK_SELECTION_CLIPBOARD
GdkAtom GDK_SELECTION_CLIPBOARD;
#endif

#ifndef GDK_SELECTION_TYPE_TIMESTAMP
GdkAtom GDK_SELECTION_TYPE_TIMESTAMP;
#endif

#ifndef GDK_SELECTION_TYPE_UTF8_STRING
GdkAtom GDK_SELECTION_TYPE_UTF8_STRING;
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::CopyPasteUIX11 --
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

CopyPasteUIX11::CopyPasteUIX11()
 : mClipboardEmpty(true),
   mHGStagingDir(""),
   mIsClipboardOwner(false),
   mClipTime(0),
   mPrimTime(0),
   mLastTimestamp(0),
   mThread(0),
   mHGGetFileStatus(DND_FILE_TRANSFER_NOT_STARTED),
   mBlockAdded(false),
   mBlockCtrl(0),
   mInited(false),
   mTotalFileSize(0),
   mGetTimestampOnly(false)
{
   TRACE_CALL();
   GuestDnDCPMgr *p = GuestDnDCPMgr::GetInstance();
   ASSERT(p);
   mCP = p->GetCopyPasteMgr();
   ASSERT(mCP);

   mThreadParams.fileBlockCondExit = false;
   pthread_mutex_init(&mThreadParams.fileBlockMutex, NULL);
   pthread_cond_init(&mThreadParams.fileBlockCond, NULL);
   mThreadParams.cp = this;
   int ret = pthread_create(&mThread,
                            NULL,
                            FileBlockMonitorThread,
                            (void *)&(this->mThreadParams));
   if (ret != 0) {
      Warning("%s: Create thread failed, errno:%d.\n", __FUNCTION__, ret);
      mThread = 0;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::Init --
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
CopyPasteUIX11::Init()
{
   TRACE_CALL();
   if (mInited) {
      g_debug("%s: mInited is true\n", __FUNCTION__);
      return true;
   }

   CPClipboard_Init(&mClipboard);

   Gtk::TargetEntry gnome(FCP_TARGET_NAME_GNOME_COPIED_FILES);
   Gtk::TargetEntry kde(FCP_TARGET_NAME_URI_LIST);
   Gtk::TargetEntry nautilus(FCP_TARGET_NAME_NAUTILUS_FILES);
   gnome.set_info(FCP_TARGET_INFO_GNOME_COPIED_FILES);
   kde.set_info(FCP_TARGET_INFO_URI_LIST);
   nautilus.set_info(FCP_TARGET_INFO_NAUTILUS_FILES);

   mListTargets.push_back(gnome);
   mListTargets.push_back(kde);
   mListTargets.push_back(nautilus);

   mCP->srcRecvClipChanged.connect(
      sigc::mem_fun(this, &CopyPasteUIX11::GetRemoteClipboardCB));
   mCP->destRequestClipChanged.connect(
      sigc::mem_fun(this, &CopyPasteUIX11::GetLocalClipboard));
   mCP->getFilesDoneChanged.connect(
      sigc::mem_fun(this, &CopyPasteUIX11::GetLocalFilesDone));

   mInited = true;
   return true;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::~CopyPaste --
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

CopyPasteUIX11::~CopyPasteUIX11()
{
   TRACE_CALL();
   CPClipboard_Destroy(&mClipboard);
   /* Any files from last unfinished file transfer should be deleted. */
   if (DND_FILE_TRANSFER_IN_PROGRESS == mHGGetFileStatus &&
       !mHGStagingDir.empty()) {
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
   if (mBlockAdded) {
      g_debug("%s: removing block for %s\n", __FUNCTION__, mHGStagingDir.c_str());
      /* We need to make sure block subsystem has not been shut off. */
      mBlockAdded = false;
      if (DnD_BlockIsReady(mBlockCtrl)) {
         mBlockCtrl->RemoveBlock(mBlockCtrl->fd, mHGStagingDir.c_str());
      }
   }

   TerminateThread();
   pthread_mutex_destroy(&mThreadParams.fileBlockMutex);
   pthread_cond_destroy(&mThreadParams.fileBlockCond);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::VmxCopyPasteVersionChanged --
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
CopyPasteUIX11::VmxCopyPasteVersionChanged(RpcChannel *chan,    // IN
                                           uint32 version)      // IN
{
   ASSERT(mCP);
   g_debug("%s: new version is %d\n", __FUNCTION__, version);
   mCP->VmxCopyPasteVersionChanged(version);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::GetLocalClipboard --
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

void
CopyPasteUIX11::GetLocalClipboard(void)
{
   g_debug("%s: enter.\n", __FUNCTION__);

   if (mIsClipboardOwner) {
      /* If we are clipboard owner, send a not-changed clip to host. */
      g_debug("%s: we are owner, send unchanged clip back.\n", __FUNCTION__);
      SendClipNotChanged();
      return;
   }

   if (!mCP->IsCopyPasteAllowed()) {
      g_debug("%s: copyPaste is not allowed\n", __FUNCTION__);
      return;
   }

   Glib::RefPtr<Gtk::Clipboard> refClipboard =
      Gtk::Clipboard::get(GDK_SELECTION_CLIPBOARD);

   mClipTime = 0;
   mPrimTime = 0;
   mGHSelection = GDK_SELECTION_CLIPBOARD;
   mGetTimestampOnly = false;
   g_debug("%s: retrieving timestamps\n", __FUNCTION__);
   refClipboard->request_contents(TARGET_NAME_TIMESTAMP,
      sigc::mem_fun(this, &CopyPasteUIX11::LocalClipboardTimestampCB));
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::GetCurrentTime --
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
CopyPasteUIX11::GetCurrentTime(void)
{
   struct timeval tv;
   VmTimeType curTime;

   if (gettimeofday(&tv, NULL) != 0) {
      g_debug("%s: gettimeofday failed!\n", __FUNCTION__);
      return (VmTimeType) 0;
   }
   curTime = (tv.tv_sec * 1000000 + tv.tv_usec);
   return curTime;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalGetFileRequestCB --
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
CopyPasteUIX11::LocalGetFileRequestCB(Gtk::SelectionData& sd,        // IN:
                                      guint info)                    // IN:
{
   g_debug("%s: enter.\n", __FUNCTION__);

   if (!mIsClipboardOwner || !mCP->IsCopyPasteAllowed()) {
      g_debug("%s: not clipboard ownder, or copy paste not allowed, returning.\n",
            __FUNCTION__);
      sd.set(sd.get_target().c_str(), "");
      return;
   }

   g_debug("%s: Got paste request, target is %s\n", __FUNCTION__,
         sd.get_target().c_str());

   if (mHGGetFileStatus != DND_FILE_TRANSFER_NOT_STARTED) {
      /*
       * On KDE (at least), we can see this multiple times, so ignore if
       * we are already getting files.
       */
      g_debug("%s: GetFiles already started, returning uriList [%s]\n",
              __FUNCTION__, mHGCopiedUriList.c_str());
      sd.set(sd.get_target().c_str(), mHGCopiedUriList.c_str());
      return;
   } else {
      utf::string str;
      utf::string hgStagingDir;
      utf::string stagingDirName;
      utf::string pre;
      utf::string post;
      size_t index = 0;

      hgStagingDir = utf::CopyAndFree(DnD_CreateStagingDirectory());
      g_debug("%s: Getting files. Staging dir: %s", __FUNCTION__,
            hgStagingDir.c_str());

      if (0 == hgStagingDir.bytes()) {
         g_debug("%s: Can not create staging directory\n", __FUNCTION__);
         sd.set(sd.get_target().c_str(), "");
         return;
      }
      mHGGetFileStatus = DND_FILE_TRANSFER_IN_PROGRESS;

      mBlockAdded = false;
      if (DnD_BlockIsReady(mBlockCtrl) && mBlockCtrl->AddBlock(mBlockCtrl->fd, hgStagingDir.c_str())) {
         g_debug("%s: add block for %s.\n",
               __FUNCTION__, hgStagingDir.c_str());
         mBlockAdded = true;
         pthread_mutex_lock(&mThreadParams.fileBlockMutex);
         mThreadParams.fileBlockCondExit = false;
         mThreadParams.fileBlockName = VMBLOCK_FUSE_NOTIFY_ROOT;
         mThreadParams.fileBlockName += DIRSEPS;
         mThreadParams.fileBlockName += GetLastDirName(hgStagingDir);
         pthread_cond_signal(&mThreadParams.fileBlockCond);
         pthread_mutex_unlock(&mThreadParams.fileBlockMutex);
      } else {
         g_debug("%s: unable to add block for %s.\n",
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
      } else if (FCP_TARGET_INFO_NAUTILUS_FILES == info) {
         mHGCopiedUriList =
            utf::string(FCP_TARGET_MIME_NAUTILUS_FILES) + "\ncopy\n";
         pre = FCP_GNOME_LIST_PRE;
         post = FCP_GNOME_LIST_POST;
      } else {
         g_debug("%s: Unknown request target: %s\n", __FUNCTION__,
               sd.get_target().c_str());
         sd.set(sd.get_target().c_str(), "");
         return;
      }

      /* Provide path within vmblock file system instead of actual path. */
      stagingDirName = GetLastDirName(hgStagingDir);
      if (0 == stagingDirName.bytes()) {
         g_debug("%s: Can not get staging directory name\n", __FUNCTION__);
         sd.set(sd.get_target().c_str(), "");
         return;
      }

      while ((str = GetNextPath(mHGFCPData, index).c_str()).bytes() != 0) {
         g_debug("%s: Path: %s", __FUNCTION__, str.c_str());
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
      g_debug("%s: Can not get uri list\n", __FUNCTION__);
      sd.set(sd.get_target().c_str(), "");
      return;
   }

   if (!mBlockAdded) {
      /*
       * If there is no blocking driver, wait here till file copy is done.
       * 2 reasons to keep this:
       * 1. If run vmware-user stand-alone as non-root, blocking driver can
       *    not be opened. g_debug purpose only.
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
      CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();
      ToolsAppCtx *ctx = wrapper->GetToolsAppCtx();
      while (mHGGetFileStatus == DND_FILE_TRANSFER_IN_PROGRESS) {
         struct timeval tv;

         tv.tv_sec = 0;
         g_main_context_iteration(g_main_loop_get_context(ctx->mainLoop),
                                  FALSE);
         if (select(0, NULL, NULL, NULL, &tv) == -1) {
            g_debug("%s: error in select (%s).\n", __FUNCTION__,
                  strerror(errno));
            sd.set(sd.get_target().c_str(), "");
            return;
         }
      }
      g_debug("%s: file transfer done!\n", __FUNCTION__);
   }

   g_debug("%s: providing file list [%s]\n", __FUNCTION__,
         mHGCopiedUriList.c_str());

   sd.set(sd.get_target().c_str(), mHGCopiedUriList.c_str());
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalGetTextOrRTFRequestCB --
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
CopyPasteUIX11::LocalGetTextOrRTFRequestCB(Gtk::SelectionData& sd, // IN/OUT
                                           guint info)             // Ignored
{
   sd.set(sd.get_target().c_str(), "");

   if (!mCP->IsCopyPasteAllowed()) {
      return;
   }

   const utf::string target = sd.get_target().c_str();

   g_debug("%s: Got paste request, target is %s\n",
         __FUNCTION__, target.c_str());

   if (target == TARGET_NAME_APPLICATION_RTF ||
       target == TARGET_NAME_TEXT_RICHTEXT ||
       target == TARGET_NAME_TEXT_RTF) {
      if (0 == mHGRTFData.size()) {
         g_debug("%s: Can not get valid RTF data\n", __FUNCTION__);
         return;
      }

      g_debug("%s: providing RTF data, size %" FMTSZ "u\n",
            __FUNCTION__, mHGRTFData.size());

      sd.set(target.c_str(), mHGRTFData.c_str());
   }

   if (target == TARGET_NAME_STRING ||
       target == TARGET_NAME_TEXT_PLAIN ||
       target == TARGET_NAME_UTF8_STRING ||
       target == TARGET_NAME_COMPOUND_TEXT) {
      if (0 == mHGTextData.bytes()) {
         g_debug("%s: Can not get valid text data\n", __FUNCTION__);
         return;
      }
      g_debug("%s: providing plain text, size %" FMTSZ "u\n",
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
CopyPasteUIX11::LocalClearClipboardCB(void)
{
   g_debug("%s: got clear callback\n", __FUNCTION__);
   mIsClipboardOwner = FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalClipboardTimestampCB --
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
CopyPasteUIX11::LocalClipboardTimestampCB(const Gtk::SelectionData& sd)  // IN
{
   int length = sd.get_length();

   /*
    * See “A Word on Selection Timestamps” above.
    */
   if (   (   sd.get_data_type().compare("INTEGER") == 0
           || sd.get_data_type().compare("TIMESTAMP") == 0)
       && sd.get_format() == 32
       && length >= 4 /* sizeof uint32 */) {
      mClipTime = reinterpret_cast<const uint32*>(sd.get_data())[0];
   } else {
      g_debug("%s: Unable to get mClipTime (sd: len %d, type %s, fmt %d).",
              __FUNCTION__, length,
              length >= 0 ? sd.get_data_type().c_str() : "(n/a)",
              sd.get_format());
   }

   Glib::RefPtr<Gtk::Clipboard> refClipboard
      = Gtk::Clipboard::get(GDK_SELECTION_PRIMARY);
   refClipboard->request_contents(TARGET_NAME_TIMESTAMP,
      sigc::mem_fun(this, &CopyPasteUIX11::LocalPrimTimestampCB));
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalPrimTimestampCB --
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
CopyPasteUIX11::LocalPrimTimestampCB(const Gtk::SelectionData& sd)  // IN
{
   int length = sd.get_length();

   /*
    * See “A Word on Selection Timestamps” above.
    */
   if (   (   sd.get_data_type().compare("INTEGER") == 0
           || sd.get_data_type().compare("TIMESTAMP") == 0)
       && sd.get_format() == 32
       && length >= 4 /* sizeof uint32 */) {
      mPrimTime = reinterpret_cast<const uint32*>(sd.get_data())[0];
   } else {
      g_debug("%s: Unable to get mPrimTime (sd: len %d, type %s, fmt %d).",
              __FUNCTION__, length,
              length >= 0 ? sd.get_data_type().c_str() : "(n/a)",
              sd.get_format());
   }

   if (mGetTimestampOnly) {
      mLastTimestamp = mClipTime > mPrimTime ? mClipTime : mPrimTime;
      return;
   }

   /* After got both timestamp, choose latest one as active selection. */
   if (mClipTime > mPrimTime) {
      mGHSelection = GDK_SELECTION_CLIPBOARD;
      if (mClipTime > 0 && mClipTime == mLastTimestamp) {
         g_debug("%s: clip is not changed\n", __FUNCTION__);
         SendClipNotChanged();
         return;
      }
      mLastTimestamp = mClipTime;
   } else {
      mGHSelection = GDK_SELECTION_PRIMARY;
      if (mPrimTime > 0 && mPrimTime == mLastTimestamp) {
         g_debug("%s: clip is not changed\n", __FUNCTION__);
         SendClipNotChanged();
         return;
      }
      mLastTimestamp = mPrimTime;
   }

   Glib::RefPtr<Gtk::Clipboard> refClipboard;
   bool flipped = false;
again:
   bool validDataInClip = false;
   refClipboard = Gtk::Clipboard::get(mGHSelection);

   g_debug("%s: trying %s selection.\n", __FUNCTION__,
          mGHSelection == GDK_SELECTION_PRIMARY ? "Primary" : "Clip");

   CPClipboard_Clear(&mClipboard);

   /* First check for URIs. This must always be done first */
   bool haveURIs = false;
   std::string format;
   if (mCP->CheckCapability(DND_CP_CAP_FILE_CP) && refClipboard->wait_is_target_available(FCP_TARGET_NAME_GNOME_COPIED_FILES)) {
      format = FCP_TARGET_NAME_GNOME_COPIED_FILES;
      haveURIs = true;
   } else if (mCP->CheckCapability(DND_CP_CAP_FILE_CP) && refClipboard->wait_is_target_available(FCP_TARGET_NAME_URI_LIST)) {
      format = FCP_TARGET_NAME_URI_LIST;
      haveURIs = true;
   }

   if (haveURIs) {
      refClipboard->request_contents(format,
                                     sigc::mem_fun(this,
                                                   &CopyPasteUIX11::LocalReceivedFileListCB));
      return;
   }

   /* Try to get image data from clipboard. */
   Glib::RefPtr<Gdk::Pixbuf> img = refClipboard->wait_for_image();
   gsize bufSize;
   if (mCP->CheckCapability(DND_CP_CAP_IMAGE_CP) && img) {
      gchar *buf = NULL;

      img->save_to_buffer(buf, bufSize, Glib::ustring("png"));
      if (bufSize > 0  &&
          bufSize <= (int)CPCLIPITEM_MAX_SIZE_V3 &&
          CPClipboard_SetItem(&mClipboard, CPFORMAT_IMG_PNG,
                              buf, bufSize)) {
         validDataInClip = true;
         g_debug("%s: Got PNG: %" FMTSZ "u\n", __FUNCTION__, bufSize);
      } else {
         g_debug("%s: Failed to get PNG\n", __FUNCTION__);
      }
      g_free(buf);
   }

   /* Try to get RTF data from clipboard. */
   bool haveRTF = false;
   if (refClipboard->wait_is_target_available(TARGET_NAME_APPLICATION_RTF)) {
      g_debug("%s: APP RTF is available\n", __FUNCTION__);
      format = TARGET_NAME_APPLICATION_RTF;
      haveRTF = true;
   }
   if (refClipboard->wait_is_target_available(TARGET_NAME_TEXT_RICHTEXT)) {
      g_debug("%s: RICHTEXT is available\n", __FUNCTION__);
      format = TARGET_NAME_TEXT_RICHTEXT;
      haveRTF = true;
   }
   if (refClipboard->wait_is_target_available(TARGET_NAME_TEXT_RTF)) {
      g_debug("%s: TEXT_RTF is available\n", __FUNCTION__);
      format = TARGET_NAME_TEXT_RTF;
      haveRTF = true;
   }

   if (mCP->CheckCapability(DND_CP_CAP_RTF_CP) && haveRTF) {
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
         g_debug("%s: Got RTF\n", __FUNCTION__);
      } else {
         g_debug("%s: Failed to get RTF size %d max %d\n",
               __FUNCTION__, (int) bufSize, (int)CPCLIPITEM_MAX_SIZE_V3);
      }
   }

   /* Try to get Text data from clipboard. */
   if (mCP->CheckCapability(DND_CP_CAP_PLAIN_TEXT_CP) &&
       refClipboard->wait_is_text_available()) {
      g_debug("%s: ask for text\n", __FUNCTION__);
      Glib::ustring str = refClipboard->wait_for_text();
      bufSize = str.bytes();
      if (bufSize > 0  &&
          bufSize <= (int)CPCLIPITEM_MAX_SIZE_V3 &&
          CPClipboard_SetItem(&mClipboard, CPFORMAT_TEXT,
                              (const void *)str.data(), bufSize + 1)) {
         validDataInClip = true;
         g_debug("%s: Got TEXT: %" FMTSZ "u\n", __FUNCTION__, bufSize);
      } else {
         g_debug("%s: Failed to get TEXT\n", __FUNCTION__);
      }
   }

   if (validDataInClip) {
      /*
       * RTF or text data (or both) in the clipboard.
       */
      mCP->DestUISendClip(&mClipboard);
   } else if (!flipped) {
      /*
       * If we get here, we got nothing (no image, URI, text) so
       * try the other selection.
       */
      g_debug("%s: got nothing for this selection, try the other.\n",
            __FUNCTION__);
      mGHSelection = mGHSelection == GDK_SELECTION_PRIMARY ?
                     GDK_SELECTION_CLIPBOARD : GDK_SELECTION_PRIMARY;
      flipped = true;
      goto again;
   } else {
      g_debug("%s: got nothing, send empty clip back.\n",
            __FUNCTION__);
      mCP->DestUISendClip(&mClipboard);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalReceivedFileListCB --
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
CopyPasteUIX11::LocalReceivedFileListCB(const Gtk::SelectionData& sd)        // IN
{
   g_debug("%s: enter", __FUNCTION__);
   const utf::string target = sd.get_target().c_str();

   if (!mCP->CheckCapability(DND_CP_CAP_FILE_CP)) {
      /*
       * Disallowed based on caps settings, return.
       */
      return;
   }
   if (target == FCP_TARGET_NAME_GNOME_COPIED_FILES ||
       target == FCP_TARGET_NAME_URI_LIST) {
      LocalGetSelectionFileList(sd);
      mCP->DestUISendClip(&mClipboard);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalGetFileContentsRequestCB --
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
CopyPasteUIX11::LocalGetFileContentsRequestCB(Gtk::SelectionData& sd, // IN
                                              guint info)             // IN
{
   std::vector<utf::string>::const_iterator iter;
   utf::string uriList = "";
   utf::string pre;
   utf::string post;

   if (!mCP->CheckCapability(DND_CP_CAP_FILE_CONTENT_CP)) {
      /*
       * Disallowed based on caps settings, return.
       */
      return;
   }

   sd.set(sd.get_target().c_str(), "");

   /* Provide URIs for each path in the guest's file list. */
   if (FCP_TARGET_INFO_GNOME_COPIED_FILES == info) {
      uriList = "copy\n";
      pre = FCP_GNOME_LIST_PRE;
      post = FCP_GNOME_LIST_POST;
   } else if (FCP_TARGET_INFO_URI_LIST == info) {
      pre = DND_URI_LIST_PRE_KDE;
      post = DND_URI_LIST_POST;
   } else if (FCP_TARGET_INFO_NAUTILUS_FILES == info) {
      uriList = utf::string(FCP_TARGET_MIME_NAUTILUS_FILES) + "\ncopy\n";
      pre = FCP_GNOME_LIST_PRE;
      post = FCP_GNOME_LIST_POST;
   } else {
      g_debug("%s: Unknown request target: %s\n",
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
      g_debug("%s: Can not get uri list\n", __FUNCTION__);
      return;
   }

   g_debug("%s: providing file list [%s]\n", __FUNCTION__, uriList.c_str());

   sd.set(sd.get_target().c_str(), uriList.c_str());
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalGetSelectionFileList --
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
CopyPasteUIX11::LocalGetSelectionFileList(const Gtk::SelectionData& sd)      // IN
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
   g_debug("%s: Got file list: [%s]\n", __FUNCTION__, source.c_str());

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
#if defined(__linux__)
      if (DnD_UriIsNonFileSchemes(newPath)) {
         /* Try to get local file path for non file uri. */
         GFile *file = g_file_new_for_uri(newPath);
         free(newPath);
         if (!file) {
            g_debug("%s: g_file_new_for_uri failed\n", __FUNCTION__);
            return;
         }
         newPath = g_file_get_path(file);
         g_object_unref(file);
         if (!newPath) {
            g_debug("%s: g_file_get_path failed\n", __FUNCTION__);
            return;
         }
      }
#endif

      /*
       * Parse relative path.
       */
      newRelPath = Str_Strrchr(newPath, DIRSEPC) + 1; // Point to char after '/'

      /* Keep track of how big the fcp files are. */
      if ((size = File_GetSizeEx(newPath)) >= 0) {
         totalSize += size;
      } else {
         g_debug("%s: Unable to get file size for %s\n", __FUNCTION__, newPath);
      }

      g_debug("%s: Adding newPath '%s' newRelPath '%s'\n", __FUNCTION__,
            newPath, newRelPath);
      fileList.AddFile(newPath, newRelPath);
      free(newPath);
   }

   DynBuf_Init(&buf);
   fileList.SetFileSize(totalSize);
   g_debug("%s: totalSize is %" FMT64 "u\n", __FUNCTION__, totalSize);
   fileList.ToCPClipboard(&buf, false);
   CPClipboard_SetItem(&mClipboard, CPFORMAT_FILELIST, DynBuf_Get(&buf),
                       DynBuf_GetSize(&buf));
   DynBuf_Destroy(&buf);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::GetLastDirName --
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
CopyPasteUIX11::GetLastDirName(const utf::string &str)     // IN
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
 * CopyPasteUIX11::GetNextPath --
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
CopyPasteUIX11::GetNextPath(utf::utf8string& str, // IN: NUL-delimited path list
                            size_t& index)        // IN/OUT: current index into string
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
 * CopyPasteUIX11::GetRemoteClipboardCB --
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
CopyPasteUIX11::GetRemoteClipboardCB(const CPClipboard *clip) // IN
{
   Glib::RefPtr<Gtk::Clipboard> refClipboard =
      Gtk::Clipboard::get(GDK_SELECTION_CLIPBOARD);
   Glib::RefPtr<Gtk::Clipboard> refPrimary =
      Gtk::Clipboard::get(GDK_SELECTION_PRIMARY);
   void *buf;
   size_t sz;

   TRACE_CALL();
   if (!clip) {
      g_debug("%s: No clipboard contents.", __FUNCTION__);
      return;
   }

   if (mBlockAdded) {
      mBlockAdded = false;
      if (DnD_BlockIsReady(mBlockCtrl)) {
         mBlockCtrl->RemoveBlock(mBlockCtrl->fd, mHGStagingDir.c_str());
      }
   }

   /* Clear the clipboard contents if we are the owner. */
   if (mIsClipboardOwner) {
      refClipboard->clear();
      refPrimary->clear();
      mIsClipboardOwner = false;
      g_debug("%s: Cleared local clipboard", __FUNCTION__);
   }

   mHGTextData.clear();
   mHGRTFData.clear();
   mHGFCPData.clear();

   if (CPClipboard_ItemExists(clip, CPFORMAT_TEXT) ||
       CPClipboard_ItemExists(clip, CPFORMAT_RTF)) {
      std::vector<Gtk::TargetEntry> targets;

      /*
       * rtf should be first in the target list otherwise OpenOffice may not
       * accept paste.
       */
      if (CPClipboard_GetItem(clip, CPFORMAT_RTF, &buf, &sz)) {
         g_debug("%s: RTF data, size %" FMTSZ "u.\n", __FUNCTION__, sz);
         Gtk::TargetEntry appRtf(TARGET_NAME_APPLICATION_RTF);
         Gtk::TargetEntry textRichText(TARGET_NAME_TEXT_RICHTEXT);
         Gtk::TargetEntry textRtf(TARGET_NAME_TEXT_RTF);

         targets.push_back(appRtf);
         targets.push_back(textRichText);
         targets.push_back(textRtf);
         mHGRTFData = std::string((const char *)buf);
         mIsClipboardOwner = true;
      }

      if (CPClipboard_GetItem(clip, CPFORMAT_TEXT, &buf, &sz)) {
         Gtk::TargetEntry stringText(TARGET_NAME_STRING);
         Gtk::TargetEntry plainText(TARGET_NAME_TEXT_PLAIN);
         Gtk::TargetEntry utf8Text(TARGET_NAME_UTF8_STRING);
         Gtk::TargetEntry compountText(TARGET_NAME_COMPOUND_TEXT);

         g_debug("%s: Text data, size %" FMTSZ "u.\n", __FUNCTION__, sz);
         targets.push_back(stringText);
         targets.push_back(plainText);
         targets.push_back(utf8Text);
         targets.push_back(compountText);
         mHGTextData = utf::string(reinterpret_cast<char *>(buf),
                                   STRING_ENCODING_UTF8);
         mIsClipboardOwner = true;
      }

      refClipboard->set(targets,
                        sigc::mem_fun(this, &CopyPasteUIX11::LocalGetTextOrRTFRequestCB),
                        sigc::mem_fun(this, &CopyPasteUIX11::LocalClearClipboardCB));
      refPrimary->set(targets,
                      sigc::mem_fun(this, &CopyPasteUIX11::LocalGetTextOrRTFRequestCB),
                      sigc::mem_fun(this, &CopyPasteUIX11::LocalClearClipboardCB));
      return;
   }

   if (CPClipboard_GetItem(clip, CPFORMAT_IMG_PNG, &buf, &sz)) {
      g_debug("%s: PNG data, size %" FMTSZ "u.\n", __FUNCTION__, sz);
      /* Try to load buf into pixbuf, and write to local clipboard. */
      try {
         Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create();
         loader->write((const guint8 *)buf, sz);
         loader->close();

         refClipboard->set_image(loader->get_pixbuf());
         refPrimary->set_image(loader->get_pixbuf());

         /*
          * Record current clipboard timestamp to prevent unexpected clipboard
          * exchange.
          *
          * XXX We should do this for all formats.
          */
         mClipTime = 0;
         mPrimTime = 0;
         mGetTimestampOnly = true;
         refClipboard->request_contents(TARGET_NAME_TIMESTAMP,
            sigc::mem_fun(this, &CopyPasteUIX11::LocalClipboardTimestampCB));
      } catch (const Gdk::PixbufError& e) {
         g_message("%s: caught Gdk::PixbufError %s\n", __FUNCTION__, e.what().c_str());
      } catch (std::exception& e) {
         g_message("%s: caught std::exception %s\n", __FUNCTION__, e.what());
      } catch (...) {
         g_message("%s: caught unknown exception (typename %s)\n", __FUNCTION__,
                   __cxxabiv1::__cxa_current_exception_type()->name());
      }
      return;
   }

   if (CPClipboard_GetItem(clip, CPFORMAT_FILELIST, &buf, &sz)) {
      g_debug("%s: File data.\n", __FUNCTION__);
      DnDFileList flist;
      flist.FromCPClipboard(buf, sz);
      mTotalFileSize = flist.GetFileSize();
      mHGFCPData = flist.GetRelPathsStr();

      refClipboard->set(mListTargets,
                        sigc::mem_fun(this, &CopyPasteUIX11::LocalGetFileRequestCB),
                        sigc::mem_fun(this, &CopyPasteUIX11::LocalClearClipboardCB));
      refPrimary->set(mListTargets,
                      sigc::mem_fun(this, &CopyPasteUIX11::LocalGetFileRequestCB),
                      sigc::mem_fun(this, &CopyPasteUIX11::LocalClearClipboardCB));

      mIsClipboardOwner = true;
      mHGGetListTime = GetCurrentTime();
      mHGGetFileStatus = DND_FILE_TRANSFER_NOT_STARTED;
      mHGCopiedUriList = "";
   }

   if (CPClipboard_ItemExists(clip, CPFORMAT_FILECONTENTS)) {
      g_debug("%s: File contents data\n", __FUNCTION__);
      if (LocalPrepareFileContents(clip)) {
         refClipboard->set(mListTargets,
                           sigc::mem_fun(this, &CopyPasteUIX11::LocalGetFileContentsRequestCB),
                           sigc::mem_fun(this, &CopyPasteUIX11::LocalClearClipboardCB));
         refPrimary->set(mListTargets,
                         sigc::mem_fun(this, &CopyPasteUIX11::LocalGetFileContentsRequestCB),
                         sigc::mem_fun(this, &CopyPasteUIX11::LocalClearClipboardCB));
         mIsClipboardOwner = true;
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalPrepareFileContents --
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
CopyPasteUIX11::LocalPrepareFileContents(const CPClipboard *clip) // IN
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

   if (!CPClipboard_GetItem(clip, CPFORMAT_FILECONTENTS, &buf, &sz)) {
      g_debug("%s: CPClipboard_GetItem failed\n", __FUNCTION__);
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

      if (fileItem[i].validFlags & CP_FILE_VALID_TYPE &&
          CP_FILE_TYPE_DIRECTORY == fileItem[i].type) {
         if (!File_CreateDirectory(filePathName.c_str())) {
            goto exit;
         }
         g_debug("%s: created directory [%s].\n",
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
         g_debug("%s: File_SetTimes failed with file [%s].\n",
               __FUNCTION__, filePathName.c_str());
      }

      /* Update file permission attributes. */
      if (fileItem->validFlags & CP_FILE_VALID_PERMS) {
         if (Posix_Chmod(filePathName.c_str(),
                         fileItem->permissions) < 0) {
            /* Not a critical error, only log it. */
            g_debug("%s: Posix_Chmod failed with file [%s].\n",
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
   g_debug("%s: created uri list\n", __FUNCTION__);
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
 * CopyPasteUIX11::GetLocalFilesDone --
 *
 *    Callback when CopyPasteUIX11::GetLocalFiles is done, which finishes the file
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
CopyPasteUIX11::GetLocalFilesDone(bool success)
{
   g_debug("%s: enter success %d\n", __FUNCTION__, success);

   if (mBlockAdded) {
      g_debug("%s: removing block for %s\n", __FUNCTION__, mHGStagingDir.c_str());
      /* We need to make sure block subsystem has not been shut off. */
      mBlockAdded = false;
      if (DnD_BlockIsReady(mBlockCtrl)) {
         mBlockCtrl->RemoveBlock(mBlockCtrl->fd, mHGStagingDir.c_str());
      }
   }

   mHGGetFileStatus = DND_FILE_TRANSFER_FINISHED;
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
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::SendClipNotChanged --
 *
 *    Send a not-changed clip to host.
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
CopyPasteUIX11::SendClipNotChanged(void)
{
   CPClipboard clip;

   g_debug("%s: enter.\n", __FUNCTION__);
   CPClipboard_Init(&clip);
   CPClipboard_SetChanged(&clip, FALSE);
   mCP->DestUISendClip(&clip);
   CPClipboard_Destroy(&clip);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::Reset --
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
CopyPasteUIX11::Reset(void)
{
   TRACE_CALL();
   /* Cancel any pending file transfer. */
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::FileBlockMonitorThread --
 *
 *    This thread monitors the access to the files in the clipboard which owns
 *    by VMTools by using the notification mechanism of VMBlock.
 *    If any access to the file is detected, then this thread requests the file
 *    transfer from host to guest.
 *
 * Results:
 *    Always return NULL
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void*
CopyPasteUIX11::FileBlockMonitorThread(void *arg)   // IN
{
   TRACE_CALL();
   ThreadParams *params = (ThreadParams *)arg;
   pthread_mutex_lock(&params->fileBlockMutex);
   while (true) {
      g_debug("%s: waiting signal\n", __FUNCTION__);
      pthread_cond_wait(&params->fileBlockCond, &params->fileBlockMutex);
      g_debug("%s: received signal. Exit:%d\n",
              __FUNCTION__,
              params->fileBlockCondExit);
      if (params->fileBlockCondExit) {
         break;
      }
      if (params->fileBlockName.bytes() == 0) {
        continue;
      }

      int fd = open(params->fileBlockName.c_str(), O_RDONLY);
      if (fd < 0) {
         g_debug("%s: Failed to open %s, errno is %d\n",
                 __FUNCTION__,
                 params->fileBlockName.c_str(),
                 errno);
         continue;
      }

      char buf[sizeof(VMBLOCK_FUSE_READ_RESPONSE)];
      ssize_t size;
      size = read(fd, buf, sizeof(VMBLOCK_FUSE_READ_RESPONSE));
      /*
       * The current thread will block in read function until
       * any other application accesses the file params->fileBlockName
       * or the block on the file params->fileBlockName is removed.
       * Currently we don't need to check the response in buf, so
       * just ignore it.
       */

      if (params->cp->IsBlockAdded()) {
         g_debug("%s: Request files\n", __FUNCTION__);
         params->cp->RequestFiles();
      } else {
         g_debug("%s: Block is not added\n", __FUNCTION__);
      }

      if (close(fd) < 0) {
         g_debug("%s: Failed to close %s, errno is %d\n",
                 __FUNCTION__,
                 params->fileBlockName.c_str(),
                 errno);
      }
   }
   pthread_mutex_unlock(&params->fileBlockMutex);
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::TerminateThread --
 *
 *    This is called when the monitor thread is asked to exit.
 *    Sets the global state and signals the monitor thread to exit and wait
 *    for the monitor thread to exit.
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
CopyPasteUIX11::TerminateThread()
{
   TRACE_CALL();
   if (mThread == 0) {
      return;
   }

   pthread_mutex_lock(&mThreadParams.fileBlockMutex);
   mThreadParams.fileBlockCondExit = true;
   pthread_cond_signal(&mThreadParams.fileBlockCond);
   pthread_mutex_unlock(&mThreadParams.fileBlockMutex);

   pthread_join(mThread, NULL);

   mThread = 0;
}
