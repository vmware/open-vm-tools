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
 * @file copyPasteDnDX11.cpp
 *
 * Implementation class for DnD and copy paste on X11 platform.
 */

#define G_LOG_DOMAIN "dndcp"

#include "copyPasteDnDWrapper.h"
#include "copyPasteDnDX11.h"
#include "dndPluginIntX11.h"

Window gXRoot;
Display *gXDisplay = NULL;
GtkWidget *gUserMainWidget = NULL;

extern "C" {
#include "copyPasteCompat.h"
#include "dndGuest.h"
#if defined(NOT_YET)
#include "unity.h"
#endif

void CopyPaste_Register(GtkWidget *mainWnd, ToolsAppCtx *ctx);
void CopyPaste_Unregister(GtkWidget *mainWnd);
}

#include "pointer.h"

extern "C" {

/**
 *
 * Enter or leave unity mode.
 *
 * @param[in] mode enter unity mode if TRUE, else leave.
 */

#if defined(NOT_YET)
void
CopyPasteDnDX11_SetUnityMode(Bool mode)
{
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();
   ASSERT(wrapper);

   wrapper->SetUnityMode(mode);
}
#endif
}

/**
 *
 * Constructor.
 */

CopyPasteDnDX11::CopyPasteDnDX11() :
   m_copyPasteUI(NULL),
   m_dndUI(NULL)
{
}


/**
 *
 * Initialize Win32 platform DnD/CP. Initialize Gtk+, and create detection
 * windows.
 *
 * @param[in] ctx tools app context.
 */

gboolean
CopyPasteDnDX11::Init(ToolsAppCtx *ctx)
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();

   ASSERT(ctx);
   int argc = 1;
   char *argv[] = {"", NULL};
   m_main = new Gtk::Main(&argc, (char ***) &argv, false);

   if (wrapper) {
      m_blockCtrl.fd = ctx->blockFD;
      m_blockCtrl.fd >= 0 ?
         DnD_CompleteBlockInitialization(m_blockCtrl.fd, &m_blockCtrl) :
         DnD_InitializeBlocking(&m_blockCtrl);
   }

   gUserMainWidget = gtk_invisible_new();
   gXDisplay = GDK_WINDOW_XDISPLAY(gUserMainWidget->window);
   gXRoot = RootWindow(gXDisplay, DefaultScreen(gXDisplay));

   /*
    * Register legacy (backdoor) version of copy paste.
    */
   CopyPaste_SetVersion(1);
   CopyPaste_Register(gUserMainWidget, ctx);
   return true;
}


/**
 *
 * Destructor.
 */

CopyPasteDnDX11::~CopyPasteDnDX11()
{
   if (m_copyPasteUI) {
      delete m_copyPasteUI;
   }
   if (m_dndUI) {
      delete m_dndUI;
   }
   if (m_main) {
      delete m_main;
   }

   /*
    * Legacy CP.
    */
   CopyPaste_Unregister(gUserMainWidget);

   if (gUserMainWidget) {
      gtk_widget_destroy(gUserMainWidget);
   }
}


/**
 *
 * Register copy and paste capabilities with the VMX.
 *
 * @return TRUE on success, FALSE on failure
 */

gboolean
CopyPasteDnDX11::RegisterCP()
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();

   if (wrapper->IsCPRegistered()) {
      return TRUE;
   }

   if (!wrapper->IsCPEnabled()) {
      return FALSE;
   }

   m_copyPasteUI = new CopyPasteUIX11();
   if (m_copyPasteUI) {
      if (m_copyPasteUI->Init()) {
         m_copyPasteUI->SetBlockControl(&m_blockCtrl);
         wrapper->SetCPIsRegistered(TRUE);
         int version = wrapper->GetCPVersion();
         g_debug("%s: version is %d\n", __FUNCTION__, version);

         if (version >= 3) {
            CopyPasteVersionChanged(version);
            m_copyPasteUI->SetCopyPasteAllowed(TRUE);
         } else {
            /*
             * Initialize legacy version.
             */
            CopyPaste_SetVersion(version);
         }
      } else {
         delete m_copyPasteUI;
         m_copyPasteUI = NULL;
      }
   }
   return wrapper->IsCPRegistered();
}


/**
 *
 * Register DnD capabilities with the VMX.
 *
 * @return TRUE on success, FALSE on failure
 */

gboolean
CopyPasteDnDX11::RegisterDnD()
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();

   if (!wrapper->IsDnDEnabled()) {
      return FALSE;
   }

   if (!wrapper->IsDnDRegistered()) {
      ToolsAppCtx *ctx = wrapper->GetToolsAppCtx();
      m_dndUI = new DnDUIX11(ctx);
      if (m_dndUI) {
         m_dndUI->SetBlockControl(&m_blockCtrl);
         if (m_dndUI->Init()) {
#if defined(NOT_YET)
            UnityDnD state;
            state.detWnd = m_dndUI->GetDetWndAsWidget();
            state.setMode = CopyPasteDnDX11_SetUnityMode;
            Unity_SetActiveDnDDetWnd(&state);
#endif
            wrapper->SetDnDIsRegistered(TRUE);
            m_dndUI->SetDnDAllowed(TRUE);
            int version = wrapper->GetDnDVersion();
            g_debug("%s: dnd version is %d\n", __FUNCTION__, version);
            if (version >= 3) {
               DnDVersionChanged(version);
            }
         } else {
            delete m_dndUI;
            m_dndUI = NULL;
         }
      }
   }

   g_debug("%s: dnd is registered? %d\n", __FUNCTION__, (int) wrapper->IsDnDRegistered());
   return wrapper->IsDnDRegistered();
}


/**
 *
 * Unregister copy paste capabilities and do general cleanup.
 */

void
CopyPasteDnDX11::UnregisterCP()
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();
   if (wrapper->IsCPRegistered()) {
      if (m_copyPasteUI) {
         delete m_copyPasteUI;
         m_copyPasteUI = NULL;
      }
      wrapper->SetCPIsRegistered(FALSE);
      wrapper->SetCPVersion(-1);
   }
}


/**
 *
 * Unregister DnD capabilities and do general cleanup.
 */

void
CopyPasteDnDX11::UnregisterDnD()
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();
   if (wrapper->IsDnDRegistered()) {
      /*
       * Detach the DnD detection window from Unity.
       */
#if defined(NOT_YET)
      UnityDnD state = { NULL, NULL };
      Unity_SetActiveDnDDetWnd(&state);
#endif

      if (m_dndUI) {
         delete m_dndUI;
         m_dndUI = NULL;
      }
      wrapper->SetDnDIsRegistered(false);
      wrapper->SetDnDVersion(-1);
      return;
   }
}


/**
 *
 * Communicate dnd allowed to platform implementation.
 *
 * @param[allowed] if TRUE, dnd allowed.
 */

void
CopyPasteDnDX11::SetDnDAllowed(bool allowed)
{
   ASSERT(m_dndUI);
   g_debug("%s: enter\n", __FUNCTION__);
   m_dndUI->SetDnDAllowed(allowed);
}


/**
 *
 * Communicate copypaste allowed to platform implementation.
 *
 * @param[allowed] if TRUE, copy paste allowed.
 */

void
CopyPasteDnDX11::SetCopyPasteAllowed(bool allowed)
{
   ASSERT(m_copyPasteUI);
   g_debug("%s: enter\n", __FUNCTION__);
   m_copyPasteUI->SetCopyPasteAllowed(allowed);
}


/**
 * Communicate copy paste version change to the platform implementation.
 *
 * @param[in] version the new version
 */

void
CopyPasteDnDX11::CopyPasteVersionChanged(int version)
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();
   ToolsAppCtx *ctx = wrapper->GetToolsAppCtx();
   g_debug("%s: calling VmxCopyPasteVersionChanged (version %d)\n",
          __FUNCTION__, version);
   if (ctx) {
      m_copyPasteUI->VmxCopyPasteVersionChanged(ctx->rpc, version);
   }
}


/**
 * Communicate DnD version change by calling the platform implementation.
 *
 * @param[in] version the new version.
 */

void
CopyPasteDnDX11::DnDVersionChanged(int version)
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();
   ToolsAppCtx *ctx = wrapper->GetToolsAppCtx();
   g_debug("%s: calling VmxDnDVersionChanged (version %d)\n",
          __FUNCTION__, version);
   ASSERT(ctx);
   ASSERT(m_dndUI);
   m_dndUI->VmxDnDVersionChanged(ctx->rpc, version);
}


/**
 *
 * Initialize pointer code.
 */

void
CopyPasteDnDX11::PointerInit()
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();

   ASSERT(wrapper);

   ToolsAppCtx *ctx = wrapper->GetToolsAppCtx();
   ASSERT(ctx);

   Pointer_Init(ctx);
}
