/*********************************************************
 * Copyright (c) 2021,2023 VMware, Inc. All rights reserved.
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
 * componentMgrUtil.c --
 *
 * Common utility functions used by the componentMgr plugin.
 *
 */


#include "componentMgrPlugin.h"
#include "vmware/tools/log.h"


/*
 ******************************************************************************
 * ComponentMgr_SendRpc --
 *
 * Sends RPC message to fetch the guestVars.
 *
 * @param[in] ctx Tools application context.
 * @param[in] guestInfoCmd Guestinfo command to fetch the guestVar.
 * @param[out] outBuffer Output buffer to hold RPC result (optional).
 * @param[out] outBufferLen Output buffer length (optional).
 *
 * @return
 *      TRUE if cmd executed successfully, otherwise FALSE
 *
 * @note: outBuffer and outBufferLen are optional parameters and be used when
 * RPC command output required. Both output parameters are required for output.
 * Users have to call RpcChannel_Free on output buffer if supplied.
 *
 * Side effects:
 *      None.
 *
 ******************************************************************************
 */

gboolean
ComponentMgr_SendRpc(ToolsAppCtx *ctx,         // IN
                     const char *guestInfoCmd, // IN
                     char **outBuffer,         // OUT
                     size_t *outBufferLen)     // OUT
{
   gboolean status;
   size_t replyLen;
   char *reply = NULL;

   ASSERT(ctx != NULL);
   ASSERT(ctx->rpc != NULL);
   ASSERT(guestInfoCmd != NULL);

   status = RpcChannel_Send(ctx->rpc,
                            guestInfoCmd,
                            strlen(guestInfoCmd) + 1,
                            &reply,
                            &replyLen);
   if (!status) {
      g_info("%s: Failed to send RPC message, request: \'%s\',"
             " reply: \'%s\'.\n", __FUNCTION__, guestInfoCmd,
             VM_SAFE_STR(reply));
   }

   if (outBuffer != NULL && outBufferLen != NULL) {
      *outBuffer = reply;
      *outBufferLen = replyLen;
   } else {
      RpcChannel_Free(reply);
   }
   return status;
}


/*
 **************************************************************************
 * ComponentMgr_GetComponentInstallStatus --
 *
 * This function returns an enum equivalent of the current status of the
 * component.
 *
 * @param[in] installStatus Enum value of status of component.
 *
 * @return
 *       String equivalent of the component current status.
 *
 * @Side effects
 *       None.
 *
 **************************************************************************
 */

const char*
ComponentMgr_GetComponentInstallStatus(InstallStatus installStatus) // IN
{
   switch (installStatus) {
      case NOTINSTALLED:      return "NOTINSTALLED";
      case INSTALLING:        return "INSTALLING";
      case INSTALLED:         return "INSTALLED";
      case REMOVING:          return "REMOVING";
      case INSTALLFAILED:     return "INSTALLFAILED";
      case REMOVEFAILED:      return "REMOVEFAILED";
      case UNMANAGED:         return "UNMANAGED";
      case SCRIPTFAILED:      return "SCRIPTFAILED";
      case SCRIPTTERMINATED:  return "SCRIPTTERMINATED";
   }
   return "INVALIDSTATUS";
}


/*
 **************************************************************************
 * ComponentMgr_GetComponentAction --
 *
 * This function returns an enum equivalent of component action to be executed.
 *
 * @param[in] action The action to be taken on the component.
 *
 * @return
 *       String equivalent of the component action.
 *
 * @Side effects
 *       None.
 *
 **************************************************************************
 */

const char*
ComponentMgr_GetComponentAction(Action action) // IN
{
   switch (action) {
      case PRESENT:       return COMPONENTMGR_COMPONENTPRESENT;
      case ABSENT:        return COMPONENTMGR_COMPONENTABSENT;
      case CHECKSTATUS:   return COMPONENTMGR_COMPONENTCHECKSTATUS;
      case INVALIDACTION: return COMPONENTMGR_COMPONENINVALIDACTION;
   }
   return COMPONENTMGR_COMPONENINVALIDACTION;
}


/*
 **************************************************************************
 * ComponentMgr_GetIncludedComponents --
 *
 * This function returns an enum equivalent of the special values in the
 * included tools.conf param.
 *
 * @param[in] specialvalue The enum value of special value in the param.
 *
 * @return
 *       String equivalent of the special value in the param.
 *
 * @Side effects
 *       None.
 *
 **************************************************************************
 */


const char*
ComponentMgr_GetIncludedComponents(IncludedComponents specialValue) // IN
{
   switch (specialValue) {
      case ALLCOMPONENTS:      return "ALLCOMPONENTS";
      case NONECOMPONENTS:     return "NONECOMPONENTS";
      case NOSPECIALVALUES:    return "NOSPECIALVALUES";
   }
   return "NOSPECIALVALUES";
}


/*
 *****************************************************************************
 * ComponentMgr_PublishAvailableComponents --
 *
 * This function publishes guestVar guestinfo.vmware.components.available with
 * requested components.
 *
 * @param[in] ctx Tools application context.
 * @param[in] components Comma separated list of available components.
 *
 * @return
 *      None
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

void
ComponentMgr_PublishAvailableComponents(ToolsAppCtx *ctx,       // IN
                                        const char *components) // IN
{
   gboolean status;
   gchar *msg = g_strdup_printf("%s.%s %s", COMPONENTMGR_PUBLISH_COMPONENTS,
                                COMPONENTMGR_INFOAVAILABLE,
                                components);

   status = ComponentMgr_SendRpc(ctx, msg, NULL, NULL);
   g_free(msg);
}
