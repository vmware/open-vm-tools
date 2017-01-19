/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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

#include "tracer.hh"
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
   TRACE_CALL();

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
}


/**
 * Session ID change and bookkeeping.
 *
 * @param[in] id Next session ID.
 */
void
GuestCopyPasteMgr::SetSessionId(uint32 id)
{
   DEVEL_ONLY(g_debug("%s: %u => %u\n", __FUNCTION__, mSessionId, id));
   mSessionId = id;
}


/**
 * State change and bookkeeping.
 *
 * @param[in] state Next state.
 */

void
GuestCopyPasteMgr::SetState(GUEST_CP_STATE state)
{
#ifdef VMX86_DEVEL
   static const char* states[] = {
      "GUEST_CP_INVALID",
      "GUEST_CP_READY",
      "GUEST_CP_HG_FILE_COPYING",
   };
   g_debug("%s: %s => %s\n", __FUNCTION__, states[mCPState], states[state]);
#endif

   mCPState = state;
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

   TRACE_CALL();

   if (!mCopyPasteAllowed) {
      g_debug("%s: CopyPaste is not allowed.\n", __FUNCTION__);
      return;
   }

   if  (GUEST_CP_READY != mCPState) {
      g_debug("%s: Bad state: %d, reset\n", __FUNCTION__, mCPState);
      /* XXX Should reset DnD here. */
      return;
   }

   if (mSrc) {
      g_debug("%s: mSrc is not NULL\n", __FUNCTION__);
      delete mSrc;
      mSrc = NULL;
   }

   SetSessionId(sessionId);

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
      g_debug("%s: mSrc is NULL\n", __FUNCTION__);
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
   TRACE_CALL();

   if (!mCopyPasteAllowed) {
      g_debug("%s: CopyPaste is not allowed.\n", __FUNCTION__);
      return;
   }

   if  (GUEST_CP_READY != mCPState) {
      g_debug("%s: Bad state: %d, reset\n", __FUNCTION__, mCPState);
      /* XXX Should reset CP here. */
      return;
   }

   if (mDest) {
      g_debug("%s: mDest is not NULL\n", __FUNCTION__);
      delete mDest;
      mDest = NULL;
   }

   SetSessionId(sessionId);

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
      g_debug("%s: mDest is NULL\n", __FUNCTION__);
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
   g_debug("GuestCopyPasteMgr::%s: enter version %d\n", __FUNCTION__, version);
   ASSERT(version >= 3);
   ASSERT(mTransport);

   if (mRpc) {
      delete mRpc;
      mRpc = NULL;
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
      g_debug("GuestCopyPasteMgr::%s: register ping reply changed %d\n",
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
   g_debug("%s: copypaste ping reply caps are %x\n", __FUNCTION__, capabilities);
   mResolvedCaps = capabilities;
}

