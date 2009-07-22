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
 * dndRpc.hh --
 *
 *     Rpc layer object for DnD.
 */

#ifndef DND_RPC_HH
#define DND_RPC_HH

#include <sigc++/connection.h>
#include <vector>
#include "vm_basic_types.h"

extern "C" {
   #include "dndClipboard.h"
}

class DnDRpc
{
   public:
      virtual ~DnDRpc(void) {};

      /* sigc signals for callback. */
      sigc::signal<void, bool, uint32> ghUpdateUnityDetWndChanged;
      sigc::signal<void, int, int> ghQueryPendingDragChanged;
      sigc::signal<void, int32, int32>ghPrivateDropChanged;
      sigc::signal<void> ghCancelChanged;
      sigc::signal<void, const CPClipboard*> hgDragEnterChanged;
      sigc::signal<void> hgDragStartChanged;
      sigc::signal<void> hgCancelChanged;
      sigc::signal<void> hgDropChanged;
      sigc::signal<void, bool, const std::vector<uint8> > hgFileCopyDoneChanged;
      sigc::signal<void, int32, int32> updateMouseChanged;
      sigc::signal<void> moveDetWndToMousePos;

      /* GH DnD. */
      virtual bool GHDragEnter(const CPClipboard *clip) = 0;
      virtual bool GHUngrabTimeout(void) = 0;

      /* HG DnD. */
      virtual bool HGDragEnterDone(int32 x, int32 y) = 0;
      virtual bool HGDragStartDone(void) = 0;
      virtual bool HGUpdateFeedback(DND_DROPEFFECT feedback) = 0;
      virtual bool HGDropDone(const char *stagingDirCP, size_t sz) = 0;

   protected:
      uint32 mHostMinorVersion;
      uint32 mGuestMinorVersion;
};

#endif // DND_RPC_HH
