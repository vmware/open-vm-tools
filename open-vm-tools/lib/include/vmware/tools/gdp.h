/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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
#define GDP_MAX_USER_DATA_LEN (17 * 4096 - 24)
#define GDP_SEND_RECV_BUF_LEN (17 * 4096)

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
   GDP_ERR_ITEM(GDP_ERROR_INTERNAL,                       \
                "Internal system error")                  \
   GDP_ERR_ITEM(GDP_ERROR_STOP,                           \
                "Stopped for vmtoolsd shutdown")          \
   GDP_ERR_ITEM(GDP_ERROR_UNREACH,                        \
                "Host daemon unreachable")                \
   GDP_ERR_ITEM(GDP_ERROR_TIMEOUT,                        \
                "Operation timed out")                    \
   GDP_ERR_ITEM(GDP_ERROR_SEND_SIZE,                      \
                "Message to host too large")              \
   GDP_ERR_ITEM(GDP_ERROR_RECV_SIZE,                      \
                "Receive buffer too small")

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
   GdpError (*publish)(const char *msg,
                       int msgLen,
                       char *reply,
                       int *replyLen);
   GdpError (*stop)(void);
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
 * @param[in]              ctx       The app context
 * @param[in]              msg       Buffer containing guest data to publish
 * @param[in]              msgLen    Guest data length
 * @param[out,optional]    reply     Buffer to receive reply from gdp daemon
 * @param[in,out,optional] replyLen  NULL when param reply is NULL, otherwise:
 *                                   reply buffer length on input,
 *                                   reply length on output
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError codes otherwise.
 *
 ******************************************************************************
 */

static inline GdpError
ToolsPluginSvcGdp_Publish(ToolsAppCtx *ctx, // IN
                          const char *msg,  // IN
                          int msgLen,       // IN
                          char *reply,      // OUT, OPTIONAL
                          int *replyLen)    // IN/OUT, OPTIONAL
{
   ToolsPluginSvcGdp *svcGdp = NULL;
   g_object_get(ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GDP, &svcGdp, NULL);
   if (svcGdp != NULL && svcGdp->publish != NULL) {
      return svcGdp->publish(msg, msgLen, reply, replyLen);
   }
   return GDP_ERROR_INTERNAL;
}


/*
 ******************************************************************************
 * ToolsPluginSvcGdp_Stop --                                             */ /**
 *
 * @brief Stops guest data publishing.
 *
 * This function notifies the pool thread in publish call to stop and return
 * immediately. It is intended to be called by the interrupt callback passed
 * to ToolsCorePool_StartThread. At vmtoolsd shutdown time, main thread will
 * call ToolsCorePool_Shutdown which invokes the interrupt callback.
 *
 * @param[in]              ctx       The app context
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return GDP_ERROR_INTERNAL otherwise.
 *
 ******************************************************************************
 */

static inline GdpError
ToolsPluginSvcGdp_Stop(ToolsAppCtx *ctx) // IN
{
   ToolsPluginSvcGdp *svcGdp = NULL;
   g_object_get(ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GDP, &svcGdp, NULL);
   if (svcGdp != NULL && svcGdp->stop != NULL) {
      return svcGdp->stop();
   }
   return GDP_ERROR_INTERNAL;
}

#endif /* _VMWARE_TOOLS_GDP_H_ */
