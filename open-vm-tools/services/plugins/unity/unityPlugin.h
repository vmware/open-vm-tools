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

/*
 * unityPlugin.h --
 *
 *    Defines the object that implements the tools core service plugin form of
 *    Unity.
 */

#ifndef _UNITY_PLUGIN_H_
#define _UNITY_PLUGIN_H_

#include <string>
#include <vector>

#ifdef _WIN32
#include "boost/shared_ptr.hpp"
#endif // _WIN32

#include "vmware/tools/plugin.h"

namespace vmware { namespace tools {

class UnityPBRPCServer;

typedef struct _UnityUpdateChannel UnityUpdateChannel;


/*
 *-----------------------------------------------------------------------------
 *
 * vmware::tools::RpcChannelCallbackEntry --
 *
 *      Helper struct to build an RPCChannelCallback structure from a command
 *      string and RpcIn_Callback.
 *
 *-----------------------------------------------------------------------------
 */

struct RpcChannelCallbackEntry
   : public RpcChannelCallback
{
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


/*
 *-----------------------------------------------------------------------------
 *
 * vmware::tools::ToolsAppCapabilityOldEntry --
 *
 *      Helper struct to build an ToolsAppCapability structure from an old
 *      style capability name and flag to indicate whether the capability is
 *      enabled.
 *
 *-----------------------------------------------------------------------------
 */

struct ToolsAppCapabilityOldEntry
   : public ToolsAppCapability
{
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


/*
 *-----------------------------------------------------------------------------
 *
 * vmware::tools::ToolsAppCapabilityNewEntry --
 *
 *      Helper struct to build an ToolsAppCapability structure from a new style
 *      capability index and flag to indicate whether the capability is enabled.
 *
 *-----------------------------------------------------------------------------
 */

struct ToolsAppCapabilityNewEntry
   : public ToolsAppCapability
{
   ToolsAppCapabilityNewEntry(GuestCapabilities cap, gboolean enabled)
   {
      type = TOOLS_CAP_NEW;
      name = NULL;
      index = cap;
      value = enabled;
   }
};


/*
 *-----------------------------------------------------------------------------
 *
 * vmware::tools::ToolsPlugin --
 *
 *      Defines a pure virtual interface for plugins to implement. These
 *      methods are called by per-plugin static C functions in the the plugin
 *      entry module.
 *
 *-----------------------------------------------------------------------------
 */

class ToolsPlugin {
public:
   virtual ~ToolsPlugin() {};

   virtual gboolean Initialize(ToolsAppCtx *ctx) = 0;
   virtual gboolean Reset(gpointer src) = 0;
   virtual void Shutdown(gpointer src) = 0;
   virtual gboolean SetOption(gpointer src,
                              const std::string &option,
                              const std::string &value) = 0;
   virtual std::vector<ToolsAppCapability> GetCapabilities(gboolean set) = 0;
   virtual std::vector<RpcChannelCallback> GetRpcCallbackList() = 0;

#if defined(G_PLATFORM_WIN32)
   virtual void SessionChange(gpointer src, DWORD stateCode, DWORD sessionID) = 0;
#endif // G_PLATFORM_WIN32
};


class UnityPlugin
   : public ToolsPlugin
{
public:
   UnityPlugin();
   virtual ~UnityPlugin();

   virtual gboolean Initialize(ToolsAppCtx *ctx);

   virtual gboolean Reset(gpointer src) { return TRUE; }
   virtual void Shutdown(gpointer src) {};
   virtual gboolean SetOption(gpointer src,
                              const std::string &option,
                              const std::string &value)
   {
      return FALSE;
   }
   virtual std::vector<ToolsAppCapability> GetCapabilities(gboolean set);
   virtual std::vector<RpcChannelCallback> GetRpcCallbackList();

protected:
   UnityUpdateChannel *mUnityUpdateChannel;
};

#ifdef _WIN32
class UnityPluginWin32
   : public UnityPlugin
{
public:
   UnityPluginWin32();
   virtual ~UnityPluginWin32();

   virtual gboolean Initialize(ToolsAppCtx *ctx);

   virtual std::vector<ToolsAppCapability> GetCapabilities(gboolean set);
   virtual void SessionChange(gpointer src, DWORD code, DWORD id) {};

protected:
   boost::shared_ptr<UnityPBRPCServer> mUnityPBRPCServer;
};
#endif // _WIN32

} /* namespace tools */ } /* namespace vmware */

#endif // _UNITY_PLUGIN_H_
