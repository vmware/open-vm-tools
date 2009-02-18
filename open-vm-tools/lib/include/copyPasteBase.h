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
 * copyPasteBase.h --
 *
 *     Base object for CopyPaste. This is the common interface between UI and 
 *     cui CopyPaste protocol layer. Both host side and guest side, and also
 *     different platforms, share this same interface.
 */

#ifndef COPYPASTE_BASE_H
#define COPYPASTE_BASE_H

#include <sigc++/connection.h>

#ifdef VMX86_TOOLS
   /*
    * LIB_EXPORT definition is not needed on guest. 
    */
#   define LIB_EXPORT
#else
   /*
    * The interface class on host side for Windows is in dll which needs
    * LIB_EXPORT definition. 
    */
#   include "libExport.hh"
#endif

extern "C" {
   #include "vm_basic_types.h"
   #include "dnd.h"
}

class LIB_EXPORT CopyPasteBase
{
   public:
      enum COPYPASTE_STATE {
         CPSTATE_INVALID = 0,
         CPSTATE_READY,
         CPSTATE_REQUESTING_CLIPBOARD,
      };

      virtual ~CopyPasteBase(void) {};

      /* sigc signals for local UI callbacks. */
      /* Local UI as CopyPaste source. */
      sigc::signal<void, const CPClipboard*> newClipboard;
      /* Local UI as CopyPaste target. */
      sigc::signal<bool, CPClipboard*> localGetClipboard;
      sigc::signal<void, bool> localGetFilesDoneChanged;

      /* cui CopyPaste protocol layer API exposed to UI (all platforms). */
      /* Local UI as CopyPaste source. */
      virtual bool SetRemoteClipboard(const CPClipboard *clip) = 0;

      /* Local UI as CopyPaste target. */
      virtual bool GetRemoteClipboard(void) = 0;

      virtual bool IsCopyPasteAllowed(void) = 0;

   protected:
      COPYPASTE_STATE mState;
};

#endif // COPYPASTE_BASE_H

