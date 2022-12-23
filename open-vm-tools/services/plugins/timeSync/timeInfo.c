/*********************************************************
 * Copyright (C) 2022 VMware, Inc. All rights reserved.
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
 * @file timeInfo.c
 *
 * The feature allows tools to subscribe and receive updates from VMX when
 * time related properties of the host change.
 */

#include "conf.h"
#include "timeSync.h"
#include "system.h"
#include "vmware/tools/plugin.h"


/**
 * Initialize TimeInfo in TimeSync.
 *
 * @param[in] ctx The application context.
 */
void
TimeInfo_Init(ToolsAppCtx *ctx)
{
   gboolean timeInfoEnabled =
      g_key_file_get_boolean(ctx->config,
                             CONFGROUPNAME_TIMESYNC,
                             CONFNAME_TIMESYNC_TIMEINFO_ENABLED,
                             NULL);
   ASSERT(vmx86_linux);
   g_debug("%s: TimeInfo support is %senabled.\n",
           __FUNCTION__, !timeInfoEnabled ? "not " : "");
}


/**
 * Cleans up internal TimeInfo state.
 */
void
TimeInfo_Shutdown(void)
{
}
