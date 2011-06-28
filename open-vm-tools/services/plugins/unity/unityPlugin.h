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
   struct SignalCtx
      : public ToolsPluginSignalCb
   {
      SignalCtx(const char* signame_, void* callback_, void* clientData_)
      {
         signame = signame_;
         callback = callback_;
         clientData = clientData_;
      }
   };

   virtual ~ToolsPlugin() {};

   virtual gboolean Initialize() = 0;
   virtual gboolean Reset(gpointer src) = 0;
   virtual void Shutdown(gpointer src) = 0;
   virtual gboolean SetOption(gpointer src,
                              const std::string &option,
                              const std::string &value) = 0;

   virtual std::vector<ToolsAppCapability> GetCapabilities(gboolean set) = 0;
   virtual std::vector<RpcChannelCallback> GetRpcCallbackList() = 0;
   virtual std::vector<ToolsPluginSignalCb> GetSignalRegistrations(ToolsPluginData*) const;

protected:
   ToolsPlugin(const ToolsAppCtx* ctx) : mCtx(ctx) {}

   // C thunks.
   static gboolean OnCReset(gpointer src, ToolsAppCtx* ctx, ToolsPluginData* pdata);
   static void OnCShutdown(gpointer src, ToolsAppCtx* ctx, ToolsPluginData* pdata);
   static GArray* OnCCapabilities(gpointer src, ToolsAppCtx* ctx, gboolean set,
                                  ToolsPluginData* plugin);
   static gboolean OnCSetOption(gpointer src, ToolsAppCtx* ctx, const gchar* option,
                                const gchar* value, ToolsPluginData* plugin);

   const ToolsAppCtx* mCtx;
};


class UnityPlugin
   : public ToolsPlugin
{
public:
   virtual gboolean Initialize();

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
   UnityPlugin(const ToolsAppCtx* ctx);
   virtual ~UnityPlugin();

   UnityUpdateChannel *mUnityUpdateChannel;
};

#ifdef _WIN32
class UnityPluginWin32
   : public UnityPlugin
{
public:
   UnityPluginWin32(const ToolsAppCtx* ctx);
   virtual ~UnityPluginWin32();

   virtual gboolean Initialize();

   virtual std::vector<ToolsAppCapability> GetCapabilities(gboolean set);
   virtual std::vector<ToolsPluginSignalCb> GetSignalRegistrations(ToolsPluginData*) const;

   virtual DWORD OnServiceControl(gpointer src,
                                  ToolsAppCtx *ctx,
                                  SERVICE_STATUS_HANDLE handle,
                                  guint control,
                                  guint evtType,
                                  gpointer evtData);

   virtual void OnDesktopSwitch();

protected:
   boost::shared_ptr<UnityPBRPCServer> mUnityPBRPCServer;

private:
   static DWORD OnCServiceControl(gpointer src, ToolsAppCtx* ctx,
                                  SERVICE_STATUS_HANDLE handle, guint control,
                                  guint evtType, gpointer evtData,
                                  ToolsPluginData* data);
   static void OnCDesktopSwitch(gpointer src, ToolsAppCtx* ctx, ToolsPluginData* plugin);
};
#endif // _WIN32

} /* namespace tools */ } /* namespace vmware */

#endif // _UNITY_PLUGIN_H_
