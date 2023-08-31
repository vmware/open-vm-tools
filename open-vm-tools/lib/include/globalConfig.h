/*********************************************************
 * Copyright (c) 2020-2021,2023 VMware, Inc. All rights reserved.
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

#ifndef _GLOBAL_CONFIG_H_
#define _GLOBAL_CONFIG_H_

#if (defined(_WIN32) && !defined(_ARM64_)) || \
    (defined(__linux__) && !defined(USERWORLD))

#define GLOBALCONFIG_SUPPORTED 1

#include "vmware/tools/plugin.h"
#include <time.h>
#include "guestStoreClient.h"

/**
 * @file globalConfig.h
 *
 * Interface of the module to fetch the tools.conf file from GuestStore.
 */

gboolean GlobalConfig_Start(ToolsAppCtx *ctx);

gboolean GlobalConfig_LoadConfig(GKeyFile **config,
                                 time_t *mtime);

gboolean GlobalConfig_GetEnabled(GKeyFile *conf);

void GlobalConfig_SetEnabled(gboolean enabled,
                             GKeyFile *conf);

gboolean GlobalConfig_DeleteConfig(void);

GuestStoreClientError GlobalConfig_DownloadConfig(GKeyFile *config);

#else

#undef GLOBALCONFIG_SUPPORTED

#endif

#endif /* _GLOBAL_CONFIG_H_ */
