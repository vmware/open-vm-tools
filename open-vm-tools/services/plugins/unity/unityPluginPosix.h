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
 * unityPluginPosix.h --
 *
 *    POSIX subclass of UnityPlugin interface.
 */

#ifndef UNITY_PLUGIN_POSIX_H
#define UNITY_PLUGIN_POSIX_H

#include <map>

#include "unityPlugin.h"

extern "C" {
#include <glib-object.h>
}


namespace vmware {
namespace tools {

class UnityPluginPosix
   : public UnityPlugin
{
public:
   UnityPluginPosix(const ToolsAppCtx* ctx);
   virtual ~UnityPluginPosix();

   std::vector<ToolsAppCapability> GetCapabilities(gboolean set);
   std::vector<ToolsPluginSignalCb> GetSignalRegistrations(ToolsPluginData*) const;

private:
   static void XSMDieCb(GObject*, ToolsAppCtx*, gpointer);
   void OnXSMDie();

   static void XIOErrorCb(GObject*, ToolsAppCtx*, gpointer);
   void OnXIOError();

   std::map<const char*, gulong> mSignalIDs;
};


} // namespace tools
} // namespace vmware

#endif // UNITY_PLUGIN_POSIX_H
