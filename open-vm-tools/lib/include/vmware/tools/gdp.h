/*********************************************************
 * Copyright (C) 2020-2021 VMware, Inc. All rights reserved.
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

#ifndef _VMWARE_TOOLS_GDP_H_
#define _VMWARE_TOOLS_GDP_H_

/**
 * @file gdp.h
 *
 * Public interface of the "gdp" plugin service.
 */

#include <glib-object.h>
#include "vmware/tools/plugin.h"

/*
 * Size in bytes:
 * 17 * 4096 - Maximum VMCI datagram size
 *        24 - VMCI datagram header size
 */
#define GDP_MAX_PACKET_LEN (17 * 4096 - 24)

/*
 * Limit GDP packet JSON base64 key value size to (16 * 4096) bytes, then
 * the rest JSON content will have (4096 - 24) bytes available.
 *
 * Base64 (16 * 4096) bytes are (12 * 4096) bytes before encoding.
 */
#define GDP_USER_DATA_LEN (12 * 4096)

/*
 * Property name of the gdp plugin service in the tools
 * applicatin context service object.
 */
#define TOOLS_PLUGIN_SVC_PROP_GDP "tps_prop_gdp"

/*
 * GdpError definitions.
 */
#define GDP_ERR_LIST                                      \
   GDP_ERR_ITEM(GDP_ERROR_SUCCESS = 0,                    \
                "No error")                               \
   GDP_ERR_ITEM(GDP_ERROR_INVALID_DATA,                   \
                "Invalid data")                           \
   GDP_ERR_ITEM(GDP_ERROR_DATA_SIZE,                      \
                "Data size too large")                    \
   GDP_ERR_ITEM(GDP_ERROR_GENERAL,                        \
                "General error")                          \
   GDP_ERR_ITEM(GDP_ERROR_STOP,                           \
                "Stopped for vmtoolsd shutdown")          \
   GDP_ERR_ITEM(GDP_ERROR_UNREACH,                        \
                "Host daemon unreachable")                \
   GDP_ERR_ITEM(GDP_ERROR_TIMEOUT,                        \
                "Operation timed out")

/*
 * GdpError codes.
 */
#define GDP_ERR_ITEM(a, b) a,
typedef enum GdpError {
   GDP_ERR_LIST
   GDP_ERR_MAX
} GdpError;
#undef GDP_ERR_ITEM

/**
 * @brief Type of the public interface of the gdp plugin service.
 *
 * This struct is published in the tools application context service object's
 * TOOLS_PLUGIN_SVC_PROP_GDP property.
 */
typedef struct ToolsPluginSvcGdp {
   GdpError (*publish)(gint64 createTime,
                       const gchar *topic,
                       const gchar *token,
                       const gchar *category,
                       const gchar *data,
                       guint32 dataLen,
                       gboolean cacheData);
} ToolsPluginSvcGdp;


/*
 ******************************************************************************
 * ToolsPluginSvcGdp_Publish --                                          */ /**
 *
 * @brief Publishes guest data to host side gdp daemon.
 *
 * This function is thread-safe and blocking, it should be called by vmtoolsd
 * pool threads started by ToolsCorePool_StartThread. Do not call the function
 * in ToolsOnLoad nor in/after TOOLS_CORE_SIG_SHUTDOWN handler.
 *
 * @param[in]          ctx         The application context
 * @param[in]          createTime  UTC timestamp, in number of micro-
 *                                 seconds since January 1, 1970 UTC.
 * @param[in]          topic       Topic
 * @param[in,optional] token       Token, can be NULL
 * @param[in,optional] category    Category, can be NULL that defaults to
 *                                 "application"
 * @param[in]          data        Buffer containing data to publish
 * @param[in]          dataLen     Buffer length
 * @param[in]          cacheData   Cache the data if TRUE
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError code otherwise.
 *
 ******************************************************************************
 */

static inline GdpError
ToolsPluginSvcGdp_Publish(ToolsAppCtx *ctx,      // IN
                          gint64 createTime,     // IN
                          const gchar *topic,    // IN
                          const gchar *token,    // IN, OPTIONAL
                          const gchar *category, // IN, OPTIONAL
                          const gchar *data,     // IN
                          guint32 dataLen,       // IN
                          gboolean cacheData)    // IN
{
   ToolsPluginSvcGdp *svcGdp = NULL;
   g_object_get(ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GDP, &svcGdp, NULL);
   if (svcGdp != NULL && svcGdp->publish != NULL) {
      return svcGdp->publish(createTime, topic, token,
                             category, data, dataLen, cacheData);
   }
   return GDP_ERROR_GENERAL;
}

#endif /* _VMWARE_TOOLS_GDP_H_ */
