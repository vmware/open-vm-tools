/*********************************************************
 * Copyright (C) 2021 VMware, Inc. All rights reserved.
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

#ifndef _CONTAINERINFOINT_H_
#define _CONTAINERINFOINT_H_

#include "conf.h"

#define G_LOG_DOMAIN CONFGROUPNAME_CONTAINERINFO

#include <glib.h>

/**
 * @file containerInfoInt.h
 *
 * Header file with few functions that are internal to containerInfo plugin.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ContainerInfo {
   char *id;
   char *image;
} ContainerInfo;

void ContainerInfo_DestroyContainerData(void *pointer);
void ContainerInfo_DestroyContainerList(GSList *containerList);

GHashTable *ContainerInfo_GetDockerContainers(const char *dockerSocketPath);

GSList *ContainerInfo_GetContainerList(const char *ns,
                                       const char *containerdSocketPath,
                                       unsigned int maxContainers);

#ifdef __cplusplus
}
#endif

#endif /* _CONTAINERINFOINT_H_ */
