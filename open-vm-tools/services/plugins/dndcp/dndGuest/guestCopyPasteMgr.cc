/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * @guestCopyPasteMgr.cc --
 *
 * Implementation of common layer GuestCopyPasteMgr object for guest.
 */

#include "guestCopyPaste.hh"
#include "copyPasteRpcV3.hh"
#include "copyPasteRpcV4.hh"
#include "guestDnDCPMgr.hh"

extern "C" {
   #include "debug.h"
}


/**
 * Constructor.
 *
 * @param[in] transport for sending/receiving packets.
 */

GuestCopyPasteMgr::GuestCopyPasteMgr(DnDCPTransport *transport)
 : mSrc(NULL),
   mDest(NULL),
   mRpc(NULL),
   mCPState(GUEST_CP_READY),
   mTransport(transport),
   mSessionId(0),
   mCopyPasteAllowed(false),
   mResolvedCaps(0xffffffff)
{
   ASSERT(transport);
}


/**
 * Destructor.
 */

GuestCopyPasteMgr::~GuestCopyPasteMgr(void)
{
   delete mRpc;
   mRpc = NULL;
}


/**
 * Reset state machine and session id. Delete mSrc and mDest.
 */

void
GuestCopyPasteMgr::ResetCopyPaste(void)
{
   Debug("%s: state %d, session id %d before reset\n",
         __FUNCTION__, mCPState, mSessionId);
   if (mSrc) {
      delete mSrc;
      mSrc = NULL;
   }
   if (mDest) {
      delete mDest;
      mDest = NULL;
   }
   SetState(GUEST_CP_READY);
   SetSessionId(0);
   Debug("%s: change to state %d, session id %d\n",
         __FUNCTION__, mCPState, mSessionId);
}


/**
 * Got valid clipboard data from host. Create mSrc if the state machine
 * is ready.
 *
 * @param[in] sessionId active session id
 * @param[in] isActive active or passive CopyPaste.
 * @param[in] clip cross-platform clipboard data.
 */

void
GuestCopyPasteMgr::OnRpcSrcRecvClip(uint32 sessionId,
                                    bool isActive,
                                    const CPClipboard *clip)
{
   ASSERT(clip);

   Debug("%s: enter\n", __FUNCTION__);

   if (!mCopyPasteAllowed) {
      Debug("%s: CopyPaste is not allowed.\n", __FUNCTION__);
      return;
   }

   if  (GUEST_CP_READY != mCPState) {
      Debug("%s: Bad state: %d, reset\n", __FUNCTION__, mCPState);
      /* XXX Should reset DnD here. */
      return;
   }

   if (mSrc) {
      Debug("%s: mSrc is not NULL\n", __FUNCTION__);
      delete mSrc;
      mSrc = NULL;
   }

   mSessionId = sessionId;
   Debug("%s: change sessionId to %d\n", __FUNCTION__, mSessionId);

   mSrc = new GuestCopyPasteSrc(this);

   mSrc->OnRpcRecvClip(isActive, clip);
}


/**
 * Wrapper for mSrc->UIRequestFiles.
 *
 * @param[in] dir staging directory in local format.
 *
 * @return The staging directory if succeed, otherwise empty string.
 */

const std::string
GuestCopyPasteMgr::SrcUIRequestFiles(const std::string &dir)
{
   if (mSrc) {
      return mSrc->UIRequestFiles(dir);
   } else {
      Debug("%s: mSrc is NULL\n", __FUNCTION__);
      return std::string("");
   }
}


/**
 * Host is asking for clipboard data. Create mDest if the state machine
 * is ready.
 *
 * @param[in] sessionId active session id
 * @param[in] isActive active or passive CopyPaste.
 */

void
GuestCopyPasteMgr::OnRpcDestRequestClip(uint32 sessionId,
                                        bool isActive)
{
   Debug("%s: enter\n", __FUNCTION__);

   if (!mCopyPasteAllowed) {
      Debug("%s: CopyPaste is not allowed.\n", __FUNCTION__);
      return;
   }

   if  (GUEST_CP_READY != mCPState) {
      Debug("%s: Bad state: %d, reset\n", __FUNCTION__, mCPState);
      /* XXX Should reset CP here. */
      return;
   }

   if (mDest) {
      Debug("%s: mDest is not NULL\n", __FUNCTION__);
      delete mDest;
      mDest = NULL;
   }

   mSessionId = sessionId;
   Debug("%s: change sessionId to %d\n", __FUNCTION__, mSessionId);

   mDest = new GuestCopyPasteDest(this);

   mDest->OnRpcRequestClip(isActive);
}


/**
 * Wrapper for mDest->UISendClip.
 *
 * @param[in] clip cross-platform clipboard data.
 */

void
GuestCopyPasteMgr::DestUISendClip(const CPClipboard *clip)
{
   if (mDest) {
      mDest->UISendClip(clip);
   } else {
      Debug("%s: mDest is NULL\n", __FUNCTION__);
   }
}


/**
 * Handle version change in VMX.
 *
 * @param[in] version negotiated CP version.
 */

void
GuestCopyPasteMgr::VmxCopyPasteVersionChanged(uint32 version)
{
   Debug("GuestCopyPasteMgr::%s: enter version %d\n", __FUNCTION__, version);
   ASSERT(version >= 3);
   ASSERT(mTransport);

   if (mRpc) {
      delete mRpc;
   }

   switch(version) {
   case 4:
      mRpc = new CopyPasteRpcV4(mTransport);
      break;
   case 3:
      mRpc = new CopyPasteRpcV3(mTransport);
      break;
   default:
      g_debug("%s: unsupported CP version\n", __FUNCTION__);
      break;
   }
   if (mRpc) {
      Debug("GuestCopyPasteMgr::%s: register ping reply changed %d\n",
            __FUNCTION__, version);
      mRpc->pingReplyChanged.connect(
         sigc::mem_fun(this, &GuestCopyPasteMgr::OnPingReply));
      mRpc->srcRecvClipChanged.connect(
         sigc::mem_fun(this, &GuestCopyPasteMgr::OnRpcSrcRecvClip));
      mRpc->destRequestClipChanged.connect(
         sigc::mem_fun(this, &GuestCopyPasteMgr::OnRpcDestRequestClip));
      mRpc->Init();
      mRpc->SendPing(GuestDnDCPMgr::GetInstance()->GetCaps() &
                     (DND_CP_CAP_CP | DND_CP_CAP_FORMATS_CP | DND_CP_CAP_VALID));
   }

   ResetCopyPaste();
}


/**
 * Check if a request is allowed based on resolved capabilities.
 *
 * @param[in] capsRequest requested capabilities.
 *
 * @return TRUE if allowed, FALSE otherwise.
 */

Bool
GuestCopyPasteMgr::CheckCapability(uint32 capsRequest)
{
   Bool allowed = FALSE;

   if ((mResolvedCaps & capsRequest) == capsRequest) {
      allowed = TRUE;
   }
   return allowed;
}


/**
 * Got pingReplyChanged message. Update capabilities.
 *
 * @param[in] capability modified capabilities from VMX controller.
 */

void
GuestCopyPasteMgr::OnPingReply(uint32 capabilities)
{
   Debug("%s: copypaste ping reply caps are %x\n", __FUNCTION__, capabilities);
   mResolvedCaps = capabilities;
}

