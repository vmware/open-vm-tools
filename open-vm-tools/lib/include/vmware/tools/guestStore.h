/*********************************************************
 * Copyright (c) 2020 VMware, Inc. All rights reserved.
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

#ifndef _GUESTSTORE_H_
#define _GUESTSTORE_H_

/**
 * @file guestStore.h
 *
 * Public interface for the GuestStore plugin.
 *
 * @addtogroup vmtools_plugins
 * @{
 */

#include <glib-object.h>
#include "vmware/tools/plugin.h"

/**
 * Signal sent when GuestStore is enabled or disabled.
 *
 * @param[in]  src      The source object.
 * @param[in]  enabled  TRUE if VMX GuestStore access is enabled, FALSE otherwise.
 * @param[in]  data     Client data.
 */
#define TOOLS_CORE_SIG_GUESTSTORE_STATE "tcs_gueststore_state"

/*
 * Property name of the guestStore plugin in the tools application context
 * service object.
 */
#define TOOLS_PLUGIN_SVC_PROP_GUESTSTORE "tps_prop_gueststore"

/**
 * @brief Type of the public interface of the guestStore plugin.
 *
 * This struct is published in the tools application context service object's
 * TOOLS_PLUGIN_SVC_PROP_GUESTSTORE property.
 */
typedef struct ToolsPluginSvcGuestStore {
   void (*shutdown)(void);
} ToolsPluginSvcGuestStore;


/*
 ******************************************************************************
 * ToolsPluginSvcGuestStore_Shutdown --                                  */ /**
 *
 * @brief Shuts down guestStore plugin.
 *
 * To avoid possible deadlock at vmtoolsd shutdown time, guestStore plugin
 * needs to be shut down before tools core thread pool. This function provides
 * a special way to shut down guestStore plugin other than regular in-plugin
 * TOOLS_CORE_SIG_SHUTDOWN signal handler.
 *
 * @param[in]              ctx       The app context
 *
 ******************************************************************************
 */

static inline void
ToolsPluginSvcGuestStore_Shutdown(ToolsAppCtx *ctx) // IN
{
   ToolsPluginSvcGuestStore *svcGuestStore = NULL;
   g_object_get(ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GUESTSTORE,
                &svcGuestStore, NULL);
   if (svcGuestStore != NULL && svcGuestStore->shutdown != NULL) {
      svcGuestStore->shutdown();
   }
}

/** @} */

#endif /* _GUESTSTORE_H_ */
