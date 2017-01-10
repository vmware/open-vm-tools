/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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

#ifndef _DESKTOPEVENTSINT_H_
#define _DESKTOPEVENTSINT_H_

/**
 * @file destopEventsInt.h
 *
 * Internal plugin definitions.
 */

#define VMW_TEXT_DOMAIN "desktopEvents"
#define G_LOG_DOMAIN    VMW_TEXT_DOMAIN
#include "vmware/tools/plugin.h"

typedef struct DesktopEventFuncs {
   gboolean (*initFn)(ToolsAppCtx *, ToolsPluginData *);
   void     (*shutdownFn)(ToolsAppCtx *ctx, ToolsPluginData *);
   gboolean initialized;
} DesktopEventFuncs;


/*
 * This plugin's private data field is a GHashTable*.  Each sub-feature may use
 * that table to keep its own private state.  The following key is reserved and
 * points to the hosting application's ToolsAppCtx.
 */
#define DE_PRIVATE_CTX  "ctx"


/*
 * This platform-specific file defines the list of features available
 * for the current platform being built.
 */
#include "deFeatures.h"

#endif /* _DESKTOPEVENTSINT_H_ */
