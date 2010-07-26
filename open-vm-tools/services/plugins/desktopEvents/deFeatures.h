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

#ifndef _DEFEATURES_H_
#define _DEFEATURES_H_

/**
 * @file deFeatures.h
 *
 * X11-specific featurs of the plugin. Don't include this file directly -
 * include desktopEventsInt.h instead.
 */

#define VMUSER_TITLE    "vmware-user"

void
Reload_Do(void);

gboolean
Reload_Init(ToolsAppCtx *ctx,
            ToolsPluginData *pdata);

void
Reload_Shutdown(ToolsAppCtx *ctx);

gboolean
X11Lock_Init(ToolsAppCtx *ctx,
             ToolsPluginData *pdata);

gboolean
XIOError_Init(ToolsAppCtx *ctx,
              ToolsPluginData *pdata);

void
XIOError_Shutdown(ToolsAppCtx *ctx);


#if defined(DE_MAIN)
static DesktopEventFuncs gFeatures[] = {
   { X11Lock_Init,         NULL,                      FALSE },
   { Reload_Init,          Reload_Shutdown,           FALSE },
   { XIOError_Init,        XIOError_Shutdown,         FALSE },
};
#endif

#endif /* _DEFEATURES_H_ */

