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
 * @file dndUI.cpp --
 *
 * This class implements stubs for the methods that allow DnD between
 * host and guest.
 */

#include "dndUI.h"

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
#include "unity.h"
#include <gtk/gtk.h>
#include <X11/extensions/XTest.h>       /* for XTest*() */
}

#include "dndGuest.h"
#include "copyPasteDnDWrapper.h"

/**
 *
 * Constructor.
 */

DnDUI::DnDUI(DblLnkLst_Links *eventQueue)
    : m_eventQueue(eventQueue),
      m_DnD(NULL),
      m_detWnd(NULL),
      m_detWndFull(NULL),
      m_blockCtrl(NULL),
      m_HGGetDataInProgress(false),
      m_blockAdded(false),
      m_GHDnDInProgress(false),
      m_GHDnDHostStatus(false),
      m_GHDnDAction((Gdk::DragAction)0),
      m_GHDnDDataReceived(false),
      m_GHDnDDropOccurred(false),
      m_VMIsSource(false),
      m_unityMode(false),
      m_inHGDrag(false),
      m_effect(DROP_UNKNOWN),
      m_needsBlock(false),
      m_isFileDnD(false)
{
   Debug("%s: enter\n", __FUNCTION__);
}


/**
 *
 * Destructor.
 */

DnDUI::~DnDUI()
{
   Debug("%s: enter\n", __FUNCTION__);
   Reset();
   if (m_DnD) {
      delete m_DnD;
   }
   if (m_detWnd) {
      delete m_detWnd;
   }
   if (m_detWndFull) {
      delete m_detWndFull;
   }
   m_feedbackChanged.disconnect();
   CPClipboard_Destroy(&m_clipboard);
}


/**
 *
 * Initialize DnDUI object.
 */

void
DnDUI::Init()
{
   Debug("%s: enter\n", __FUNCTION__);
   char *reply = NULL;
   size_t replyLen;

   ASSERT(m_eventQueue);
   CPClipboard_Init(&m_clipboard);
   m_DnD = new DnD(m_eventQueue);
   if (!m_DnD) {
      Debug("%s: unable to allocate DnD object\n", __FUNCTION__);
      goto fail;
   }
   m_detWnd = new DragDetWnd();
   if (!m_detWnd) {
      Debug("%s: unable to allocate DragDetWnd object\n", __FUNCTION__);
      goto fail;
   }
   m_detWndFull = new DragDetWnd();
   if (!m_detWndFull) {
      Debug("%s: unable to allocate DragDetWnd object\n", __FUNCTION__);
      goto fail;
   }

#if defined(DETWNDDEBUG)

   /*
    * This code can only be called when DragDetWnd is derived from
    * Gtk::Window. The normal case is that DragDetWnd is an instance of
    * Gtk::Invisible, which doesn't implement the methods that SetAttributes
    * relies upon.
    */

   m_detWnd->SetAttributes();
   m_detWndFull->SetAttributes();
#endif

   SetTargetsAndCallbacks();

   /* Exchange dnd version information with the VMX */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.dnd_version 3")) {
      Debug("%s: could not set guest dnd version capability\n", __FUNCTION__);
      goto fail;
   } else {
      if (!RpcOut_sendOne(&reply, &replyLen, "vmx.capability.dnd_version")) {
         Debug("%s: could not get VMX dnd version capability\n",
               __FUNCTION__);
         goto fail;
      } else if (atoi(reply) < 3) {
         Debug("%s: VMX DnD version is less than 3.\n", __FUNCTION__);
         goto fail;
      }
   }

   Debug("%s: VMX version ok: %d\n", __FUNCTION__, atoi(reply));

   m_DnD->reset.connect(sigc::mem_fun(this, &DnDUI::ResetUIStateCB));
   m_DnD->dragStartChanged.connect(
      sigc::mem_fun(this, &DnDUI::RemoteDragStartCB));
   m_DnD->fileCopyDoneChanged.connect(
      sigc::mem_fun(this, &DnDUI::GetLocalFilesDoneCB));
   m_DnD->updateDetWndChanged.connect(
      sigc::mem_fun(this, &DnDUI::UpdateDetWndCB));
   m_DnD->updateUnityDetWndChanged.connect(
      sigc::mem_fun(this, &DnDUI::UpdateUnityDetWndCB));
   m_DnD->sourceCancelChanged.connect(
      sigc::mem_fun(this, &DnDUI::SourceCancelCB));
   m_DnD->sourceDropChanged.connect(
      sigc::mem_fun(this, &DnDUI::SourceDropCB));

   m_detWnd->signal_drag_data_get().connect(
      sigc::mem_fun(this, &DnDUI::LocalDragDataGetCB));
   m_detWnd->signal_drag_end().connect(
      sigc::mem_fun(this, &DnDUI::LocalDragEndCB));
   m_detWnd->signal_drag_drop().connect(
      sigc::mem_fun(this, &DnDUI::LocalDragDropCB));

   UpdateDetWndCB(false, 0, 0);
   UpdateUnityDetWndCB(false, 0);
   goto out;
fail:
   if (m_DnD) {
      delete m_DnD;
   }
   if (m_detWnd) {
      delete m_detWnd;
   }
   if (m_detWndFull) {
      delete m_detWndFull;
   }
out:
   if (reply) {
      free(reply);
   }
   return;
}


/**
 *
 * Setup targets we support, claim ourselves as a drag destination, and
 * register callbacks for Gdk drag and drop callbacks the platform will
 * send to us.
 */

void
DnDUI::SetTargetsAndCallbacks()
{
   Debug("%s: enter\n", __FUNCTION__);

   /* Construct supported target list for HG DnD. */
   std::list<Gtk::TargetEntry> targets;

   /* File DnD. */
   targets.push_back(Gtk::TargetEntry(DRAG_TARGET_NAME_URI_LIST));

   /* RTF text DnD. */
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_APPLICATION_RTF));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_TEXT_RICHTEXT));

   /* Plain text DnD. */
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_STRING));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_TEXT_PLAIN));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_UTF8_STRING));
   targets.push_back(Gtk::TargetEntry(TARGET_NAME_COMPOUND_TEXT));

   /*
    * We don't want Gtk handling any signals for us, we want to
    * do it ourselves based on the results from the guest.
    *
    * Second argument in drag_dest_set defines the automatic behaviour options
    * of the destination widget. We used to not define it (0) and in some
    * distributions (like Ubuntu 6.10) DragMotion only get called once,
    * and not send updated mouse position to guest, and also got cancel
    * signal when user drop the file (bug 175754). With flag DEST_DEFAULT_MOTION
    * the bug is fixed. Almost all other example codes use DEST_DEFAULT_ALL
    * but in our case, we will call drag_get_data during DragMotion, and
    * will cause X dead with DEST_DEFAULT_ALL. The reason is unclear.
    */
   m_detWnd->drag_dest_set(targets, Gtk::DEST_DEFAULT_MOTION,
                           Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
   m_detWnd->signal_drag_leave().connect(sigc::mem_fun(this, &DnDUI::LocalDragLeaveCB));
   m_detWnd->signal_drag_motion().connect(sigc::mem_fun(this, &DnDUI::LocalDragMotionCB));
   m_detWnd->signal_drag_drop().connect(sigc::mem_fun(this, &DnDUI::LocalDragDropCB));
   m_detWnd->signal_drag_data_received().connect(sigc::mem_fun(this, &DnDUI::LocalDragDataReceivedCB));
}


/**
 *
 * Reset Callback to reset dnd ui state.
 */

void
DnDUI::ResetUIStateCB(void)
{
   m_GHDnDHostStatus = false;
   m_GHDnDAction = (Gdk::DragAction)0;
   m_GHDnDDataReceived = false;
   m_GHDnDDropOccurred = false;
   m_HGGetDataInProgress = false;
   m_VMIsSource = false;
   m_GHDnDInProgress = false;
   m_feedbackChanged.disconnect();
   m_effect = DROP_UNKNOWN;
   RemoveBlock();
}


/**
 * Set block control member.
 *
 * @param[in] block control as setup by vmware-user.
 */

void
DnDUI::SetBlockControl(DnDBlockControl *blockCtrl)
{
   m_blockCtrl = blockCtrl;
}

/* Source functions for HG DnD. */

/**
 *
 * Called when host successfully detected a pending HG drag.
 *
 * param[in] clip cross-platform clipboard
 * param[in] stagingDir associated staging directory
 */

void
DnDUI::RemoteDragStartCB(const CPClipboard *clip, std::string stagingDir)
{
   Glib::RefPtr<Gtk::TargetList> targets;
   Gdk::DragAction actions;
   GdkEventMotion event;

   CPClipboard_Clear(&m_clipboard);
   CPClipboard_Copy(&m_clipboard, clip);

   Debug("%s: enter\n", __FUNCTION__);

   /*
    * Construct the target and action list, as well as a fake motion notify
    * event that's consistent with one that would typically start a drag.
    */
   targets = Gtk::TargetList::create(std::list<Gtk::TargetEntry>());

   if (CPClipboard_ItemExists(&m_clipboard, CPFORMAT_FILELIST)) {
      m_HGStagingDir = stagingDir;
      if (!m_HGStagingDir.empty()) {
         targets->add(Glib::ustring(DRAG_TARGET_NAME_URI_LIST));
         /* Add private data to tag dnd as originating from this vm. */
         char *pid;
         Debug("%s: adding re-entrant drop target, pid %d\n", __FUNCTION__, getpid());
         pid = Str_Asprintf(NULL, "guest-dnd-target %d", static_cast<int>(getpid()));
         if (pid) {
            targets->add(Glib::ustring(pid));
            free(pid);
         }
      }
   }

   if (CPClipboard_ItemExists(&m_clipboard, CPFORMAT_FILECONTENTS)) {
      if (LocalPrepareFileContentsDrag()) {
         targets->add(Glib::ustring(DRAG_TARGET_NAME_URI_LIST));
      }
   }

   if (CPClipboard_ItemExists(&m_clipboard, CPFORMAT_TEXT)) {
      targets->add(Glib::ustring(TARGET_NAME_STRING));
      targets->add(Glib::ustring(TARGET_NAME_TEXT_PLAIN));
      targets->add(Glib::ustring(TARGET_NAME_UTF8_STRING));
      targets->add(Glib::ustring(TARGET_NAME_COMPOUND_TEXT));
   }

   if (CPClipboard_ItemExists(&m_clipboard, CPFORMAT_RTF)) {
      targets->add(Glib::ustring(TARGET_NAME_APPLICATION_RTF));
      targets->add(Glib::ustring(TARGET_NAME_TEXT_RICHTEXT));
   }

   actions = Gdk::ACTION_COPY | Gdk::ACTION_MOVE;

   /* TODO set the x/y coords to the actual drag initialization point. */
   event.type = GDK_MOTION_NOTIFY;
   event.window = m_detWnd->get_window()->gobj();
   event.send_event = FALSE;
   event.time = GDK_CURRENT_TIME;
   event.x = 10;
   event.y = 10;
   event.axes = NULL;
   event.state = GDK_BUTTON1_MASK;
   event.is_hint = 0;
   event.device = gdk_device_get_core_pointer();
   event.x_root = 0;
   event.y_root = 5;

   /* Tell Gtk that a drag should be started from this widget. */
   m_detWnd->drag_begin(targets, actions, 1, (GdkEvent *)&event);
   m_blockAdded = false;
   m_needsBlock = false;
   m_isFileDnD = false;
   SourceDragStartDone();
}


/**
 *
 * Try to extract file contents from m_clipboard. Write all files to a
 * temporary staging directory. Construct uri list.
 *
 * @return TRUE if success, FALSE otherwise.
 */

bool
DnDUI::LocalPrepareFileContentsDrag(void)
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
   bool ret = FALSE;

   if (!CPClipboard_GetItem(&m_clipboard, CPFORMAT_FILECONTENTS, &buf, &sz)) {
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

   m_HGFileContentsUriList = "";

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
         Debug("%s: created file [%s].\n",
               __FUNCTION__, filePathName.c_str());
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
         m_HGFileContentsUriList += "file://" + filePathName + "\r\n";
      }
   }
   Debug("%s: created uri list [%s].\n",
         __FUNCTION__, m_HGFileContentsUriList.c_str());
   ret = true;

exit:
   xdr_free((xdrproc_t) xdr_CPFileContents, (char *)&fileContents);
   if (tempDir && !ret) {
      DnD_DeleteStagingFiles(tempDir, FALSE);
   }
   free(tempDir);
   return ret;
}


/**
 *
 * Tell host that we are done with HG DnD initialization.
 */

void
DnDUI::SourceDragStartDone(void)
{
   Debug("%s: enter\n", __FUNCTION__);
   m_inHGDrag = true;
   m_DnD->HGDragStartDone();
}


/**
 *
 * Handler for when the guest provides drag status during a host->guest
 * operation. This will change as we drag over different things in the guest.
 * Here, we map what is report to a Gdk drag action, and then forward this
 * status value to the Gdk drag context.
 *
 * @param[in] effect drop effect
 * @param[in] gc Gdk drag context object, to which status is forwarded.
 */

void
DnDUI::SourceFeedbackChangedCB(DND_DROPEFFECT effect,
                               const Glib::RefPtr<Gdk::DragContext> dc)
{
   Debug("%s: enter\n", __FUNCTION__);
   if (effect == DROP_NONE) {
      m_GHDnDAction = (Gdk::DragAction)0;
   } else if (effect & DROP_COPY) {
      m_GHDnDAction = Gdk::ACTION_COPY;
   } else if (effect & DROP_MOVE) {
      m_GHDnDAction = Gdk::ACTION_MOVE;
   } else if (effect & DROP_LINK) {
      /*
       * We don't do link.
       */
      m_GHDnDAction = Gdk::ACTION_COPY;
   } else {
      m_GHDnDAction = (Gdk::DragAction)0;
   }
   m_GHDnDHostStatus = !!effect;

   dc->drag_status(m_GHDnDAction, GDK_CURRENT_TIME);
}


/**
 *
 * Cancel current HG DnD.
 */

void
DnDUI::SourceCancelCB(void)
{
   Debug("%s: entering\n", __FUNCTION__);
   m_inHGDrag = false;
}


/**
 *
 * Got drop from host side. Add block first then host can simulate the drop
 * in the guest.
 */

void
DnDUI::SourceDropCB(void)
{
   m_inHGDrag = false;

   if (m_needsBlock) {
      AddBlock();
      m_needsBlock = false;
   }
}


/**
 *
 * Callback when HG file transfer is done, which finishes the file
 * copying from host to guest staging directory.
 *
 * @param[in] success if true, transfer was successful
 * @param[in] path of staging dir (which will have a block that needs removing)
 */

void
DnDUI::GetLocalFilesDoneCB(bool success,
                           std::vector<uint8> stagingDir)
{
   Debug("%s: %s\n", __FUNCTION__, success ? "success" : "failed");
   /* Copied files are already removed in common layer. */
   stagingDir.clear();
   Reset();
   m_HGGetDataInProgress = false;
   RemoveBlock();
}


/* Target functions for GH DnD. */

/**
 *
 * This is triggered when user drags valid data from guest to host. Try to
 * get clip data and notify host to start GH DnD.
 */

void
DnDUI::TargetDragEnter(void)
{
   Debug("%s: entering\n", __FUNCTION__);

   /* Check if there is valid data with current detection window. */
   if (!CPClipboard_IsEmpty(&m_clipboard)) {
      Debug("%s: got valid data from detWnd.\n", __FUNCTION__);
      m_DnD->DragEnter(&m_clipboard);
   }
}


/**
 *
 * Shows/hides drag detection windows based on the mask.
 *
 * @param[in] bShow if true, show the window, else hide it.
 * @param[in] x x-coordinate to which the detection window needs to be moved
 * @param[in] y y-coordinate to which the detection window needs to be moved
 */

void
DnDUI::UpdateDetWndCB(bool bShow,
                      int32 x,
                      int32 y)
{
   /* If the window is being shown, move it to the right place. */
   if (bShow) {
      x = MAX(x - DRAG_DET_WINDOW_WIDTH / 2, 0);
      y = MAX(y - DRAG_DET_WINDOW_WIDTH / 2, 0);

      m_detWnd->Show();
      m_detWnd->Raise();
      m_detWnd->SetGeometry(x, y, 2 * DRAG_DET_WINDOW_WIDTH, 2 * DRAG_DET_WINDOW_WIDTH);
      /*
       * Wiggle the mouse here. Especially for G->H DnD, this improves
       * reliability of making the drag escape the guest window immensly.
       * Stolen from the legacy V2 DnD code.
       */

      DnDHGFakeMove(x, y);
   } else {
      m_detWnd->Hide();
   }
}


/**
 *
 * Shows/hides full-screen Unity drag detection window.
 *
 * @param[in] bShow if true, show the window, else hide it.
 * @param[in] unityWndId active front window
 */

void
DnDUI::UpdateUnityDetWndCB(bool bShow,
                           uint32 unityWndId)
{
   if (bShow && unityWndId > 0) {
      int width = m_detWndFull->GetScreenWidth();
      int height = m_detWndFull->GetScreenHeight();
      m_detWndFull->SetGeometry(0, 0, width, height);
      m_detWndFull->Lower();
      m_detWndFull->Show();
   } else {
      m_detWndFull->Hide();
   }
}


/**
 *
 * Reset the UI independent DnD layer.
 */

void
DnDUI::Reset(void)
{
   Debug("%s: entering\n", __FUNCTION__);
   if (m_DnD) {
      m_DnD->ResetDnD();
   }
}


/**
 *
 * "drag_motion" signal handler for GTK. We should respond by setting drag
 * status. Note that there is no drag enter signal. We need to figure out
 * if a new drag is happening on our own. Also, we don't respond with a
 * "allowed" drag status right away, we start a new drag operation over VMDB
 * (which tries to notify the host of the new operation). Once the host has
 * responded), we respond with a proper drag status.
 *
 * @param[in] dc associated drag context
 * @param[in] x x coordinate of the drag motion
 * @param[in] y y coordinate of the drag motion
 * @param[in] time time of the drag motion
 *
 * @return returning false means we won't get notified of future motion. So,
 * we only return false if we don't recognize the types being offered. We
 * return true otherwise, even if we don't accept the drag right now for some
 * other reason.
 */

bool
DnDUI::LocalDragMotionCB(const Glib::RefPtr<Gdk::DragContext> &dc,
                         int x,
                         int y,
                         guint time)
{
   DND_DROPEFFECT effect;

   effect = ToDropEffect(dc->get_action());
   if (effect != m_effect) {
      m_effect = effect;
      Debug("%s: Updating feedback\n", __FUNCTION__);
      SourceUpdateFeedback(m_effect);
   }

   /*
    * If this is a Host to Guest drag, we are done here, so return.
    */

   if (m_inHGDrag) {
      return true;
   }

   Gdk::DragAction srcActions;
   Gdk::DragAction suggestedAction;
   Glib::ustring target = m_detWnd->drag_dest_find_target(dc);

   if (!m_DnD->IsDnDAllowed()) {
      Debug("%s: No dnd allowed!\n", __FUNCTION__);
      m_GHDnDAction = (Gdk::DragAction)0;
      dc->drag_status(m_GHDnDAction, time);
      return true;
   }

   /* Check if dnd began from this vm. */

   /*
    * TODO: Once we upgrade to shipping gtkmm 2.12, we can go back to
    *       Gdk::DragContext::get_targets, but API/ABI broke between 2.10 and
    *       2.12, so we work around it like this for now.
    */
   Glib::ListHandle<std::string, Gdk::AtomStringTraits> targets(
      dc->gobj()->targets, Glib::OWNERSHIP_NONE);

   std::list<Glib::ustring> as = targets;
   std::list<Glib::ustring>::iterator result;
   char *pid;
   pid = Str_Asprintf(NULL, "guest-dnd-target %d", static_cast<int>(getpid()));
   if (pid) {
      result = std::find(as.begin(), as.end(), std::string(pid));
      free(pid);
   } else {
      result = as.end();
   }
   if (result != as.end()) {
      Debug("%s: found re-entrant drop target, pid %s\n", __FUNCTION__, pid );
      if (m_GHDnDDataReceived) {
         Debug("%s: re-entrant calling SetMouse()\n", __FUNCTION__);
         m_DnD->SetMouse(x, y, true);
      } else {
         Debug("%s: re-entrant calling DragEnter()\n", __FUNCTION__);
         m_localDragLeaveTimer.disconnect();
         m_DnD->DragEnter(NULL);
         m_GHDnDDataReceived = true;
         m_VMIsSource = true;
      }
      return true;
   }

   if (target != "") {
      /*
       * We give preference to the suggested action from the source, and prefer
       * copy over move.
       */
      suggestedAction = dc->get_suggested_action();
      srcActions = dc->get_actions();
      if (suggestedAction == Gdk::ACTION_COPY || suggestedAction == Gdk::ACTION_MOVE) {
         m_GHDnDAction = suggestedAction;
      } else if (srcActions & Gdk::ACTION_COPY) {
         m_GHDnDAction= Gdk::ACTION_COPY;
      } else if (srcActions & Gdk::ACTION_MOVE) {
         m_GHDnDAction = Gdk::ACTION_MOVE;
      } else {
         m_GHDnDAction = (Gdk::DragAction)0;
      }
   } else {
      m_GHDnDAction = (Gdk::DragAction)0;
   }

   if (m_GHDnDAction != (Gdk::DragAction)0) {
      if (!m_GHDnDInProgress) {
         Debug("%s: new drag, need to get data for host\n", __FUNCTION__);
         /*
          * This is a new drag operation. We need to start a drag thru the
          * backdoor, and to the host. Before we can tell the host, we have to
          * retrieve the drop data.
          */
         m_GHDnDInProgress = true;
         m_GHDnDAction = (Gdk::DragAction)0;
         /* only begin drag enter after we get the data */
         /* Need to grab all of the data. */
         m_detWnd->drag_get_data(dc, target, time);
      } else if (m_GHDnDDataReceived) {
         Debug("%s: m_GHDnDDataReceived, setting mouse position\n", __FUNCTION__);
         m_DnD->SetMouse(x, y, true);

         if (m_GHDnDHostStatus) {
            /* Got host response, respond with a proper drag status. */
            Debug("%s: Existing status, calling drag_status()\n", __FUNCTION__);
            dc->drag_status(m_GHDnDAction, time);
         }
      } else {
         Debug("%s: Multiple drag motions before gh data has been received.\n",
               __FUNCTION__);
      }
   } else {
      Debug("%s: Invalid drag\n", __FUNCTION__);
      return false;
   }
   return true;
}


/**
 *
 * Got feedback from our DropSource, send it over to host. Called by
 * drag motion callback.
 *
 * @param[in] effect feedback to send to the UI-independent DnD layer.
 */

void
DnDUI::SourceUpdateFeedback(DND_DROPEFFECT effect)
{
   Debug("%s: entering\n", __FUNCTION__);
   m_DnD->SetFeedback(effect);
}


/**
 *
 * "drag_leave" signal handler for GTK. We can't clean up the drop
 *  operation here, because it might be part of a drop. See the
 *  LocalDragLeaveTimeout() function for more details.
 *
 *  @param[in] dc drag context
 *  @param[in] time time of the drag
 */

void
DnDUI::LocalDragLeaveCB(const Glib::RefPtr<Gdk::DragContext> &dc, guint time)
{
   Debug("%s: enter\n", __FUNCTION__);
   m_localDragLeaveTimer.disconnect();
   m_localDragLeaveTimer = Glib::signal_timeout().connect(
      sigc::bind_return(sigc::mem_fun(this, &DnDUI::LocalDragLeaveTimeout), false),
      DRAG_LEAVE_TIMEOUT);
   m_feedbackChanged.disconnect();
   dc->drag_finish(true, dc->get_action() == Gdk::ACTION_MOVE, time);
}


/**
 *
 * Gtk emits "drag_leave" both when the mouse leaves our widget (in which
 * case we should cancel the in progress dnd operation), and as part
 * of a "drag_drop" (in which case our drag_leave handler should NOT
 * cancel the operation. The trouble is, our drag_leave handler does not
 * have enough information to determine whether it's dealing with a
 * real leave or a drop. So, the drag_leave handler sets a timeout which
 * calls this function when it expires.
 *
 * If this function is called, it means a previous "drag_leave" signal is
 * really a leave and we should clean up the in-progress drag operation.
 *
 * @note side effects: cancel the drop operation and tell the host to
 * cancel its drop operation.
 */

void
DnDUI::LocalDragLeaveTimeout(void)
{
   Debug("%s: enter\n", __FUNCTION__);
   m_DnD->DragLeave(0, 0);
   ResetUIStateCB();
}


/**
 *
 * "drag_drop" signal handler for GTK. Send the drop to the host (by
 * way of the backdoor), then tell the host to get the files.
 *
 * @param[in] dc drag context
 * @param[in] x x location of the drop
 * @param[in] y y location of the drop
 * @param[in] time timestamp for the drop
 *
 * @return true on success, false otherwise.
 */

bool
DnDUI::LocalDragDropCB(const Glib::RefPtr<Gdk::DragContext> &dc,
                       int x,
                       int y,
                       guint time)
{
   Debug("%s: enter x %d y %d\n", __FUNCTION__, x, y);

   Glib::ustring target;

   m_localDragLeaveTimer.disconnect();
   target = m_detWnd->drag_dest_find_target(dc);

   if (m_VMIsSource) {
      Debug("%s: Dropping in same vm.\n", __FUNCTION__);
      /* Act as a drop */
      ResetUIStateCB();
      return false;
   }

   if (target == "") {
      Debug("%s: No valid data on clipboard.\n", __FUNCTION__);
      m_DnD->ResetDnD();
      return false;
   }

   if (CPClipboard_IsEmpty(&m_clipboard)) {
      Debug("%s: No valid data on m_clipboard.\n", __FUNCTION__);
      m_DnD->ResetDnD();
      return false;
   }
   return true;
}


/**
 *
 * "drag_data_get" handler for GTK. We don't send drop until we are done.
 *
 * @param[in] dc drag state
 * @param[in] selection_data buffer for data
 * @param[in] info unused
 * @param[in] time timestamp
 *
 * @note if the drop has occurred, the files are copied from the guest.
 *
 *-----------------------------------------------------------------------------
 */

void
DnDUI::LocalDragDataGetCB(const Glib::RefPtr<Gdk::DragContext> &dc,
                          Gtk::SelectionData& selection_data,
                          guint info,
                          guint time)
{
   size_t index = 0;
   std::string str;
   std::string uriList;
   std::string stagingDirName;
   void *buf;
   size_t sz;
   utf::utf8string hgData;
   DnDFileList fList;
   std::string pre;
   std::string post;

   const utf::string target = selection_data.get_target().c_str();

   if (target == DRAG_TARGET_NAME_URI_LIST &&
       CPClipboard_GetItem(&m_clipboard, CPFORMAT_FILELIST, &buf, &sz)) {

      /* Provide path within vmblock file system instead of actual path. */
      stagingDirName = GetLastDirName(m_HGStagingDir);
      if (stagingDirName.length() == 0) {
         Debug("%s: Cannot get staging directory name, stagingDir: %s\n",
               __FUNCTION__, m_HGStagingDir.c_str());
         return;
      }

      if (!fList.FromCPClipboard(buf, sz)) {
         Debug("%s: Can't get data from clipboard\n", __FUNCTION__);
         return;
      }

      /* Provide URIs for each path in the guest's file list. */
      if (FCP_TARGET_INFO_GNOME_COPIED_FILES == info) {
         pre = FCP_GNOME_LIST_PRE;
         post = FCP_GNOME_LIST_POST;
      } else if (FCP_TARGET_INFO_URI_LIST == info) {
         pre = DND_URI_LIST_PRE_KDE;
         post = DND_URI_LIST_POST;
      } else {
         Debug("%s: Unknown request target: %s\n", __FUNCTION__,
               selection_data.get_target().c_str());
         return;
      }

      /* Provide path within vmblock file system instead of actual path. */
      hgData = fList.GetRelPathsStr();

      /* Provide URIs for each path in the guest's file list. */
      while ((str = GetNextPath(hgData, index).c_str()).length() != 0) {
         uriList += pre;
         if (DnD_BlockIsReady(m_blockCtrl)) {
            uriList += m_blockCtrl->blockRoot;
            uriList += DIRSEPS + stagingDirName + DIRSEPS + str + post;
         } else {
            uriList += DIRSEPS + m_HGStagingDir + DIRSEPS + str + post;
         }
      }

      m_HGGetDataInProgress = true;
      selection_data.set(DRAG_TARGET_NAME_URI_LIST, uriList.c_str());
      m_needsBlock = true;
      m_isFileDnD = true;
      return;
   }

   if (target == DRAG_TARGET_NAME_URI_LIST &&
       CPClipboard_ItemExists(&m_clipboard, CPFORMAT_FILECONTENTS)) {
      Debug("%s: Providing uriList [%s] for file contents DnD\n",
            __FUNCTION__, m_HGFileContentsUriList.c_str());

      selection_data.set(DRAG_TARGET_NAME_URI_LIST,
                         m_HGFileContentsUriList.c_str());
      return;
   }

   if ((target == TARGET_NAME_STRING ||
        target == TARGET_NAME_TEXT_PLAIN ||
        target == TARGET_NAME_UTF8_STRING ||
        target == TARGET_NAME_COMPOUND_TEXT) &&
       CPClipboard_GetItem(&m_clipboard, CPFORMAT_TEXT, &buf, &sz)) {
      Debug("%s: providing plain text, size %"FMTSZ"u\n", __FUNCTION__, sz);
      selection_data.set(target.c_str(), (const char *)buf);
      return;
   }

   if ((target == TARGET_NAME_APPLICATION_RTF ||
        target == TARGET_NAME_TEXT_RICHTEXT) &&
       CPClipboard_GetItem(&m_clipboard, CPFORMAT_RTF, &buf, &sz)) {
      Debug("%s: providing rtf text, size %"FMTSZ"u\n", __FUNCTION__, sz);
      selection_data.set(target.c_str(), (const char *)buf);
      return;
   }

   /* Can not get any valid data, cancel this HG DnD. */
   Debug("%s: no valid data for HG DnD\n", __FUNCTION__);
   m_DnD->SourceCancel();
   ResetUIStateCB();
}


/**
 *
 * "drag_end" handler for GTK.
 *
 * @param[in] dc drag state
 */

void
DnDUI::LocalDragEndCB(const Glib::RefPtr<Gdk::DragContext> &dc)
{
   Debug("%s: enter\n", __FUNCTION__);
   if (!m_isFileDnD) {
      Reset();
   }
}


/**
 *
 * "drag_data_received" signal handler for GTK. We requested the drag
 * data earlier from some drag source on the guest; this is the response.
 *
 * This is for G->H DnD.
 *
 * @param[in] dc drag context
 * @param[in] x where the drop happened
 * @param[in] y where the drop happened
 * @param[in] sd the received data
 * @param[in] info the info that has been registered with the target in the
 * target list.
 * @param[in] time the timestamp at which the data was received.
 */

void
DnDUI::LocalDragDataReceivedCB(const Glib::RefPtr<Gdk::DragContext> &dc,
                               int x,
                               int y,
                               const Gtk::SelectionData& sd,
                               guint info,
                               guint time)
{
   Debug("%s: enter\n", __FUNCTION__);
   /* The HG DnD may already finish before we got response. */
   if (!m_GHDnDInProgress) {
      Debug("%s: not valid\n", __FUNCTION__);
      return;
   }

   CPClipboard_Clear(&m_clipboard);

   /*
    * Try to get data provided from the source.  If we cannot get any data,
    * there is no need to inform the guest of anything.
    */
   LocalGetSelection(sd);
   if (CPClipboard_IsEmpty(&m_clipboard)) {
      Debug("%s: Failed getting item.\n", __FUNCTION__);
      return;
   }

   /*
    * There are two points in the DnD process at which this is called, and both
    * are in response to us calling drag_data_get().  The first occurs on the
    * first "drag_motion" received and is used to start a drag; at that point
    * we need to provide the file list to the guest so we request the data from
    * the target.  The second occurs when the "drag_drop" signal is received
    * and we confirm this data with the target before starting the drop.
    *
    * Note that we prevent against sending multiple "dragStart"s or "drop"s for
    * each DnD.
    */
   if (!m_GHDnDDataReceived) {
      Debug("%s: Drag entering.\n", __FUNCTION__);
      m_GHDnDDataReceived = true;
      m_localDragLeaveTimer.disconnect();
      TargetDragEnter();
      m_feedbackChanged = m_DnD->updateFeedbackChanged.connect(
         sigc::bind(sigc::mem_fun(this, &DnDUI::SourceFeedbackChangedCB), dc));
   } else if (m_GHDnDDropOccurred) {
      Debug("%s: Drag dropping.\n", __FUNCTION__);
      m_DnD->TargetDrop(&m_clipboard, x, y);
      dc->drag_finish(true, dc->get_action() == Gdk::ACTION_MOVE, time);
      /*
       * Can't use ResetUIStateCB because file transfer state should not be
       * cleared.
       */
      m_GHDnDDataReceived = false;
      m_GHDnDAction = (Gdk::DragAction)0;
      m_GHDnDHostStatus = false;
      m_GHDnDInProgress = false;
      m_feedbackChanged.disconnect();
   } else {
      Debug("%s: neither m_GHDnDDropOccurred, nor !m_GHDnDDataReceived\n",
         __FUNCTION__);
   }
}


/**
 *
 * Try to construct cross-platform clipboard data from selection data.
 *
 * @param[in] sd Gtk selection data to convert to CP clipboard data
 */

void
DnDUI::LocalGetSelection(const Gtk::SelectionData& sd) // IN
{
   char *newPath;
   char *newRelPath;
   size_t newPathLen;
   size_t index = 0;
   DnDFileList fileList;
   DynBuf buf;
   uint64 totalSize = 0;
   int64 size;

   const utf::string target = sd.get_target().c_str();

   /* Try to get file list. */
   if (target == DRAG_TARGET_NAME_URI_LIST) {
      /*
       * Turn the uri list into two \0  delimited lists. One for full paths and
       * one for just the last path component.
       */
      utf::string source = sd.get_data_as_string().c_str();
      Debug("%s: Got file list: [%s]\n", __FUNCTION__, source.c_str());
      /*
       * In gnome, before file list there may be a extra line indicating it
       * is a copy or cut.
       */
      if (source.length() >= 5 && source.compare(0, 5, "copy\n") == 0) {
         source = source.erase(0, 5);
      }

      if (source.length() >= 4 && source.compare(0, 4, "cut\n") == 0) {
         source = source.erase(0, 4);
      }

      while (source.length() > 0 &&
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

         if ((size = File_GetSize(newPath)) >= 0) {
            totalSize += size;
         } else {
            Debug("%s: unable to get file size for %s\n", __FUNCTION__, newPath);
         }
         Debug("%s: Adding newPath '%s' newRelPath '%s'\n", __FUNCTION__,
               newPath, newRelPath);
         fileList.AddFile(newPath, newRelPath);
         free(newPath);
      }

      DynBuf_Init(&buf);
      fileList.SetFileSize(totalSize);
      if (fileList.ToCPClipboard(&buf, false)) {
          CPClipboard_SetItem(&m_clipboard, CPFORMAT_FILELIST, DynBuf_Get(&buf),
                              DynBuf_GetSize(&buf));
      }
      DynBuf_Destroy(&buf);
      return;
   }

   /* Try to get plain text. */
   if (target == TARGET_NAME_STRING ||
       target == TARGET_NAME_TEXT_PLAIN ||
       target == TARGET_NAME_UTF8_STRING ||
       target == TARGET_NAME_COMPOUND_TEXT) {
      utf::string source = sd.get_data_as_string().c_str();
      if (source.bytes() > 0 &&
          source.bytes() < DNDMSG_MAX_ARGSZ &&
          CPClipboard_SetItem(&m_clipboard, CPFORMAT_TEXT, source.c_str(),
                              source.bytes() + 1)) {
         Debug("%s: Got text, size %"FMTSZ"u\n", __FUNCTION__, source.bytes());
      } else {
         Debug("%s: Failed to get text\n", __FUNCTION__);
      }
      return;
   }

   /* Try to get RTF string. */
   if (target == TARGET_NAME_APPLICATION_RTF ||
       target == TARGET_NAME_TEXT_RICHTEXT) {
      utf::string source = sd.get_data_as_string().c_str();
      if (source.bytes() > 0 &&
          source.bytes() < DNDMSG_MAX_ARGSZ &&
          CPClipboard_SetItem(&m_clipboard, CPFORMAT_RTF, source.c_str(),
                              source.bytes() + 1)) {
         Debug("%s: Got RTF, size %"FMTSZ"u\n", __FUNCTION__, source.bytes());
      } else {
         Debug("%s: Failed to get text\n", __FUNCTION__ );
      }
   }
}


/**
 *
 * Try to get last directory name from a full path name.
 *
 * @param[in] str pathname to process
 *
 * @return last dir name in the full path name if sucess, empty str otherwise
 */

std::string
DnDUI::GetLastDirName(const std::string &str)
{
   std::string ret;
   size_t start;
   size_t end;

   end = str.size() - 1;
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


/**
 *
 * Provide a substring containing the next path from the provided
 * NUL-delimited string starting at the provided index.
 *
 * @param[in] str NUL-delimited path list
 * @param[in] index current index into string
 *
 * @return a string with the next path or "" if there are no more paths.
 */

utf::utf8string
DnDUI::GetNextPath(utf::utf8string& str,
                   size_t& index)
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


/**
 *
 * Issue a fake mouse move event to the detection window. Code stolen from
 * DnD V2 Linux guest implementation, where it was originally defined as a
 * macro.
 *
 * @param[in] x x-coordinate of location to move mouse to.
 * @param[in] y y-coordinate of location to move mouse to.
 *
 * @return true on success, false on failure.
 */

bool
DnDUI::DnDHGFakeMove(const int x,
                     const int y)
{
   return DnDFakeXEvents(false, false, false, false, true, x, y);
}


/**
 *
 * Fake X mouse events and window movement for the provided Gtk widget.
 *
 * This function will optionally show the widget, move the provided widget
 * to either the provided location or the current mouse position if no
 * coordinates are provided, and cause a button press or release event.
 *
 * @param[in] showWidget       whether to show Gtk widget
 * @param[in] buttonEvent      whether to send a button event
 * @param[in] buttonPress      whether to press or release mouse
 * @param[in] moveWindow:      whether to move our window too
 * @param[in] coordsProvided   whether coordinates provided
 * @param[in] xCoord           x coordinate
 * @param[in] yCoord           y coordinate
 *
 * @return true on success, false on failure.
 */

bool
DnDUI::DnDFakeXEvents(const bool showWidget,
                      const bool buttonEvent,
                      const bool buttonPress,
                      const bool moveWindow,
                      const bool coordsProvided,
                      const int xCoord,
                      const int yCoord)
{
   GtkWidget *widget;
   Window rootWnd;
   bool ret;
   Display *dndXDisplay;
   Window dndXWindow;
   int x;
   int y;

   x = xCoord;
   y = yCoord;

   Debug("%s: enter\n", __FUNCTION__);
   widget = GetDetWndAsWidget(false);

   if (!widget) {
      Debug("%s: unable to get widget\n", __FUNCTION__);
      return false;
   }

   dndXDisplay = GDK_WINDOW_XDISPLAY(widget->window);
   dndXWindow = GDK_WINDOW_XWINDOW(widget->window);

   /*
    * Turn on X synchronization in order to ensure that our X events occur in
    * the order called.  In particular, we want the window movement to occur
    * before the mouse movement so that the events we are coercing do in fact
    * happen.
    */
   XSynchronize(dndXDisplay, True);

   if (showWidget) {
      Debug("%s: showing Gtk widget\n", __FUNCTION__);
      gtk_widget_show(widget);
      gdk_window_show(widget->window);
   }

   /* Get the current location of the mouse if coordinates weren't provided. */
   if (!coordsProvided) {
      Window rootReturn;
      Window childReturn;
      int rootXReturn;
      int rootYReturn;
      int winXReturn;
      int winYReturn;
      unsigned int maskReturn;

      rootWnd = RootWindow(dndXDisplay, DefaultScreen(dndXDisplay));
      ret = XQueryPointer(dndXDisplay, rootWnd, &rootReturn, &childReturn,
                          &rootXReturn, &rootYReturn, &winXReturn, &winYReturn,
                          &maskReturn);
      if (ret == False) {
         Warning("%s: XQueryPointer() returned False.\n", __FUNCTION__);
         XSynchronize(dndXDisplay, False);
         return false;
      }

      Debug("%s: mouse is at (%d, %d)\n", __FUNCTION__, rootXReturn, rootYReturn);

      x = rootXReturn;
      y = rootYReturn;
   }

   if (moveWindow) {
      /*
       * Make sure the window is at this point and at the top (raised).  The
       * window is resized to be a bit larger than we would like to increase
       * the likelihood that mouse events are attributed to our window -- this
       * is okay since the window is invisible and hidden on cancels and DnD
       * finish.
       */
      XMoveResizeWindow(dndXDisplay, dndXWindow, x, y, 25, 25);
      XRaiseWindow(dndXDisplay, dndXWindow);
   }

   /*
    * Generate mouse movements over the window.  The second one makes ungrabs
    * happen more reliably on KDE, but isn't necessary on GNOME.
    */
   XTestFakeMotionEvent(dndXDisplay, -1, x, y, CurrentTime);
   XTestFakeMotionEvent(dndXDisplay, -1, x + 1, y + 1, CurrentTime);

   if (buttonEvent) {
      Debug("%s: faking left mouse button %s\n", __FUNCTION__,
            buttonPress ? "press" : "release");
      XTestFakeButtonEvent(dndXDisplay, 1, buttonPress, CurrentTime);
   }

   XSynchronize(dndXDisplay, False);
   return true;
}


/**
 *
 * Get the GtkWidget pointer for a DragDetWnd object. The X11 Unity
 * implementation requires access to the drag detection window as
 * a GtkWindow pointer, which it uses to show and hide the detection
 * window. This function is also called by the code that issues fake
 * X events to the detection window.
 *
 * @param[in] full true to get the GtkWindow for the full (unity)
 * detection window, otherwise false.
 *
 * @return a pointer to the GtkWidget for the detection window, or NULL
 * on failure.
 */

GtkWidget *
DnDUI::GetDetWndAsWidget(const bool full)
{
   GtkInvisible *window;
   GtkWidget *widget = NULL;
   DragDetWnd *detWnd;

   if (full) {
      detWnd = m_detWndFull;
   } else {
      detWnd = m_detWnd;
   }

   if (!detWnd) {
      return NULL;
   }
   window = detWnd->gobj();
   if (window) {
      widget = GTK_WIDGET(window);
   }
   return widget;
}


/**
 *
 * Add a block for the current H->G file transfer. Must be paired with a
 * call to RemoveBlock() on finish or cancellation.
 */

void
DnDUI::AddBlock()
{
   if (DnD_BlockIsReady(m_blockCtrl) && m_blockCtrl->AddBlock(m_blockCtrl->fd, m_HGStagingDir.c_str())) {
      m_blockAdded = true;
      Debug("%s: add block for %s.\n", __FUNCTION__, m_HGStagingDir.c_str());
   } else {
      m_blockAdded = false;
      Debug("%s: unable to add block dir %s.\n", __FUNCTION__, m_HGStagingDir.c_str());
   }
}


/**
 *
 * Remove block for the current H->G file transfer. Must be paired with a
 * call to AddBlock(), but it will only attempt to remove block if one is
 * currently in effect.
 */

void
DnDUI::RemoveBlock()
{
   if (m_blockAdded && !m_HGGetDataInProgress) {
      Debug("%s: removing block for %s\n", __FUNCTION__, m_HGStagingDir.c_str());
      m_blockCtrl->RemoveBlock(m_blockCtrl->fd, m_HGStagingDir.c_str());
      m_blockAdded = false;
   }
}


/**
 *
 * Convert a Gdk::DragAction value to its corresponding DND_DROPEFFECT.
 *
 * @param[in] the Gdk::DragAction value to return.
 *
 * @return the corresponding DND_DROPEFFECT, with DROP_UNKNOWN returned
 * if no mapping is supported.
 *
 * @note DROP_NONE is not mapped in this function.
 */

DND_DROPEFFECT
DnDUI::ToDropEffect(Gdk::DragAction action)
{
   DND_DROPEFFECT effect;

   switch(action) {
   case Gdk::ACTION_COPY:
      effect = DROP_COPY;
      break;
   case Gdk::ACTION_MOVE:
      effect = DROP_MOVE;
      break;
   case Gdk::ACTION_LINK:
      effect = DROP_LINK;
      break;
   case Gdk::ACTION_PRIVATE:
   case Gdk::ACTION_DEFAULT:
   case Gdk::ACTION_ASK:
   default:
      effect = DROP_UNKNOWN;
      break;
   }
   return effect;
}



