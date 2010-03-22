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
 * @file copyPasteDnDWrapper.h
 *
 * This singleton class implements a wrapper around various versions of
 * copy and paste and dnd versions for Linux.
 *
 */

#ifndef COPYPASTEDNDWRAPPER_H
#define COPYPASTEDNDWRAPPER_H

extern "C" {
#include "dnd.h"     /* for DnDBlockControl */
}

#if defined(HAVE_GTKMM)
#include "copyPasteUI.h"
#include "dndUI.h"
#endif

#include "vm_basic_types.h"
#include <gtk/gtk.h>

struct DblLnkLst_Links;

extern "C" {
void CopyPasteDnDWrapper_SetUnityMode(Bool mode);
}

class CopyPasteDnDWrapper
{
public:
   ~CopyPasteDnDWrapper();
   static CopyPasteDnDWrapper *GetInstance();
   bool RegisterCP();
   void UnregisterCP();
   bool RegisterDnD();
   void UnregisterDnD();
   int GetCPVersion();
   int GetDnDVersion();
   void SetCPIsRegistered(bool isRegistered);
   bool IsCPRegistered();
   void SetDnDIsRegistered(bool isRegistered);
   bool IsDnDRegistered();
   void OnReset();
   void Cancel();
   void SetBlockControl(DnDBlockControl *blockCtrl);
   void SetUserData(const void *userData);
   void SetHGWnd(GtkWidget *wnd) {m_hgWnd = wnd;};
   void SetGHWnd(GtkWidget *wnd) {m_ghWnd = wnd;};
   void SetEventQueue(DblLnkLst_Links *queue) {m_eventQueue = queue;};
#if defined(HAVE_GTKMM)
   void SetUnityMode(Bool mode)
      {m_dndUI->SetUnityMode(mode);};
#endif
private:
   /*
    * We're a singleton, so it is a compile time error to call these.
    */
   CopyPasteDnDWrapper();
   CopyPasteDnDWrapper(const CopyPasteDnDWrapper &wrapper);
   CopyPasteDnDWrapper& operator=(const CopyPasteDnDWrapper &wrapper);
private:
#if defined(HAVE_GTKMM)
   CopyPasteUI *m_copyPasteUI;
   DnDUI *m_dndUI;
#endif
   bool m_isCPRegistered;
   bool m_isDnDRegistered;
   const void *m_userData;
   int m_cpVersion;
   int m_dndVersion;
   static CopyPasteDnDWrapper *m_instance;
   DnDBlockControl *m_blockCtrl;
   bool m_isLegacy;
   GtkWidget *m_hgWnd;
   GtkWidget *m_ghWnd;
   DblLnkLst_Links *m_eventQueue;
};

#endif // COPYPASTEDNDWRAPPER_H
