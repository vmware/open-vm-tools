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

#ifndef _UNITY_PLUGIN_H_
#define _UNITY_PLUGIN_H_

/**
 * @file unityPlugin.h
 *
 *    Defines the object that implements the tools core service plugin form of Unity.
 *
 * @addtogroup vmtools_plugins
 * @{
 */

#include <string>
#include <vector>

#ifdef _WIN32
#include "boost/shared_ptr.hpp"
#endif // _WIN32

#include "vmware/tools/plugin.h"

namespace vmware { namespace tools {

class UnityPBRPCServer;

typedef struct _UnityUpdateChannel UnityUpdateChannel;


/**
 * Helper struct to build an RPCChannelCallback structure from a name and a
 * simple function pointer that takes an RPCInData pointer.
 *
 * @param[in]  RpcName   RPC Name.
 * @param[in]  func      RPC Callback Function Pointer.
 *
 */

struct RpcChannelCallbackEntry  : public RpcChannelCallback {
   RpcChannelCallbackEntry(const char *RpcName, RpcIn_Callback func)
   {
      name = RpcName;
      callback = func;
      clientData = NULL;
      xdrIn = NULL;
      xdrOut = NULL;
      xdrInSize = 0;
   }
};


/**
 * Helper struct to build an ToolsAppCapability structure from an old style
 * capability name and flag to indicate whether the capability is enabled.
 *
 * @param[in]  capName   Old style capabilty name.
 * @param[in]  enabled   True if the capability is enabled.
 *
 */

struct ToolsAppCapabilityOldEntry : public ToolsAppCapability {
   ToolsAppCapabilityOldEntry(const char *capName, gboolean enabled)
   {
      type = TOOLS_CAP_OLD;
      /* The capability name field is not-const (though it seems to be treated that way)
       * so we'll need to cast away the const here 8-(
       */
      name = const_cast<gchar*>(capName);
      value = enabled;
   }
};


/**
 * Helper struct to build an ToolsAppCapability structure from a new style
 * capability index and flag to indicate whether the capability is enabled.
 *
 * @param[in]  cap       GuestCapabilities index.
 * @param[in]  enabled   True if the capability is enabled.
 *
 */

struct ToolsAppCapabilityNewEntry : public ToolsAppCapability {
   ToolsAppCapabilityNewEntry(GuestCapabilities cap, gboolean enabled)
   {
      type = TOOLS_CAP_NEW;
      name = NULL;
      index = cap;
      value = enabled;
   }
};


/**
 * Defines a pure virtual interface for plugins to implement. These methods
 * are called by per-plugin static C functions in the the plugin entry module.
 */

class ToolsPlugin {
public:
   virtual ~ToolsPlugin() {};

   /**
    * Handles a reset signal. This callback is called when the service receives a
    * "reset" message from the VMX, meaning the VMX may be restarting the RPC
    * channel (due, for example, to restoring a snapshot, resuming a VM or a VMotion),
    * and should be used to reset any plugin state that depends on the VMX.
    *
    * @param[in]  src      Event source.
    *
    * @return TRUE on success.
    */

   virtual gboolean Reset(gpointer src) = 0;

   /**
    * Handles a shutdown callback; This is called before the service is shut down,
    * and should be used to clean up any resources that were initialized by the plugin.
    *
    * @param[in]  src      The source object.
    */

   virtual void Shutdown(gpointer src) = 0;

   /**
    * Handles a "Set_Option" callback. This callback is called when the VMX sends
    * a "Set_Option" command to tools, to configure different options whose values
    * are kept outside of the virtual machine.
    *
    * @param[in]  src      Event source.
    * @param[in]  option   Option being set.
    * @param[in]  value    Option value.
    *
    * @return TRUE on success.
    */

   virtual gboolean SetOption(gpointer src,
                              const std::string &option,
                              const std::string &value) = 0;

   /**
    * Called by the service core when the host requests the capabilities supported
    * by the guest tools.
    *
    * @param[in]  set      Whether capabilities are being set or unset.
    *
    * @return A list of capabilities to be sent to the host.
    */

   virtual std::vector<ToolsAppCapability> GetCapabilities(gboolean set) = 0;

   /**
    * Called by the service core when the host requests the RPCs supported
    * by the guest tools.
    *
    * @return A list of RPC Callbacks to be sent to the host.
    */

   virtual std::vector<RpcChannelCallback> GetRpcCallbackList() = 0;

#if defined(G_PLATFORM_WIN32)
   /**
    * Handles a session state change callback; this is only called on Windows,
    * from both the "vmsvc" instance (handled by SCM notifications) and from
    * "vmusr" with the "fast user switch" plugin.
    *
    * @param[in]  src         The source object.
    * @param[in]  stateCode   Session state change code.
    * @param[in]  sessionID   Session ID.
    */

   virtual void SessionChange(gpointer src, DWORD stateCode, DWORD sessionID) = 0;
#endif // G_PLATFORM_WIN32
};


class UnityPlugin  : public ToolsPlugin {
public:
   UnityPlugin(ToolsAppCtx *ctx);
   virtual ~UnityPlugin();

   virtual gboolean Reset(gpointer src) { return TRUE; }
   virtual void Shutdown(gpointer src) {};
   virtual gboolean SetOption(gpointer src, const std::string &option, const std::string &value)
   {
      return FALSE;
   }
   virtual std::vector<ToolsAppCapability> GetCapabilities(gboolean set);
   virtual std::vector<RpcChannelCallback> GetRpcCallbackList();

protected:
   UnityUpdateChannel *mUnityUpdateChannel;
};

#ifdef _WIN32
class UnityPluginWin32 : public UnityPlugin {
public:
   UnityPluginWin32(ToolsAppCtx *ctx);
   virtual ~UnityPluginWin32();

   virtual std::vector<ToolsAppCapability> GetCapabilities(gboolean set);
   virtual void SessionChange(gpointer src, DWORD code, DWORD id) {};

protected:
   boost::shared_ptr<UnityPBRPCServer> mUnityPBRPCServer;
};
#endif // _WIN32

} /* namespace tools */ } /* namespace vmware */

#endif // _UNITY_PLUGIN_H_
