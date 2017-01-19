/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * This singleton class implements an abstraction around the concrete classes
 * that implement DnD, and copy and paste.
 */

#ifndef __COPYPASTEDNDWRAPPER_H__
#define __COPYPASTEDNDWRAPPER_H__

#include "vmware/tools/plugin.h"
#include "copyPasteDnDImpl.h"
#include "dndPluginInt.h"

class CopyPasteDnDWrapper
{
public:
   ~CopyPasteDnDWrapper();
   static CopyPasteDnDWrapper *GetInstance();
   static void Destroy();
   gboolean RegisterCP();
   void UnregisterCP();
   gboolean RegisterDnD();
   void UnregisterDnD();
   void SetDnDVersion(int version) {m_dndVersion = version;};
   int GetDnDVersion();
   void SetCPVersion(int version) {m_cpVersion = version;};
   int GetCPVersion();
   void SetCPIsRegistered(gboolean isRegistered);
   gboolean IsCPRegistered();
   void SetDnDIsRegistered(gboolean isRegistered);
   gboolean IsDnDRegistered();
   void SetDnDIsEnabled(gboolean isEnabled);
   gboolean IsDnDEnabled();
   void SetCPIsEnabled(gboolean isEnabled);
   gboolean IsCPEnabled();
   void OnReset();
   void OnResetInternal();
   void OnCapReg(gboolean set);
   gboolean OnSetOption(const char *option, const char *value);
   void Init(ToolsAppCtx *ctx);
   void PointerInit(void);
   ToolsAppCtx *GetToolsAppCtx() {return m_ctx;};
   uint32 GetCaps();
private:
   /*
    * We're a singleton, so it is a compile time error to call these.
    */
   CopyPasteDnDWrapper();
   CopyPasteDnDWrapper(const CopyPasteDnDWrapper &wrapper);
   CopyPasteDnDWrapper& operator=(const CopyPasteDnDWrapper &wrapper);
private:
   gboolean m_isCPEnabled;
   gboolean m_isDnDEnabled;
   gboolean m_isCPRegistered;
   gboolean m_isDnDRegistered;
   int m_cpVersion;
   int m_dndVersion;
   static CopyPasteDnDWrapper *m_instance;
   ToolsAppCtx *m_ctx;
   CopyPasteDnDImpl *m_pimpl;
};

#endif // __COPYPASTEDNDWRAPPER_H__
