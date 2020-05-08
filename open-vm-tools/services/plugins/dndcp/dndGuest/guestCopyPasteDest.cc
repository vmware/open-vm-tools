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
 * @guestCopyPasteDest.cc --
 *
 * Implementation of common layer GuestCopyPasteDest object for guest.
 */

#include "guestCopyPaste.hh"

extern "C" {
   #include <glib.h>

   #include "dndClipboard.h"
   #include "debug.h"
}


/**
 * Constructor.
 *
 * @param[in] mgr guest CP manager
 */

GuestCopyPasteDest::GuestCopyPasteDest(GuestCopyPasteMgr *mgr)
 : mMgr(mgr),
   mIsActive(false)
{
   ASSERT(mMgr);
}


/**
 * Got valid clipboard data from UI. Send sendClip cmd to controller.
 *
 * @param[in] clip cross-platform clipboard data.
 */

void
GuestCopyPasteDest::UISendClip(const CPClipboard *clip)
{
   ASSERT(clip);

   g_debug("%s: state is %d\n", __FUNCTION__, mMgr->GetState());
   if (mMgr->GetState() != GUEST_CP_READY) {
      /* Reset DnD for any wrong state. */
      g_debug("%s: Bad state: %d\n", __FUNCTION__, mMgr->GetState());
      goto error;
   }

   if (!mMgr->GetRpc()->DestSendClip(mMgr->GetSessionId(), mIsActive, clip)) {
      g_debug("%s: DestSendClip failed\n", __FUNCTION__);
      goto error;
   }

   return;

error:
   mMgr->ResetCopyPaste();
}


/**
 * Host is asking for clipboard data. Emit destRequestClipChanged signal.
 *
 * @param[in] isActive active or passive CopyPaste.
 */

void
GuestCopyPasteDest::OnRpcRequestClip(bool isActive)
{
   mIsActive = isActive;
   g_debug("%s: state is %d\n", __FUNCTION__, mMgr->GetState());
   mMgr->destRequestClipChanged.emit();
}

