/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 * unityPluginPosix.cpp --
 *
 *    POSIX subclass of UnityPlugin interface.
 */


#include "unityPluginPosix.h"


extern "C" {
#include <glib-object.h>

#include "vmware/tools/desktopevents.h"
#if defined(OPEN_VM_TOOLS)
   #include "unitylib/unity.h"
#else
   #include "unity.h"
#endif // OPEN_VM_TOOLS
}


namespace vmware {
namespace tools {


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::UnityPluginPosix --
 *
 *      Constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

UnityPluginPosix::UnityPluginPosix()
   : UnityPlugin(),
     mCtx(NULL)
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::~UnityPluginPosix --
 *
 *      Destructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

UnityPluginPosix::~UnityPluginPosix()
{
   if (mCtx) {
      for (std::map<const char*, gulong>::iterator i = mSignalIDs.begin();
           i != mSignalIDs.end();
           ++i) {
         g_signal_handler_disconnect(mCtx->serviceObj, i->second);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::Initialize --
 *
 *      Initialize UnityPlugin base class and connect to X Session Manager
 *      GLib signals.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
UnityPluginPosix::Initialize(ToolsAppCtx* ctx)  // IN
{
   if (UnityPlugin::Initialize(ctx)) {
      mCtx = ctx;
      mSignalIDs[TOOLS_CORE_SIG_XSM_DIE] =
         g_signal_connect(ctx->serviceObj, TOOLS_CORE_SIG_XSM_DIE,
                          G_CALLBACK(XSMDieCb), static_cast<gpointer>(this));
      return TRUE;
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::OnXSMEvent --
 *
 *      X Session Management event handler.  Exits Unity upon notice of session
 *      termination.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May trigger Unity exit.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPluginPosix::OnXSMDie()
{
   if (Unity_IsActive()) {
      Unity_Exit();
   }
}


/*
 ******************************************************************************
 * BEGIN Static member functions
 */


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::XSMDieCb --
 *
 *      Thunk between XSM "die" OnXSMDie.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See OnXSMDie.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPluginPosix::XSMDieCb(GObject* obj,        // UNUSED
                           ToolsAppCtx* ctx,    // UNUSED
                           gpointer cbData)     // IN: UnityPluginPosix*
{
   UnityPluginPosix* unityPlugin = static_cast<UnityPluginPosix*>(cbData);
   unityPlugin->OnXSMDie();
}


/*
 * END Static member functions
 ******************************************************************************
 */


} // namespace tools
} // namespace vmware
