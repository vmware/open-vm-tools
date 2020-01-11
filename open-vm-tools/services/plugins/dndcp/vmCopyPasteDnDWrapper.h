/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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
 * @file vmCopyPasteDnDWrapper.h
 *
 * The inherited implementation of common class CopyPasteDnDWrapper in VM side.
 */

#ifndef __VM_COPYPASTEDNDWRAPPER_H__
#define __VM_COPYPASTEDNDWRAPPER_H__

#include "copyPasteDnDWrapper.h"

class VMCopyPasteDnDWrapper
   : public CopyPasteDnDWrapper
{
public:
   ~VMCopyPasteDnDWrapper() { }

   static VMCopyPasteDnDWrapper *CreateInstance(void);
   virtual void Init(ToolsAppCtx *ctx);
   virtual ToolsAppCtx *GetToolsAppCtx() { return m_ctx; }
   virtual void OnCapReg(gboolean set);
   virtual int GetCPVersion();
   virtual int GetDnDVersion();
   virtual void OnResetInternal();
   virtual gboolean OnSetOption(const char *option, const char *value);
   void RemoveDnDPluginResetTimer(void);

protected:
   void AddDnDPluginResetTimer(void);

private:
   VMCopyPasteDnDWrapper() : m_ctx(NULL), m_resetTimer(NULL) { }
   VMCopyPasteDnDWrapper(const VMCopyPasteDnDWrapper &wrapper);
   VMCopyPasteDnDWrapper& operator=(const VMCopyPasteDnDWrapper &wrapper);

private:
   ToolsAppCtx *m_ctx;
   GSource *m_resetTimer;
};

#endif // __VM_COPYPASTEDNDWRAPPER_H__
