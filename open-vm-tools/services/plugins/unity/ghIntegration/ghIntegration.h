/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * ghIntegration.h --
 *
 *    Commands for guest host integration.
 */

#ifndef _GH_INTEGRATION_H_
#define _GH_INTEGRATION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

#include "vmware.h"

Bool GHI_IsSupported(void);
void GHI_Init(GMainLoop *mainLoop, const char **envp);
void GHI_Cleanup(void);
void GHI_RegisterCaps(void);
void GHI_UnregisterCaps(void);
void GHI_Gather(void);

#ifndef _WIN32
const char *GHIX11_FindDesktopUriByExec(const char *exec);
#endif

#ifdef __cplusplus
}; /* extern "C" */
#endif // __cplusplus
#endif // _GH_INTEGRATION_H_
