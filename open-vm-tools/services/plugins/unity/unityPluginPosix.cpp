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

UnityPluginPosix::UnityPluginPosix(const ToolsAppCtx* ctx) // IN: The app context.
   : UnityPlugin(ctx)
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
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::GetCapabilities --
 *
 *      Called by the service core when the host requests the capabilities
 *      supported by the guest tools.
 *
 * Results:
 *      A list of capabilities to be sent to the host.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

std::vector<ToolsAppCapability>
UnityPluginPosix::GetCapabilities(gboolean set) // IN
{
   std::vector<ToolsAppCapability> caps = UnityPlugin::GetCapabilities(set);
#define ADDCAP(capName) caps.push_back(ToolsAppCapabilityNewEntry(capName, set))
   ADDCAP(UNITY_CAP_WORK_AREA);
   ADDCAP(UNITY_CAP_START_MENU);
   ADDCAP(UNITY_CAP_MULTI_MON);
   ADDCAP(UNITY_CAP_VIRTUAL_DESK);
   ADDCAP(UNITY_CAP_STICKY_WINDOWS);
#undef ADDCAP
   return caps;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::GetSignalRegistrations --
 *
 *      Returns a vector containing signal registration info (signal name,
 *      callback, callback context).  Signals will be connected by container
 *      after all plugins have successfully registered.
 *
 * Results:
 *      Vector of signal registration info.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

std::vector<ToolsPluginSignalCb>
UnityPluginPosix::GetSignalRegistrations(ToolsPluginData* pdata) // IN
   const
{
   std::vector<ToolsPluginSignalCb> signals =
      UnityPlugin::GetSignalRegistrations(pdata);
   signals.push_back(
      ToolsPlugin::SignalCtx(TOOLS_CORE_SIG_XIOERROR, (void*)XIOErrorCb,
                             static_cast<gpointer>(const_cast<UnityPluginPosix*>(this))));
   signals.push_back(
      ToolsPlugin::SignalCtx(TOOLS_CORE_SIG_XSM_DIE, (void*)XSMDieCb,
                             static_cast<gpointer>(const_cast<UnityPluginPosix*>(this))));
   return signals;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::OnXSMDie --
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
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::OnXIOError --
 *
 *      In response to an X I/O error, signals host UI that vmusr is no longer
 *      Unity-capable.
 *
 *      This is done because we can't perform a full, correct clean-up after
 *      receiving an X I/O error.  See xioError.c for details.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Invokes a G->H RPC.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPluginPosix::OnXIOError()
{
   char *result;
   size_t resultLen;
   char tmp[] = "tools.capability.unity 0";
   RpcChannel_Send(mCtx->rpc, tmp, sizeof tmp, &result, &resultLen);
   vm_free(result);
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
 *      Thunk between XSM "die" and OnXSMDie.
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
 *-----------------------------------------------------------------------------
 *
 * UnityPluginPosix::XIOErrorCb --
 *
 *      Thunk between XIOErrorHandler and OnXIOError.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See OnXIOError.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPluginPosix::XIOErrorCb(GObject* obj,        // UNUSED
                             ToolsAppCtx* ctx,    // UNUSED
                             gpointer cbData)     // IN: UnityPluginPosix*
{
   UnityPluginPosix* unityPlugin = static_cast<UnityPluginPosix*>(cbData);
   unityPlugin->OnXIOError();
}


/*
 * END Static member functions
 ******************************************************************************
 */


} // namespace tools
} // namespace vmware
