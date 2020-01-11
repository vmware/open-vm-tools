/*********************************************************
 * Copyright (C) 2010-2018 VMware, Inc. All rights reserved.
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
 * @file copyPasteDnDX11.h
 *
 * This class provides the concrete UI implementation for the DnD and
 * copy paste abstraction, for the X11 platform.
 */

#ifndef __COPYPASTEDNDX11_H__
#define __COPYPASTEDNDX11_H__

#include "dnd.h"     /* for DnDBlockControl */
#include "dndUIX11.h"
#include <gtk/gtk.h>
#include "vm_basic_types.h"
#include "copyPasteDnDImpl.h"

extern "C" {
void CopyPasteDnDWrapper_SetUnityMode(Bool mode);
}

class CopyPasteUIX11;

class CopyPasteDnDX11 : public CopyPasteDnDImpl
{
public:
   CopyPasteDnDX11();
   ~CopyPasteDnDX11();
   virtual gboolean Init(ToolsAppCtx *ctx);
   virtual void PointerInit();
   virtual gboolean RegisterCP();
   virtual void UnregisterCP();
   virtual gboolean RegisterDnD();
   virtual void UnregisterDnD();
   virtual void DnDVersionChanged(int version);
   virtual void CopyPasteVersionChanged(int version);
   virtual uint32 GetCaps();
   void SetUnityMode(Bool mode) {m_dndUI->SetUnityMode(mode);};
   void SetDnDAllowed(bool allowed);
   void SetCopyPasteAllowed(bool allowed);
private:
   Gtk::Main *m_main;
   CopyPasteUIX11 *m_copyPasteUI;
   DnDUIX11 *m_dndUI;
};

#endif // __COPYPASTEDNDX11_H__
