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
 * ghIntegration.c --
 *
 *    Guest-host integration functions.
 */

#include "vmware.h"
#include "appUtil.h"
#include "debug.h"
#include "dynxdr.h"
#include "ghIntegration.h"
#include "ghIntegrationInt.h"
#include "guestCaps.h"
#include "guestrpc/ghiGetBinaryHandlers.h"
#include "guestrpc/ghiProtocolHandler.h"
#include "guest_msg_def.h"
#include "rpcin.h"
#include "rpcout.h"
#include "str.h"
#include "strutil.h"
#include "unityCommon.h"
#include "util.h"


/*
 * Local functions
 */

static Bool GHITcloGetBinaryInfo(char const **result, size_t *resultLen,
                                 const char *name, const char *args,
                                 size_t argsSize, void *clientData);
static Bool GHITcloGetBinaryHandlers(RpcInData *data);
static Bool GHITcloGetStartMenuItem(char const **result,
                                    size_t *resultLen,
                                    const char *name,
                                    const char *args,
                                    size_t argsSize,
                                    void *clientData);
static Bool GHITcloOpenStartMenu(char const **result,
                                 size_t *resultLen,
                                 const char *name,
                                 const char *args,
                                 size_t argsSize,
                                 void *clientData);
static Bool GHITcloCloseStartMenu(char const **result,
                                  size_t *resultLen,
                                  const char *name,
                                  const char *args,
                                  size_t argsSize,
                                  void *clientData);
static Bool GHITcloShellOpen(char const **result,
                             size_t *resultLen,
                             const char *name,
                             const char *args,
                             size_t argsSize,
                             void *clientData);
static Bool GHITcloShellAction(char const **result,
                               size_t *resultLen,
                               const char *name,
                               const char *args,
                               size_t argsSize,
                               void *clientData);
static Bool GHITcloSetGuestHandler(RpcInData *data);
static Bool GHITcloRestoreDefaultGuestHandler(RpcInData *data);

static Bool GHIUpdateHost(GHIProtocolHandlerList *handlers);

DynBuf gTcloUpdate;
static GHIPlatform *ghiPlatformData;

DblLnkLst_Links launchMenu;

/*
 *----------------------------------------------------------------------------
 *
 * GHI_IsSupported --
 *
 *     Determine whether this guest supports guest-host integration.
 *
 * Results:
 *     TRUE if the guest supports guest-host integration
 *     FALSE otherwise
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHI_IsSupported(void)
{
   return GHIPlatformIsSupported();
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHI_RegisterCaps  --
 *
 *     Called by the application (VMwareUser) to allow the GHI subsystem to
 *     register its capabilities.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

void
GHI_RegisterCaps(void)
{
   /* Register guest platform specific capabilities. */
   GHIPlatformRegisterCaps(ghiPlatformData);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHI_UnregisterCaps  --
 *
 *     Called by the application (VMwareUser) to allow the GHI subsystem to
 *     unregister its capabilities.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

void
GHI_UnregisterCaps(void)
{
   /* Unregister guest platform specific capabilities. */
   GHIPlatformUnregisterCaps(ghiPlatformData);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHI_Init --
 *
 *     One time initialization stuff.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May register with the tools poll loop.
 *
 *-----------------------------------------------------------------------------
 */

void
GHI_Init(VMU_ControllerCB *vmuControllerCB, // IN
         void *ctx)                         // IN
{
   Debug("%s\n", __FUNCTION__);

   DblLnkLst_Init(&launchMenu);

   ghiPlatformData = GHIPlatformInit(vmuControllerCB, ctx);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHI_Cleanup --
 *
 *     One time cleanup.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

void
GHI_Cleanup(void)
{
   GHIPlatformCleanup(ghiPlatformData);
   ghiPlatformData = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHI_InitBackdoor --
 *
 *    One time initialization stuff for the backdoor.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
GHI_InitBackdoor(struct RpcIn *rpcIn)   // IN
{
   /*
    * Only register the callback if the guest is capable of supporting GHI.
    * This way, if the VMX/UI sends us a GHI request on a non-supported platform
    * (for whatever reason), we will reply with 'command not supported'.
    */

   if (GHI_IsSupported()) {
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_GET_BINARY_INFO,
                             GHITcloGetBinaryInfo, NULL);
      RpcIn_RegisterCallbackEx(rpcIn, UNITY_RPC_GET_BINARY_HANDLERS,
                               GHITcloGetBinaryHandlers, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_OPEN_LAUNCHMENU,
                             GHITcloOpenStartMenu, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_GET_LAUNCHMENU_ITEM,
                             GHITcloGetStartMenuItem, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_CLOSE_LAUNCHMENU,
                             GHITcloCloseStartMenu, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_SHELL_OPEN,
                             GHITcloShellOpen, NULL);
      RpcIn_RegisterCallback(rpcIn, GHI_RPC_GUEST_SHELL_ACTION,
                             GHITcloShellAction, NULL);
      RpcIn_RegisterCallbackEx(rpcIn, GHI_RPC_SET_GUEST_HANDLER,
                               GHITcloSetGuestHandler, NULL);
      RpcIn_RegisterCallbackEx(rpcIn, GHI_RPC_RESTORE_DEFAULT_GUEST_HANDLER,
                               GHITcloRestoreDefaultGuestHandler, NULL);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHI_Gather --
 *
 *    Collects all the desired guest/host integration mapping details for
 *    URL Protocol handling and sending an RPC to the host with the collected
 *    details. Also initializes the global application -> filetype list.
 *
 * Result
 *    None.
 *
 * Side-effects
 *    Updates the global application -> filetype list.
 *
 *-----------------------------------------------------------------------------
 */

void
GHI_Gather(void)
{
   GHIProtocolHandlerList protocolHandlers;

   /* Get Protocol Handler information. */
   protocolHandlers.handlers.handlers_len = 0;
   protocolHandlers.handlers.handlers_val = NULL;

   if (!GHIPlatformGetProtocolHandlers(ghiPlatformData, &protocolHandlers)) {
      Debug("Failed to get protocol handler info.\n");
   } else {
      if (!GHIUpdateHost(&protocolHandlers)) {
         Debug("Failed to update the host.\n");
      }
   }

   VMX_XDR_FREE(xdr_GHIProtocolHandlerList, &protocolHandlers);


#ifdef _WIN32
   AppUtil_BuildGlobalApplicationList();
#endif // _WIN32

   Debug("Exited Guest/Host Integration Gather.\n");
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloGetBinaryInfo --
 *
 *     RPC handler for 'unity.get.binary.info'. Get required binary info
 *     and send it back to the VMX.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
GHITcloGetBinaryInfo(char const **result,     // OUT
                     size_t *resultLen,       // OUT
                     const char *name,        // IN
                     const char *args,        // IN
                     size_t argsSize,         // ignored
                     void *clientData)        // ignored

{
   char *binaryPathUtf8;
   DynBuf *buf = &gTcloUpdate;
   unsigned int index = 0;
   Bool ret = TRUE;

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, name, args);

   /* Skip the leading space. */
   index++;

   /* The binary path provided by the VMX is in UTF8. */
   binaryPathUtf8 = StrUtil_GetNextToken(&index, args, "");

   if (!binaryPathUtf8) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      ret = RpcIn_SetRetVals(result, resultLen,
                             "Invalid arguments. Expected \"binary_path\"",
                             FALSE);
      goto exit;
   }

   DynBuf_SetSize(buf, 0);
   if (!GHIPlatformGetBinaryInfo(ghiPlatformData, binaryPathUtf8, buf)) {
      Debug("%s: Could not get binary info.\n", __FUNCTION__);
      ret = RpcIn_SetRetVals(result, resultLen,
                             "Could not get binary info",
                             FALSE);
      goto exit;
   }

   /*
    * Write the final result into the result out parameters and return!
    */
   *result = (char *)DynBuf_Get(buf);
   *resultLen = DynBuf_GetSize(buf);

exit:
   free(binaryPathUtf8);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloGetBinaryHandlers --
 *
 *     RPC handler for 'unity.get.binary.handlers'. Get filetypes supported
 *     by the binary and send it back to the VMX.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
GHITcloGetBinaryHandlers(RpcInData *data)        // IN/OUT
{
   char *binaryPathUtf8;
   XDR xdrs;
   unsigned int index = 0;
   Bool ret = TRUE;

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /* Skip the leading space. */
   index++;

   /* The binary path provided by the VMX is in UTF8. */
   binaryPathUtf8 = StrUtil_GetNextToken(&index, data->args, "");

   if (!binaryPathUtf8) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Invalid arguments. Expected \"binary_path\"", FALSE);
      goto exit;
   }

   DynXdr_Create(&xdrs);
   if (!GHIPlatformGetBinaryHandlers(ghiPlatformData, binaryPathUtf8, &xdrs)) {
      ret = RPCIN_SETRETVALS(data, "Could not get binary filetypes", FALSE);
      DynXdr_Destroy(&xdrs, FALSE);
      goto exit;
   }

   /*
    * If the serialized data exceeds our maximum message size we have little choice
    * but to fail the request and log the oversize message.
    */
   if (xdr_getpos(&xdrs) > GUESTMSG_MAX_IN_SIZE) {
      ret = RPCIN_SETRETVALS(data, "Filetype list too large", FALSE);
      DynXdr_Destroy(&xdrs, FALSE);
      goto exit;
   }

   /*
    * Write the final result into the result out parameters and return!
    */
    data->result = DynXdr_Get(&xdrs);
    data->resultLen = xdr_getpos(&xdrs);
    data->freeResult = TRUE;
    ret = TRUE;

    /*
     * Destroy the XDR structure but leave the data buffer alone since it will be
     * freed by the RpcIn layer.
     */
    DynXdr_Destroy(&xdrs, FALSE);
exit:
   free(binaryPathUtf8);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloOpenStartMenu --
 *
 *     RPC handler for 'unity.launchmenu.open'. Get the start menu sub-tree
 *     for a given item, save it in the array so it can be accessed
 *     later when the VMX needs to iterate over the items. Return the count
 *     of the items in the sub-tree and a handle to this sub-tree. The handle
 *     will be used by the VMX to iterate over the sub-items.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
GHITcloOpenStartMenu(char const **result,     // OUT
                     size_t *resultLen,       // OUT
                     const char *name,        // IN
                     const char *args,        // IN
                     size_t argsSize,         // ignored
                     void *clientData)        // ignored
{
   char *rootUtf8 = NULL;
   DynBuf *buf = &gTcloUpdate;
   uint32 index = 0;
   Bool ret = TRUE;

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, name, args);

   /* Skip the leading space. */
   index++;

   /* The start menu root provided by the VMX is in UTF8. */
   rootUtf8 = StrUtil_GetNextToken(&index, args, "");

   if (!rootUtf8) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      ret = RpcIn_SetRetVals(result, resultLen,
                             "Invalid arguments. Expected \"root\"",
                             FALSE);
      goto exit;
   }

   DynBuf_SetSize(buf, 0);
   if (!GHIPlatformOpenStartMenuTree(ghiPlatformData, rootUtf8, buf)) {
      Debug("%s: Could not open start menu.\n", __FUNCTION__);
      ret = RpcIn_SetRetVals(result, resultLen,
                             "Could not get start menu count",
                             FALSE);
      goto exit;
   }

   /*
    * Write the final result into the result out parameters and return!
    */
   *result = (char *)DynBuf_Get(buf);
   *resultLen = DynBuf_GetSize(buf);

exit:
   free(rootUtf8);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloGetStartMenuItem --
 *
 *     RPC handler for 'unity.launchmenu.next'. Get the start menu item
 *     at the given index for the tree with a given handle.
 *     If there's no item at the given index, return FALSE.
 *
 * Results:
 *     TRUE if the item was found.
 *     FALSE otherwise (i.e. if the VMX provides a wrong handle or if there's
                        no items left).
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
GHITcloGetStartMenuItem(char const **result,     // OUT
                        size_t *resultLen,       // OUT
                        const char *name,        // IN
                        const char *args,        // IN
                        size_t argsSize,         // ignored
                        void *clientData)        // ignored
{
   DynBuf *buf = &gTcloUpdate;
   uint32 index = 0;
   Bool ret = TRUE;
   uint32 itemIndex = 0;
   uint32 handle = 0;

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, name, args);

   /* Parse the handle of the menu tree that VMX wants. */
   if (!StrUtil_GetNextUintToken(&handle, &index, args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments. Expected handle index",
                              FALSE);
   }

   /* The index of the menu item to be send back. */
   if (!StrUtil_GetNextUintToken(&itemIndex, &index, args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments. Expected handle index",
                              FALSE);
   }

   DynBuf_SetSize(buf, 0);
   if (!GHIPlatformGetStartMenuItem(ghiPlatformData, handle, itemIndex, buf)) {
      Debug("%s: Could not get start menu item.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Could not get start menu item",
                              FALSE);
   }

   /*
    * Write the final result into the result out parameters and return!
    */
   *result = (char *)DynBuf_Get(buf);
   *resultLen = DynBuf_GetSize(buf);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloCloseStartMenu --
 *
 *     RPC handler for 'unity.launchmenu.close'. The VMX is done with this
 *     particular start menu tree. Free all memory and cleanup.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Memory allocated when the start menu tree was opened is finally freed.
 *
 *----------------------------------------------------------------------------
 */

static Bool
GHITcloCloseStartMenu(char const **result,     // OUT
                      size_t *resultLen,       // OUT
                      const char *name,        // IN
                      const char *args,        // IN
                      size_t argsSize,         // ignored
                      void *clientData)        // ignored
{
   uint32 index = 0;
   uint32 handle = 0;

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, name, args);

   /* Parse the handle of the menu tree that VMX wants. */
   if (!StrUtil_GetNextIntToken(&handle, &index, args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments. Expected handle",
                              FALSE);
   }

   GHIPlatformCloseStartMenuTree(ghiPlatformData, handle);

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloShellOpen --
 *
 *     RPC handler for 'unity.shell.open'. Open the specified file with the
 *     default shell handler. Note that the file path may be either a URI
 *     (originated with Tools >= NNNNN), or a regular path (originated with
 *     Tools < NNNNN).
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
GHITcloShellOpen(char const **result, // OUT
                 size_t *resultLen,   // OUT
                 const char *name,    // IN
                 const char *args,    // IN
                 size_t argsSize,     // ignored
                 void *clientData)    // ignored
{
   char *fileUtf8 = NULL;
   Bool ret = TRUE;
   unsigned int index = 0;

   Debug("%s: name: '%s', args: '%s'\n", __FUNCTION__, name, args);

   /* Skip the leading space. */
   index++;

   /* The file path provided by the VMX is in UTF8. */
   fileUtf8 = StrUtil_GetNextToken(&index, args, "");

   if (!fileUtf8) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments. Expected file_name",
                              FALSE);
   }

   ret = GHIPlatformShellOpen(ghiPlatformData, fileUtf8);
   free(fileUtf8);

   if (!ret) {
      Debug("%s: Could not perform the requested shell open action.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Could not perform the requested shell open action.",
                              FALSE);
   }

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloShellAction --
 *
 *     RPC handler for "ghi.guest.shell.action". The action command has three
 *     arguments: an action URI, a target URI, and an array of location URIs.
 *     Action URIs are in the form: "x-vmware-action://<verb>", where <verb> is
 *     the name of a specific action to perform.
 *     The target URI is a guest-specific URI that was previously given to the
 *     host (usually a path to an application to run). Note that this may be
 *     either a URI (new Tools) or a regular path (old Tools).
 *     The locations can be files or URLs. Files are typically specified as
 *     HGFS shared folder locations (see below), but can potentially use the
 *     "file://<path>" URIs as well.
 *     Each guest can specify the features it supports using capability flags:
 *
 *     Capability                      Description
 *
 *     GHI_CAP_CMD_SHELL_ACTION        Guest allows 'ghi.guest.shell.action'.
 *                                     This encompasses this entire command
 *                                     and the rest of the capabilities.
 *
 *     GHI_CAP_SHELL_ACTION_BROWSE     Guest supports the 'browse' action verb,
 *                                     used to open a file browser window with
 *                                     a given set of locations.
 *
 *     GHI_CAP_SHELL_ACTION_RUN        Guest supports the 'run' action verb,
 *                                     used for running applications as well
 *                                     as opening file or URL locations.
 *
 *     GHI_CAP_SHELL_LOCATION_HGFS     Guest supports HGFS share location URIs:
 *                                     "x-vmware-share://<path>", where <path>
 *                                     specifies a shared folder name and an
 *                                     optional path within the shared folder.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
GHITcloShellAction(char const **result, // OUT
                   size_t *resultLen,   // OUT
                   const char *name,    // IN
                   const char *args,    // IN
                   size_t argsSize,     // IN
                   void *clientData)    // ignored
{
   Bool ret = TRUE;
   XDR xdrs;

   /*
    * Build an XDR Stream from the argument data which beings are args + 1
    * since there is a space separator between the RPC name and the XDR serialization.
    */
   xdrmem_create(&xdrs, (char *) args + 1, argsSize - 1, XDR_DECODE);

   ret = GHIPlatformShellAction(ghiPlatformData, &xdrs);

   xdr_destroy(&xdrs);

   if (!ret) {
      Debug("%s: Could not perform the requested shell action.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Could not perform the requested shell action.",
                              FALSE);
   }

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloSetGuestHandler --
 *
 *     RPC handler for 'ghi.guest.handler.set'. Changes the nominated handlerType
 *     to use the VMwareHostOpen proxy app to open files or URLs in the host.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
GHITcloSetGuestHandler(RpcInData *data)        // IN/OUT
{
   Bool ret = FALSE;
   XDR xdrs;

   Debug("%s name:%s args length: %"FMTSZ"u\n", __FUNCTION__, data->name, data->argsSize);

   /*
    * Build an XDR Stream from the argument data which beings are args + 1
    * since there is a space separator between the RPC name and the XDR serialization.
    */
   xdrmem_create(&xdrs, (char *) data->args + 1, data->argsSize - 1, XDR_DECODE);
   ret = GHIPlatformSetGuestHandler(ghiPlatformData, &xdrs);
   xdr_destroy(&xdrs);

   if (ret == FALSE) {
      Debug("%s: Unable to set guest handler\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Unable to set guest handler", FALSE);
      goto exit;
   }
   /*
    * Write the final result into the result out parameters and return!
    */
    data->result = "";
    data->resultLen = 0;
    data->freeResult = FALSE;
    ret = TRUE;

exit:
    return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloRestoreDefaultGuestHandler --
 *
 *     RPC handler for 'ghi.guest.handler.restoreDefault'. Changes the nominated
 *     handlerType back to the value in use prior to any changes by tools.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
GHITcloRestoreDefaultGuestHandler(RpcInData *data)        // IN/OUT
{
   Bool ret = FALSE;
   XDR xdrs;

   Debug("%s name:%s args length: %"FMTSZ"u\n", __FUNCTION__, data->name, data->argsSize);

   /*
    * Build an XDR Stream from the argument data which beings are args + 1
    * since there is a space separator between the RPC name and the XDR serialization.
    */
   xdrmem_create(&xdrs, (char *) data->args + 1, data->argsSize - 1, XDR_DECODE);
   ret = GHIPlatformRestoreDefaultGuestHandler(ghiPlatformData, &xdrs);
   xdr_destroy(&xdrs);

   if (ret == FALSE) {
      Debug("%s: Unable to restore guest handler\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Unable to restore guest handler", FALSE);
      goto exit;
   }
   /*
    * Write the final result into the result out parameters and return!
    */
    data->result = "";
    data->resultLen = 0;
    data->freeResult = FALSE;
    ret = TRUE;

exit:
    return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHILaunchMenuChangeRPC --
 *
 *     Informs host that one or more Launch Menu changes have been detected.
 *
 * Results:
 *     TRUE on success
 *     FALSE on error
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
GHILaunchMenuChangeRPC(void)
{
   if (!RpcOut_sendOne(NULL, NULL, GHI_RPC_LAUNCHMENU_CHANGE)) {
      Debug("%s: could not send unity launchmenu change\n", __FUNCTION__);
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIUpdateHost --
 *
 *    Update the host with new guest/host integration information.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    VMDB is updated if the given value has changed.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GHIUpdateHost(GHIProtocolHandlerList *handlers) // IN: type specific information
{
   /* +1 for the space separator */
   char request[sizeof GHI_RPC_PROTOCOL_HANDLER_INFO + 1];
   Bool status;
   XDR xdrs;

   ASSERT(handlers);

   if (DynXdr_Create(&xdrs) == NULL) {
      return FALSE;
   }

   Str_Sprintf(request,
               sizeof request,
               "%s ",
               GHI_RPC_PROTOCOL_HANDLER_INFO);

   /* Write preamble and serialized protocol handler info to XDR stream. */
   if (!DynXdr_AppendRaw(&xdrs, request, strlen(request)) ||
       !xdr_GHIProtocolHandlerList(&xdrs, handlers)) {
      Debug("%s: could not serialize protocol handler info\n", __FUNCTION__);
      DynXdr_Destroy(&xdrs, TRUE);
      return FALSE;
   }

   status = RpcOut_SendOneRaw(DynXdr_Get(&xdrs),
                              xdr_getpos(&xdrs),
                              NULL,
                              NULL);
   DynXdr_Destroy(&xdrs, TRUE);

   if (!status) {
      Debug("%s: failed to update protocol handler information\n",
            __FUNCTION__);
   }
   return status;
}
