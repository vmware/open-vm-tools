/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
#include "copyPasteUIX11.h"
#include "dndPluginIntX11.h"
#include "tracer.hh"

Window gXRoot;
Display *gXDisplay;
GtkWidget *gUserMainWidget;


extern "C" {
#include "copyPasteCompat.h"
#include "vmware/tools/plugin.h"

void CopyPaste_Register(GtkWidget *mainWnd, ToolsAppCtx *ctx);
void CopyPaste_Unregister(GtkWidget *mainWnd);
}

#include "pointer.h"


/**
 *
 * BlockingService - a singleton class responsible for initializing and
 * cleaning up blocking state (vmblock).
 */

class BlockService {
public:
   static BlockService *GetInstance();
   void Init(ToolsAppCtx *);
   DnDBlockControl *GetBlockCtrl() { return &m_blockCtrl; }

private:
   BlockService();
   ~BlockService();

   void Shutdown();
   static gboolean ShutdownSignalHandler(const siginfo_t *, gpointer);

   GSource *m_shutdownSrc;
   DnDBlockControl m_blockCtrl;
   bool m_initialized;

   static BlockService *m_instance;
};

BlockService *BlockService::m_instance = 0;


/**
 *
 * Constructor.
 */

BlockService::BlockService() :
   m_shutdownSrc(0),
   m_initialized(false)
{
   memset(&m_blockCtrl, 0, sizeof m_blockCtrl);
   m_blockCtrl.fd = -1;
}


/**
 *
 * Get an instance of BlockService, which is an application singleton.
 *
 * @return a pointer to the singleton BlockService object, or NULL if
 * for some reason it could not be allocated.
 */

BlockService *
BlockService::GetInstance()
{
   TRACE_CALL();

   if (!m_instance) {
      m_instance = new BlockService();
   }

   ASSERT(m_instance);
   return m_instance;
}


/**
 *
 * Initialize blocking subsystem so that GTK+ DnD operations won't
 * time out. Also install SIGUSR1 handler so we can disconnect from
 * blcoing subsystem upon request.
 *
 * @param[in] ctx tools app context.
 */

void
BlockService::Init(ToolsAppCtx *ctx)
{
   TRACE_CALL();

   if (!m_initialized && ctx) {
      m_blockCtrl.fd = ctx->blockFD;
      m_blockCtrl.fd >= 0 ?
         DnD_CompleteBlockInitialization(m_blockCtrl.fd, &m_blockCtrl) :
         DnD_InitializeBlocking(&m_blockCtrl);

      m_shutdownSrc = VMTools_NewSignalSource(SIGUSR1);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, m_shutdownSrc, ShutdownSignalHandler,
                               ctx, NULL);

      m_initialized = true;
   }
}


/**
 *
 * Signal handler called when we receive SIGUSR1 which is a hint for us
 * to disconnect from blocking subsystem so that it can be upgraded.
 *
 * @param[in] siginfo unused.
 * @param[in] data    unused.
 *
 * @return always TRUE.
 */

gboolean
BlockService::ShutdownSignalHandler(const siginfo_t *siginfo,
                                    gpointer data)
{
   TRACE_CALL();

   g_debug("Shutting down block service on SIGUSR1 ...\n");
   GetInstance()->Shutdown();

   return FALSE;
}


/**
 *
 * Shut down blocking susbsystem so that we can perform upgrade.
 */

void
BlockService::Shutdown()
{
   TRACE_CALL();

   if (m_initialized) {
      g_source_destroy(m_shutdownSrc);
      g_source_unref(m_shutdownSrc);
      m_shutdownSrc = 0;

      if (DnD_BlockIsReady(&m_blockCtrl)) {
         DnD_UninitializeBlocking(&m_blockCtrl);
      }

      m_initialized = false;
   }
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
   TRACE_CALL();

#if GTK_MAJOR_VERSION > 3 || (GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION >= 10)
   /*
    * On recent distros, Wayland is the default display server. If the obtained
    * display or window is a wayland one, applying X11 specific functions on them
    * will result in crashes. Before migrating the X11 specific code to Wayland,
    * force using X11 as the backend of Gtk+3. gdk_set_allowed_backends() is
    * introduced since Gtk+3.10 and Wayland is supported from Gtk+3.10.
    */
   gdk_set_allowed_backends("x11");
#endif

   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();

   ASSERT(ctx);
   int argc = 1;
   const char *argv[] = {"", NULL};
   m_main = new Gtk::Main(&argc, (char ***) &argv, false);

   if (wrapper) {
      BlockService::GetInstance()->Init(ctx);
   }

   gUserMainWidget = gtk_invisible_new();
#ifndef GTK3
   gXDisplay = GDK_WINDOW_XDISPLAY(gUserMainWidget->window);
#else
   gXDisplay = GDK_WINDOW_XDISPLAY(gtk_widget_get_window(gUserMainWidget));
#endif
   gXRoot = RootWindow(gXDisplay, DefaultScreen(gXDisplay));

   /*
    * Register legacy (backdoor) version of copy paste.
    */
   CopyPaste_SetVersion(1);
   CopyPaste_Register(gUserMainWidget, ctx);

   return TRUE;
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
   TRACE_CALL();
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
         BlockService *bs = BlockService::GetInstance();
         m_copyPasteUI->SetBlockControl(bs->GetBlockCtrl());
         wrapper->SetCPIsRegistered(TRUE);
         int version = wrapper->GetCPVersion();
         g_debug("%s: version is %d\n", __FUNCTION__, version);

         if (version >= 3) {
            CopyPasteVersionChanged(version);
            m_copyPasteUI->SetCopyPasteAllowed(TRUE);
         }
         /*
          * Set legacy copy/paste version.
          */
         CopyPaste_SetVersion(version);
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
   TRACE_CALL();
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();

   if (!wrapper->IsDnDEnabled()) {
      return FALSE;
   }

   if (!wrapper->IsDnDRegistered()) {
      m_dndUI = new DnDUIX11(wrapper->GetToolsAppCtx());
      if (m_dndUI) {
         BlockService *bs = BlockService::GetInstance();
         m_dndUI->SetBlockControl(bs->GetBlockCtrl());
         if (m_dndUI->Init()) {
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
   TRACE_CALL();
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
   TRACE_CALL();
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();
   if (wrapper->IsDnDRegistered()) {
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
   TRACE_CALL();
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
   TRACE_CALL();
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
   TRACE_CALL();
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
   TRACE_CALL();
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
   TRACE_CALL();
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();

   ASSERT(wrapper);

   ToolsAppCtx *ctx = wrapper->GetToolsAppCtx();
   ASSERT(ctx);

   Pointer_Init(ctx);
}


/**
 * Return platform DnD/CP caps.
 *
 * @return 32-bit caps vector.
 */

uint32
CopyPasteDnDX11::GetCaps()
{
   return DND_CP_CAP_VALID |
          DND_CP_CAP_DND |
          DND_CP_CAP_CP |
          DND_CP_CAP_FORMATS_ALL |
          DND_CP_CAP_ACTIVE_CP |
          DND_CP_CAP_BIG_BUFFER;
}
