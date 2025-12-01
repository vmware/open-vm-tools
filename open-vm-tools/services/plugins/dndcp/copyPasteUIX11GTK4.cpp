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

/*
 * copyPasteUIX11GTK4.cpp --
 *
 *    Like copyPasteUIX11.c, this is the GTK4 version implementation
 *    of CopyPaste between host and guest for Linux GuestOS. GTK4 API's
 *    are completely different than GTK3 hence a seperate implementation
 *    is needed.
 *
 *    Currently there are 2 versions for copy/paste.
 *    Version 1 is based on backdoor,  supports only text copy/paste.
 *    Version 4 is based on guestRPC,  supports text, image, rtf and
 *    file copy/paste.
 *
 *    G->H Copy/Paste (version 4)
 *    --------------------
 *    When Ungrab, GetLocalClipboard got called, which fetches the
 *    text, image, rtf or file data from the Clipboard and send it to Host.
 *
 *    H->G Copy/Paste (version 4)
 *    --------------------
 *    When grab, GetRemoteClipboardCB got called, which sets the text,
 *    image, rtf or file data received from the Host to the Linux Guest
 *    OS clipboard
 *
 */


#ifndef GTK4
#error "This should build with GTK4 only."
#endif

#define G_LOG_DOMAIN "dndcp"

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
#include "dndPluginIntX11.h"
#include "copyPasteDnDUtil.h"
#include <gtk/gtk.h>
#include <sys/time.h>

extern "C" {
   #include "dndClipboard.h"
   #include "cpName.h"
   #include "cpNameUtil.h"
   #include "rpcout.h"
   #include "vmware/guestrpc/tclodefs.h"
}

static unsigned int gGuestClipGenNum;
static unsigned int gHostClipGenNum;


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
   mClipTime(0),
   mPrimTime(0),
   mLastTimestamp(0),
   mThread(0),
   mHGGetListTime(0),
   mHGGetFileStatus(DND_FILE_TRANSFER_NOT_STARTED),
   mBlockAdded(false),
   mBlockCtrl(0),
   mInited(false),
   mDftClipboardPtr(nullptr),
   mPrimClipboardPtr(nullptr),
   mTotalFileSize(0)
{
   TRACE_CALL();
   GuestDnDCPMgr *p = GuestDnDCPMgr::GetInstance();
   ASSERT(p);
   mCP = p->GetCopyPasteMgr();
   ASSERT(mCP);
   Glib::init();

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
 *    Always true
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

   gGuestClipGenNum = 0;
   gHostClipGenNum = 0;

   mCP->srcRecvClipChanged.connect(
      sigc::mem_fun(this, &CopyPasteUIX11::GetRemoteClipboardCB));
   mCP->destRequestClipChanged.connect(
      sigc::mem_fun(this, &CopyPasteUIX11::GetLocalClipboard));
   mCP->getFilesDoneChanged.connect(
      sigc::mem_fun(this, &CopyPasteUIX11::GetLocalFilesDone));


   Glib::RefPtr<Gdk::Display> display = Gdk::Display::get_default();
   mDftClipboardPtr = display->get_clipboard();
   mPrimClipboardPtr = display->get_primary_clipboard();

   mDftClipboardPtr->signal_changed().connect(
      sigc::mem_fun(*this, &CopyPasteUIX11::GuestDefaultClipboardChangedCb));
   mPrimClipboardPtr->signal_changed().connect(
      sigc::mem_fun(*this, &CopyPasteUIX11::GuestPrimaryClipboardChangedCb));

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
 * CopyPasteUIX11::GuestDefaultClipboardChangedCb --
 *
 *      Callback when the guest default clipboard changed.
 *      Either guest own app or the H-G copy could change the guest clipboard.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      gGuestCpGenNum is increased by 1.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUIX11::GuestDefaultClipboardChangedCb()
{
   ++gGuestClipGenNum;
   mClipTime = g_get_real_time();
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::GuestPrimaryClipboardChangedCb --
 *
 *      Callback when the guest primary clipboard changed.
 *      Either guest own app or the H-G copy could change the guest clipboard.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      gGuestCpGenNum is increased by 1.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUIX11::GuestPrimaryClipboardChangedCb()
{
   ++gGuestClipGenNum;
   mPrimTime = g_get_real_time();
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
 * CopyPasteUIX11::GetLocalClipboard --
 *
 *    Retrieves the data from local clipboard and sends it to host. Send empty
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
   Glib::RefPtr<Gdk::Clipboard> clipboardPtr;

   if (!mCP->IsCopyPasteAllowed()) {
      g_debug("%s: copyPaste is not allowed\n", __FUNCTION__);
      return;
   }

   if (gHostClipGenNum == gGuestClipGenNum) {
      g_debug("%s: Guest clipboard in sync with host with the same GenNum=%d, "
              "skip send", __FUNCTION__, gGuestClipGenNum);
      SendClipNotChanged();
      return;
   }

   CPClipboard_Clear(&mClipboard);

   if (mClipTime > mPrimTime) {
      if (mClipTime > 0 && mClipTime == mLastTimestamp) {
         g_debug("%s: clip is not changed\n", __FUNCTION__);
         SendClipNotChanged();
         return;
      }
      mLastTimestamp = mClipTime;
      clipboardPtr = mDftClipboardPtr;
   } else {
      if (mPrimTime > 0 && mPrimTime == mLastTimestamp) {
         g_debug("%s: clip is not changed\n", __FUNCTION__);
         SendClipNotChanged();
         return;
      }
      mLastTimestamp = mPrimTime;
      clipboardPtr = mPrimClipboardPtr;
   }

   Glib::RefPtr<Gdk::ContentFormats> formats = clipboardPtr->get_formats();

   /*
    * *** DEBUG ONLY log for dev to track clipboard mime formats ***
    * TODO: Remove the below before turn FSS on
    */
   Glib::ustring clipFmtStr = formats->to_string();
   const std::vector<Glib::ustring> clipFmtVector = formats->get_mime_types();

   g_debug("%s: Guest clipboard formats all:'/%s/', ",  __FUNCTION__, clipFmtStr.c_str());
   for (Glib::ustring fmt : clipFmtVector) {
      g_debug("%s: Guest clipboard format list:'/%s/', ",  __FUNCTION__, fmt.c_str());

   }
   /* TODO: Remove the above before Turn FSS on */

   /* Try to get URI's from clipboard if present. This must always be done first */
   std::vector<Glib::ustring> fmtVec;
   bool haveURIs = false;

   if (formats->contain_mime_type(FCP_TARGET_NAME_GNOME_COPIED_FILES)) {
      g_debug("%s: Gnome URI list is available in guest clipboard", __FUNCTION__);
      fmtVec.push_back(FCP_TARGET_NAME_GNOME_COPIED_FILES);
      haveURIs = true;
   }
   if (formats->contain_mime_type(FCP_TARGET_NAME_URI_LIST)) {
      g_debug("%s: text uri list is available in guest clipboard", __FUNCTION__);
      fmtVec.push_back(FCP_TARGET_NAME_URI_LIST);
      haveURIs = true;
   }
   if (mCP->CheckCapability(DND_CP_CAP_FILE_CP) && haveURIs) {
      g_debug("%s: Getting file URI from guest clipboard",  __FUNCTION__);
      clipboardPtr->read_async(fmtVec, G_PRIORITY_DEFAULT, sigc::bind(sigc::mem_fun(
         *this, &CopyPasteUIX11::CopyPasteRequestStreamCb), clipboardPtr), nullptr);
      return;
   }

#if (GTK_MAJOR_VERSION == 4 && GTK_MINOR_VERSION >= 6)
   /* Image Copy/Paste from G->H is supported from GTK 4.6 and onwards.
    * The dependency is due to usage of gdk_texture_save_to_png_bytes().
    * This check can be removed once we move the Host code to
    * GTK4 as well. GDKTexture is supported from GTK4 and we can
    * directly set it to clipboard for Image.
    */

   /* Try to get Image data from clipboard. */
   if (mCP->CheckCapability(DND_CP_CAP_IMAGE_CP) &&
       formats->contain_gtype(GDK_TYPE_TEXTURE)) {
      clipboardPtr->read_texture_async(sigc::bind(sigc::mem_fun(
         *this, &CopyPasteUIX11::CopyPasteRequestImageCb), clipboardPtr), nullptr);
   }
#endif

   /* Try to get rtf data from clipboard if present, then check for plain text */
   bool haveRtf = false;
   fmtVec.clear();

   if (formats->contain_mime_type(TARGET_NAME_APPLICATION_RTF)) {
      g_debug("%s: APP RTF is available in guest clipboard", __FUNCTION__);
      fmtVec.push_back(TARGET_NAME_APPLICATION_RTF);
      haveRtf = true;
   }
   if (formats->contain_mime_type(TARGET_NAME_TEXT_RICHTEXT)) {
      g_debug("%s: RICHTEXT is available in guest clipboard",
              __FUNCTION__);
      fmtVec.push_back(TARGET_NAME_TEXT_RICHTEXT);
      haveRtf = true;
   }
   if (formats->contain_mime_type(TARGET_NAME_TEXT_RTF)) {
      g_debug("%s: TEXT RTF is available in guest clipboard", __FUNCTION__);
      fmtVec.push_back(TARGET_NAME_TEXT_RTF);
      haveRtf = true;
   }
   if (mCP->CheckCapability(DND_CP_CAP_RTF_CP) && haveRtf) {
      g_debug("%s: Getting rtf text from guest clipboard",  __FUNCTION__);
      clipboardPtr->read_async(fmtVec, G_PRIORITY_DEFAULT, sigc::bind(sigc::mem_fun(
	 *this, &CopyPasteUIX11::CopyPasteRequestStreamCb), clipboardPtr), nullptr);
   }

   /* Try to get plain text from clipboard if present */
   if (mCP->CheckCapability(DND_CP_CAP_PLAIN_TEXT_CP) &&
              formats->contain_gtype(G_TYPE_STRING)) {
      g_debug("%s: Getting plain text from guest clipboard",  __FUNCTION__);
      clipboardPtr->read_text_async(sigc::bind(sigc::mem_fun(
         *this, &CopyPasteUIX11::CopyPasteRequestTextCb), clipboardPtr), nullptr);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 *  CopyPasteUIX11::CopyPasteSendText--
 *
 *      Send the text data to Host.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *-----------------------------------------------------------------------------
 */
void
CopyPasteUIX11::CopyPasteSendText(const Glib::ustring& cpStr)   // IN
{
   if (cpStr.empty()) {
      return;
   }

   size_t bufSize = cpStr.size();
   if (bufSize > 0  &&
       bufSize <= CPCLIPITEM_MAX_SIZE_V3 &&
       CPClipboard_SetItem(&mClipboard, CPFORMAT_TEXT,
                           cpStr.c_str(), bufSize + 1)) {
      g_debug("%s: set TEXT: %" FMTSZ "u\n", __FUNCTION__, bufSize);
      mCP->DestUISendClip(&mClipboard);
      // Reset the host and guest clip gen number.
      gHostClipGenNum = 0;
      gGuestClipGenNum = 0;
   } else {
      /* clipboard failure is not critical one,  warn but do not error exit */
      g_warning("%s: Failed to copy text from guest clipboard,"
                " either text is empty or too large\n", __FUNCTION__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 *  CopyPasteSendImage--
 *
 *      Send the Image data to Host
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUIX11::CopyPasteSendImage(const Glib::RefPtr<Glib::Bytes>& imgData)   // IN
{
   if (imgData == nullptr) {
      return;
   }

   gsize bufSize = imgData->get_size();
   const void *buf = imgData->get_data(bufSize);
   g_debug("%s: Got PNG byte: %" FMTSZ "u\n", __FUNCTION__, bufSize);

   if (bufSize > 0  &&
       bufSize <= CPCLIPITEM_MAX_SIZE_V3 &&
       CPClipboard_SetItem(&mClipboard, CPFORMAT_IMG_PNG,
                           buf, bufSize)) {
      mCP->DestUISendClip(&mClipboard);
      // Reset the host and guest clip gen number.
      gHostClipGenNum = 0;
      gGuestClipGenNum = 0;
   } else {
      /* clipboard failure is not critical one,  warn but do not error exit */
      g_warning("%s: Failed to copy image from guest clipboard,"
                " either image is empty or too large\n", __FUNCTION__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 *  CopyPasteUIX11::CopyPasteRequestTextCb--
 *
 *      This Callback read's the guest's text data from clipboard. If succeeded,
 *      the data is sent to Host.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUIX11::CopyPasteRequestTextCb(const Glib::RefPtr<Gio::AsyncResult>& result,    // IN
                                       const Glib::RefPtr<Gdk::Clipboard>& clipboard)   // IN
{
   try {
      /* gdk_clipboard_read_text_finish return NUL terminated UTF-8 string */
      Glib::ustring cpStr = clipboard->read_text_finish(result);
      g_debug("%s: Clip is [%s]\n", __FUNCTION__, cpStr.c_str());
      CopyPasteSendText(cpStr);
   } catch (const Glib::Error& e) {
      /* clipboard failure is not critical one,  warn but do not error exit */
      g_warning("%s: Error to copy from guest: %s\n", __FUNCTION__, e.what());
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 *  CopyPasteUIX11::CopyPasteRequestImageCb--
 *
 *      This Callback read's the guest's image data from clipboard. If succeeded,
 *      the data is sent to Host.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUIX11::CopyPasteRequestImageCb(const Glib::RefPtr<Gio::AsyncResult>& result,    // IN
                                        const Glib::RefPtr<Gdk::Clipboard>& clipboard)   // IN
{
   try {
      Glib::RefPtr<Gdk::Texture> image = clipboard->read_texture_finish(result);
      if (image) {
         Glib::RefPtr<Glib::Bytes> imgData = image->save_to_png_bytes();
         CopyPasteSendImage(imgData);
      }
   } catch (const Glib::Error& e) {
      /* clipboard failure is not critical one,  warn but do not error exit */
      g_warning("%s: Error to copy from guest: %s\n", __FUNCTION__, e.what());
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 *  CopyPasteSendRtfValue--
 *
 *      Get GBytes from the Gio::InputStream, and sent the buffer to host.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Host clipboard is changed.
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUIX11::CopyPasteSendRtfValue(const Glib::RefPtr<Gio::AsyncResult>& result)   // IN
{
   Glib::RefPtr<Glib::Bytes> rtfData = mCpStream->read_bytes_finish(result);
   size_t readSize = rtfData->get_size();

   g_debug("%s: Got Glib:Bytes from clipboard with size=%" FMTSZ "u\n",
           __FUNCTION__, readSize);

   if (readSize > 0) {
      const void *buf = rtfData->get_data(readSize);

      if (readSize > CPCLIPITEM_MAX_SIZE_V3) {
         g_warning("%s: Failed to copy rtf from guest clipboard,"
                   " as rtf data is too large\n, sizeLimit=%" FMTSZ "u, actualSize=%" FMTSZ "u",
                   __FUNCTION__, CPCLIPITEM_MAX_SIZE_V3, readSize);
         return;
      }
      if (CPClipboard_SetItem(&mClipboard, CPFORMAT_RTF,
                              buf, readSize)) {
         mCP->DestUISendClip(&mClipboard);
         // Reset the host and guest clip gen number.
         gHostClipGenNum = 0;
         gGuestClipGenNum = 0;
      } else {
         /* clipboard failure is not critical one,  warn but do not error exit */
         g_warning("%s: Failed to set rtf data to dest buffer from guest clipboard", __FUNCTION__);
      }
   } else {
      g_warning("%s: Failed to copy rtf from guest clipboard,"
               " as rtf data is empty\n", __FUNCTION__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::CopyPasteSendFile --
 *
 *      Construct local file list and remote file list and send it to Host.
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
CopyPasteUIX11::CopyPasteSendFile(const Glib::RefPtr<Gio::AsyncResult>& result)      // IN
{
   char *newPath;
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
   Glib::RefPtr<Glib::Bytes> fileUriData = mCpStream->read_bytes_finish(result);
   gsize readSize = fileUriData->get_size();

   g_debug("%s: Got Glib:Bytes from clipboard with size=%" FMTSZ "u\n",
           __FUNCTION__, readSize);

   if (readSize <= 0) {
      g_warning("%s: Failed to copy file from guest clipboard,"
                " as file uri data is empty or not valid.\n", __FUNCTION__);
      return;
   }

   const char *data = static_cast<const char*>(fileUriData->get_data(readSize));
   /* Converting data to utf::string directly is leading to buffer overflow.
    * Its because data is Byte data and not NULL terminated. utf::string
    * has no implementation for string creation with data size.
    * In order to avoid that, first converting it to std::string and then
    * converting to utf::string.
    */
   std::string str(data, readSize);
   utf::string source(str.c_str());
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
      char *newRelPath;

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
   bool validDataInClip = false;
   fileList.SetFileSize(totalSize);
   g_debug("%s: totalSize is %" FMT64 "u\n", __FUNCTION__, totalSize);
   fileList.ToCPClipboard(&buf, false);
   gsize bufSize = DynBuf_GetSize(&buf);
   if (bufSize > 0  &&
       bufSize <= (int)CPCLIPITEM_MAX_SIZE_V3 &&
       CPClipboard_SetItem(&mClipboard, CPFORMAT_FILELIST, DynBuf_Get(&buf),
                           bufSize)) {
      validDataInClip = true;
      g_debug("%s: Got File List. \n", __FUNCTION__);
      // Reset the host and guest clip gen number.
      gHostClipGenNum = 0;
      gGuestClipGenNum = 0;
   } else {
      g_debug("%s: Failed to get File List\n", __FUNCTION__);
   }

   DynBuf_Destroy(&buf);

   if (validDataInClip) {
      /*
       * File List in the clipboard.
       */
      mCP->DestUISendClip(&mClipboard);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 *  CopyPasteRequestStreamCb--
 *
 *      Get Gio::InputStream from clipboard, need to read from the resulting
 *      InputStream Async.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      class member mCpStream is modified.
 *-----------------------------------------------------------------------------
 */

 void
 CopyPasteUIX11::CopyPasteRequestStreamCb(const Glib::RefPtr<Gio::AsyncResult>& result,   // IN
                                          const Glib::RefPtr<Gdk::Clipboard>& clipboard)  // IN
 {
    Glib::ustring out_mime_type;
 
    /* Get the InputStream from clipboard */
    mCpStream = clipboard->read_finish(result, out_mime_type);
    g_debug("%s: Got rtf format [%s] from clipboard\n", __FUNCTION__, out_mime_type.c_str());
 
    /*
     * get GBytes from the stream and CopyPasteSendValue callback when finish.
     * Here, a max of CPCLIPITEM_MAX_SIZE_V3+1 size is set, if we really read that much
     * then we could error out after read_bytes_finish is called.
     */
    if (out_mime_type == TARGET_NAME_APPLICATION_RTF ||
        out_mime_type == TARGET_NAME_TEXT_RICHTEXT ||
	out_mime_type == TARGET_NAME_TEXT_RTF) {
        mCpStream->read_bytes_async(CPCLIPITEM_MAX_SIZE_V3 + 1,
           sigc::mem_fun(*this, &CopyPasteUIX11::CopyPasteSendRtfValue), G_PRIORITY_DEFAULT);
    } else if (out_mime_type == FCP_TARGET_NAME_GNOME_COPIED_FILES ||
	       out_mime_type == FCP_TARGET_NAME_URI_LIST) {
        mCpStream->read_bytes_async(CPCLIPITEM_MAX_SIZE_V3 + 1,
           sigc::mem_fun(*this, &CopyPasteUIX11::CopyPasteSendFile), G_PRIORITY_DEFAULT);
    }
 }


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalSetFileListToClipboard --
 *
 *      API to set the guest clipboard with file list.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      gHostClipGenNum is increased.
 *      Clipboard is set with File list.
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPasteUIX11::LocalSetFileListToClipboard(const char* desktop)   // IN
{
   std::vector<Glib::RefPtr<Gdk::ContentProvider>> provider;
   Glib::RefPtr<Gdk::ContentProvider> providerUnion;
   Glib::RefPtr<Glib::Bytes> fileListGnome;
   Glib::RefPtr<Glib::Bytes> fileListKde;
   Glib::RefPtr<Glib::Bytes> fileListNautilus;

   g_debug("%s: enter.\n", __FUNCTION__);
   g_debug("%s: Desktop = %s\n", __FUNCTION__, desktop);

   if (desktop && strstr(desktop, "GNOME")) {
      // Set the Gnome file list to GDKContentProvider
      fileListGnome = Glib::Bytes::create(mHGCopiedUriListGnome.c_str(), mHGCopiedUriListGnome.bytes());
      fileListNautilus = Glib::Bytes::create(mHGCopiedUriListNautilus.c_str(), mHGCopiedUriListNautilus.bytes());
      provider.push_back(Gdk::ContentProvider::create(FCP_TARGET_NAME_GNOME_COPIED_FILES, fileListGnome));
      provider.push_back(Gdk::ContentProvider::create(FCP_TARGET_NAME_NAUTILUS_GTK4_UTF8, fileListNautilus));
   } else if (desktop && strstr(desktop, "KDE")) {
      // Set the Kde file list to GDKContentProvider
      fileListKde = Glib::Bytes::create(mHGCopiedUriListKde.c_str(), mHGCopiedUriListKde.bytes());
      provider.push_back(Gdk::ContentProvider::create(FCP_TARGET_NAME_URI_LIST, fileListKde));
   } else {
      g_warning("%s: Unsupported Desktop Manager %s. \n", __FUNCTION__, desktop);
      return;
   }

   providerUnion = Gdk::ContentProvider::create(provider);
   mDftClipboardPtr->set_content(providerUnion);
   mPrimClipboardPtr->set_content(providerUnion);

   /* Increase the Host Clip number by 2 as we are updating the
    * Clipboard content for both default and Primary Clipboard.
    */
   gHostClipGenNum += 2;
   return;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::BuildFileURIList --
 *
 *      Used to populate the file URI lists for GNOME, Nautilus, and KDE.
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
CopyPasteUIX11::BuildFileURIList(utf::string& uriList,         // IN/OUT
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
}


/*    
 *-----------------------------------------------------------------------------
 *    
 * CopyPasteUIX11::BuildFileContentURIList --
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
CopyPasteUIX11::BuildFileContentURIList(utf::string& uriList,         // IN/OUT
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
 * CopyPasteUIX11::LocalGetFileRequest --
 *
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
CopyPasteUIX11::LocalGetFileRequest()
{
   g_debug("%s: enter.\n", __FUNCTION__);

   if (!mCP->IsCopyPasteAllowed()) {
      g_debug("%s: copy paste not allowed, returning.\n",
            __FUNCTION__);
      return;
   }

   const char *desktop = getenv("XDG_CURRENT_DESKTOP");

   if (mHGGetFileStatus != DND_FILE_TRANSFER_NOT_STARTED) {
      /*
       * On KDE (at least), we can see this multiple times, so ignore if
       * we are already getting files.
       */
      g_debug("%s: GetFiles already started, returning uriList [%s]\n",
              __FUNCTION__, mHGCopiedUriListGnome.c_str());
      LocalSetFileListToClipboard(desktop);
      return;
   } else {
      utf::string hgStagingDir;
      utf::string stagingDirName;

      hgStagingDir = utf::CopyAndFree(DnD_CreateStagingDirectory());
      g_debug("%s: Getting files. Staging dir: %s", __FUNCTION__,
            hgStagingDir.c_str());

      if (0 == hgStagingDir.bytes()) {
         g_debug("%s: Can not create staging directory\n", __FUNCTION__);
	 mDftClipboardPtr->unset_content();
	 mPrimClipboardPtr->unset_content();
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

      /* Provide path within vmblock file system instead of actual path. */
      stagingDirName = GetLastDirName(hgStagingDir);
      if (stagingDirName.empty()) {
         g_debug("%s: Can not get staging directory name\n", __FUNCTION__);
	 mDftClipboardPtr->unset_content();
	 mPrimClipboardPtr->unset_content();
         return;
      }

      if (desktop && strstr(desktop, "GNOME")) {
         /* Setting Gnome URIs for each path in the guest's file list. */
         mHGCopiedUriListGnome = "copy\n";
	 BuildFileURIList(mHGCopiedUriListGnome, FCP_GNOME_LIST_PRE, FCP_GNOME_LIST_POST, stagingDirName);

         /* Nautilus on Gnome does not expect FCP_GNOME_LIST_POST after the last uri. See bug 143147. */
	 mHGCopiedUriListGnome.erase(mHGCopiedUriListGnome.size() - strlen(FCP_GNOME_LIST_POST));
	 g_debug("%s: providing file list [%s]\n", __FUNCTION__,
                 mHGCopiedUriListGnome.c_str());

	 /* Setting Nautilus URIs for each path in the guest's file list. */
         mHGCopiedUriListNautilus = utf::string(FCP_TARGET_MIME_NAUTILUS_FILES) + "\ncopy\n";
	 BuildFileURIList(mHGCopiedUriListNautilus, FCP_GNOME_LIST_PRE, FCP_GNOME_LIST_POST, stagingDirName);
      } else if (desktop && strstr(desktop, "KDE")) {
         /* Setting KDE URIs for each path in the guest's file list. */
	 BuildFileURIList(mHGCopiedUriListKde, DND_URI_LIST_PRE_KDE, DND_URI_LIST_POST, stagingDirName);
	 g_debug("%s: providing file list [%s]\n", __FUNCTION__,
                 mHGCopiedUriListKde.c_str());
      } else {
	 g_warning("%s: Unsupported Desktop Manager %s. \n", __FUNCTION__, desktop);
         return;
      }
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
         struct timeval tv {};

         tv.tv_sec = 0;
         g_main_context_iteration(g_main_loop_get_context(ctx->mainLoop),
                                  FALSE);
         if (select(0, NULL, NULL, NULL, &tv) == -1) {
            g_debug("%s: error in select (%s).\n", __FUNCTION__,
                    strerror(errno));
	    mDftClipboardPtr->unset_content();
	    mPrimClipboardPtr->unset_content();
            return;
         }
      }
      g_debug("%s: file transfer done!\n", __FUNCTION__);
   }

   /* Set the content of clipboard with the File URI's. */
   LocalSetFileListToClipboard(desktop);
   return;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::GetLastDirName --
 *
 *      Try to get last directory name from a full path name.
 *
 * Results:
 *      Last dir name in the full path name if success, empty str otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

utf::string
CopyPasteUIX11::GetLastDirName(const utf::string &str)     // IN
{
   /* This is a common function with DNDUIX11 class.
    * When we implement Drag and Drop with GTK4, we can
    * evaluate and see if this needs to be moved to a common
    * util file.
    */

   char *baseName;
   File_GetPathName(str.c_str(), NULL, &baseName);
   return utf::CopyAndFree(baseName);
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
   /* This is a common function with DNDUIX11 class.
    * When we implement Drag and Drop with GTK4, we can
    * evaluate and see if this needs to be moved to a common
    * util file.
    */

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
      return static_cast<VmTimeType>(0);
   }
   curTime = (tv.tv_sec * 1000000 + tv.tv_usec);
   return curTime;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteUIX11::LocalGetFileContentsRequest --
 *
 *      Sets the files content uri list to clipboard.
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
CopyPasteUIX11::LocalGetFileContentsRequest()
{
   if (!mCP->CheckCapability(DND_CP_CAP_FILE_CONTENT_CP)) {
      /*
       * Disallowed based on caps settings, return.
       */
      return;
   }

   const char *desktop = getenv("XDG_CURRENT_DESKTOP");

   if (desktop && strstr(desktop, "GNOME")) {
      /* Setting Gnome URIs for each path in the guest's file list. */
      mHGCopiedUriListGnome = "copy\n";
      BuildFileContentURIList(mHGCopiedUriListGnome, FCP_GNOME_LIST_PRE, FCP_GNOME_LIST_POST);

      /* Nautilus on Gnome does not expect FCP_GNOME_LIST_POST after the last uri. See bug 143147. */
      mHGCopiedUriListGnome.erase(mHGCopiedUriListGnome.size() - strlen(FCP_GNOME_LIST_POST));
      g_debug("%s: providing file list [%s]\n", __FUNCTION__, mHGCopiedUriListGnome.c_str());

      /* Setting Nautilus URIs for each path in the guest's file list. */
      mHGCopiedUriListNautilus = utf::string(FCP_TARGET_MIME_NAUTILUS_FILES) + "\ncopy\n";
      BuildFileContentURIList(mHGCopiedUriListNautilus, FCP_GNOME_LIST_PRE, FCP_GNOME_LIST_POST);
   } else if (desktop && strstr(desktop, "KDE")) {
      /* Setting KDE URIs for each path in the guest's file list. */
      BuildFileContentURIList(mHGCopiedUriListKde, DND_URI_LIST_PRE_KDE, DND_URI_LIST_POST);
      g_debug("%s: providing file list [%s]\n", __FUNCTION__, mHGCopiedUriListKde.c_str());
   } else {
      g_warning("%s: Unsupported Desktop Manager %s. \n", __FUNCTION__, desktop);
      return;
   }

   /* Set the content of clipboard with the File URI's. */
   LocalSetFileListToClipboard(desktop);
   return;
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
   g_debug("%s: enter.", __FUNCTION__);
   void *buf;
   size_t sz;
   std::vector<Glib::RefPtr<Gdk::ContentProvider>> provider;


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

   mHGFCPData.clear();

   if (CPClipboard_ItemExists(clip, CPFORMAT_TEXT) ||
       CPClipboard_ItemExists(clip, CPFORMAT_RTF)) {
      Glib::RefPtr<Gdk::ContentProvider> providerUnion;
      Glib::RefPtr<Glib::Bytes> textData;
      Glib::RefPtr<Glib::Bytes> rtfData;

      if (CPClipboard_GetItem(clip, CPFORMAT_RTF, &buf, &sz)) {
         g_debug("%s: RTF data, size %" FMTSZ "u.\n", __FUNCTION__, sz);
	 rtfData = Glib::Bytes::create(buf, sz);
	 provider.push_back(Gdk::ContentProvider::create(TARGET_NAME_APPLICATION_RTF, rtfData));
	 provider.push_back(Gdk::ContentProvider::create(TARGET_NAME_TEXT_RICHTEXT, rtfData));
	 provider.push_back(Gdk::ContentProvider::create(TARGET_NAME_TEXT_RTF, rtfData));
      }

      if (CPClipboard_GetItem(clip, CPFORMAT_TEXT, &buf, &sz)) {
         g_debug("%s: Text data, size %" FMTSZ "u.\n", __FUNCTION__, sz);
	 textData = Glib::Bytes::create(buf, sz);
	 provider.push_back(Gdk::ContentProvider::create(TARGET_NAME_TEXT_PLAIN, textData));
	 provider.push_back(Gdk::ContentProvider::create(TARGET_NAME_TEXT_PLAIN_UTF8, textData));
      }

      providerUnion = Gdk::ContentProvider::create(provider);

      if (providerUnion) {
         mDftClipboardPtr->set_content(providerUnion);
	 mPrimClipboardPtr->set_content(providerUnion);
         g_debug("%s: RTF/Text data is set to Clipboard. \n", __FUNCTION__);

	 /* Increase the Host Clip number by 2 as we are updating the
	  * Clipboard content for both default and Primary Clipboard.
	  */
	 gHostClipGenNum += 2;
      } else {
         g_debug("%s: Error in setting RTF/Text data to clipboard. \n", __FUNCTION__);
      }

      return;
   }

   if (CPClipboard_GetItem(clip, CPFORMAT_IMG_PNG, &buf, &sz)) {
      g_debug("%s: PNG data, size %" FMTSZ "u.\n", __FUNCTION__, sz);
      /* Try to load buf into gdk texture, and write to local clipboard. */
      Glib::RefPtr<Glib::Bytes> imgData = Glib::Bytes::create(buf, sz);

      try {
         Glib::RefPtr<Gdk::Texture> image = Gdk::Texture::create_from_bytes(imgData);
         if (image) {
	    mDftClipboardPtr->set_texture(image);
	    mPrimClipboardPtr->set_texture(image);
            g_debug("%s: Image data is set to Clipboard. \n", __FUNCTION__);

	    /* Increase the Host Clip number by 2 as we are updating the
             * Clipboard content for both default and Primary Clipboard.
             */
            gHostClipGenNum += 2;
	 }
      } catch (const Glib::Error& e) {
         /* clipboard failure is not critical one,  warn but do not error exit */
         g_warning("%s: Error in setting Image data to clipboard: %s\n", __FUNCTION__, e.what());
      }

      return;
   }

   if (CPClipboard_GetItem(clip, CPFORMAT_FILELIST, &buf, &sz)) {
      g_debug("%s: File data.\n", __FUNCTION__);
      DnDFileList flist;
      flist.FromCPClipboard(buf, sz);
      mTotalFileSize = flist.GetFileSize();
      mHGFCPData = flist.GetRelPathsStr();
      mHGGetListTime = GetCurrentTime();
      mHGGetFileStatus = DND_FILE_TRANSFER_NOT_STARTED;
      mHGCopiedUriListGnome.clear();
      mHGCopiedUriListKde.clear();
      mHGCopiedUriListNautilus.clear();
      LocalGetFileRequest();
   }

   if (CPClipboard_ItemExists(clip, CPFORMAT_FILECONTENTS)) {
      g_debug("%s: File contents data\n", __FUNCTION__);
      if (LocalPrepareFileContents(clip, mHGFileContentsList)) {
         mHGCopiedUriListGnome.clear();
         mHGCopiedUriListKde.clear();
         mHGCopiedUriListNautilus.clear();
         LocalGetFileContentsRequest();
      }
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
   /* This is a common function with GTK3 implementation of CopyPasteUIX11 class.
    * Keeping it seperate as this API modifies the class member variables
    * mThread and mThreadParams.
    */

   TRACE_CALL();
   ThreadParams *params = static_cast<ThreadParams*>(arg);
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

      char buf[sizeof VMBLOCK_FUSE_READ_RESPONSE];
      ssize_t size;
      size = read(fd, buf, sizeof buf);
      g_debug("%s: Number of bytes read : %" FMTSZ "d\n", __FUNCTION__, size);
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
   /* This is a common function with GTK3 implementation of CopyPasteUIX11 class.
    * Keeping it seperate as this API modifies the class member variables
    * mThread and mThreadParams.
    */

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

