/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * ghiTclo.cpp --
 *
 *    Guest-host integration functions.
 */

#include "appUtil.h"
#include "ghIntegration.h"
#include "ghiTclo.h"

extern "C" {
#include "vmware.h"
#include "vmware/tools/guestrpc.h"
#include "debug.h"
#include "dynxdr.h"
#include "guest_msg_def.h"
#include "rpcin.h"
#include "rpcout.h"
#include "str.h"
#include "strutil.h"
#include "unityCommon.h"
#include "util.h"
#include "xdrutil.h"
};

#include "guestrpc/ghiGetBinaryHandlers.h"
#include "guestrpc/ghiGetExecInfoHash.h"
#include "guestrpc/ghiProtocolHandler.h"
#include "guestrpc/ghiSetFocusedWindow.h"
#include "guestrpc/ghiSetGuestHandler.h"
#include "guestrpc/ghiSetOutlookTempFolder.h"
#include "guestrpc/ghiShellAction.h"
#include "guestrpc/ghiStartMenu.h"
#include "guestrpc/ghiTrayIcon.h"
#include "vmware/guestrpc/capabilities.h"

static DynBuf gTcloUpdate;

/*
 * Overhead of encoding the icon data in a dynbuf - used to make sure we don't
 * exceed GUEST_MSG_MAX_IN_SIZE when serializing the icons for an app.
 */
static const int GHI_ICON_OVERHEAD = 1024;


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloInit --
 *
 *     Initialize the global state (a Dynbuf) used to handle the TCLO parsing
 *     and dispatch.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
GHITcloInit()
{
   DynBuf_Init(&gTcloUpdate);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloCleanup --
 *
 *     Cleanup the global state (a Dynbuf) used to handle the TCLO parsing
 *     and dispatch.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
GHITcloCleanup()
{
   DynBuf_Destroy(&gTcloUpdate);
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

gboolean
GHITcloGetBinaryInfo(RpcInData *data)        // IN/OUT
{
   char *binaryPathUtf8;
   DynBuf *buf = &gTcloUpdate;
   DynBuf iconDataBuf;
   unsigned int index = 0;
   Bool ret = TRUE;
   std::string friendlyName;
   std::list<GHIBinaryIconInfo> iconList;
   char temp[128];   // Used to hold sizes and indices as strings
   uint32 serializedIconCount = 0;

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /* Skip the leading space. */
   index++;

   /* The binary path provided by the VMX is in UTF8. */
   binaryPathUtf8 = StrUtil_GetNextToken(&index, data->args, "");

   if (!binaryPathUtf8) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data,
                             "Invalid arguments. Expected \"binary_path\"",
                             FALSE);
      goto exit;
   }

   DynBuf_SetSize(buf, 0);

   if (!GHI_GetBinaryInfo(binaryPathUtf8, friendlyName, iconList)) {
      Debug("%s: Could not get binary info.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data,
                             "Could not get binary info",
                             FALSE);
      goto exit;
   }

   /*
    * Append the name to the output buffer now. If we fail to get the
    * icons, we still want to return the app name. Then the UI can display
    * the default icon and correct app name.
    *
    * The output buffer should look like this:
    * <name>\0<icon count>\0<width>\0<height>\0<size>\0<bgraData>\0...
    *
    * Note that the icon data is in BGRA format. An alpha channel value of 255 means
    * "fully opaque", and an alpha channel value of 0 means "fully transparent".
    */

   DynBuf_AppendString(buf, friendlyName.c_str());

   if (iconList.size() <= 0) {
      Debug("%s: Could not find any icons for path: %s", __FUNCTION__, binaryPathUtf8);
   }

   DynBuf_Init(&iconDataBuf);
   /* Copy icon info to the output buffer. */
   for (std::list<GHIBinaryIconInfo>::const_iterator it = iconList.begin();
        it != iconList.end();
        it++) {
      /*
       * XXX: The backdoor has a maximum RPC data size of 64K - don't attempt to send
       * icons larger than this size.
       */
      if ((DynBuf_GetSize(&iconDataBuf) + it->dataBGRA.size()) <
           GUESTMSG_MAX_IN_SIZE - GHI_ICON_OVERHEAD) {
         Str_Sprintf(temp, sizeof temp, "%u", it->width);
         DynBuf_AppendString(&iconDataBuf, temp);

         Str_Sprintf(temp, sizeof temp, "%u", it->height);
         DynBuf_AppendString(&iconDataBuf, temp);

         Str_Sprintf(temp, sizeof temp, "%u", (int32) it->dataBGRA.size());
         DynBuf_AppendString(&iconDataBuf, temp);

         DynBuf_Append(&iconDataBuf, &(it->dataBGRA[0]),
                       it->dataBGRA.size());
         DynBuf_AppendString(&iconDataBuf, "");

         serializedIconCount++;
      }
   }

   Str_Sprintf(temp, sizeof temp, "%d", serializedIconCount);
   DynBuf_AppendString(buf, temp);

   /* Append the icon data */
   DynBuf_Append(buf, DynBuf_Get(&iconDataBuf), DynBuf_GetSize(&iconDataBuf));
   DynBuf_Destroy(&iconDataBuf);

   /*
    * Write the final result into the result out parameters and return!
    */
   data->result = (char *)DynBuf_Get(buf);
   data->resultLen = DynBuf_GetSize(buf);

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

gboolean
GHITcloGetBinaryHandlers(RpcInData *data)        // IN/OUT
{
   char *binaryPathUtf8;
   XDR xdrs;
   unsigned int index = 0;
   Bool ret = TRUE;
   GHIBinaryHandlersList *handlersList = NULL;

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /* Skip the leading space. */
   index++;

   /* The binary path provided by the VMX is in UTF8. */
   binaryPathUtf8 = StrUtil_GetNextToken(&index, data->args, "");

   if (!binaryPathUtf8) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments. Expected \"binary_path\"", FALSE);
   }

   handlersList = (GHIBinaryHandlersList *) Util_SafeCalloc(1, sizeof *handlersList);
   ASSERT_MEM_ALLOC(DynXdr_Create(&xdrs));

#if !defined(OPEN_VM_TOOLS)
   FileTypeList::const_iterator fileTypeIterator;
   FileTypeList aFileTypeList;

   aFileTypeList = GHI_GetBinaryHandlers(binaryPathUtf8);

   /*
    * Take the list of filetypes handled by this application and convert it into
    * the XDR based structure that we'll then serialize
    */

   int fileTypeCount;
   for (fileTypeIterator = aFileTypeList.begin(), fileTypeCount = 0;
        (fileTypeIterator != aFileTypeList.end()) &&
         (fileTypeCount < GHI_MAX_NUM_BINARY_HANDLERS);
        ++fileTypeIterator, ++fileTypeCount)
   {
      std::string extension = (*fileTypeIterator)->Extension();
      size_t extensionStringLength = strlen(extension.c_str()) + 1;
      std::string actionURI = (*fileTypeIterator)->GetActionURIList().front();
      size_t actionURILength = strlen(actionURI.c_str()) + 1;

      /*
       * Copy the handlers suffix/extension string.
       */
      GHIBinaryHandlersDetails *aHandler = (GHIBinaryHandlersDetails *)
                                                XDRUTIL_ARRAYAPPEND(handlersList,
                                                                    handlers,
                                                                    1);
      ASSERT_MEM_ALLOC(aHandler);
      aHandler->suffix = (char *) Util_SafeCalloc(extensionStringLength,
                                                  sizeof *aHandler->suffix);
      Str_Strcpy(aHandler->suffix, extension.c_str(), extensionStringLength);

      /*
       * Empty strings for all the other 'type' fields. Note that we cannot leave the
       * string pointer as NULL, we must encode an empty string for XDR to work.
       */
      aHandler->mimetype = (char *) Util_SafeCalloc(1, sizeof *aHandler->mimetype);
      aHandler->mimetype[0] = '\0';
      aHandler->UTI = (char *) Util_SafeCalloc(1, sizeof *aHandler->UTI);
      aHandler->UTI[0] = '\0';

      /*
       * Set the Action URI and friendly name for this type
       */
      GHIBinaryHandlersActionURIPair *anActionPair = (GHIBinaryHandlersActionURIPair *)
                                                         XDRUTIL_ARRAYAPPEND(aHandler,
                                                                             actionURIs,
                                                                             1);
      ASSERT_MEM_ALLOC(anActionPair);
      anActionPair->actionURI = (char *) Util_SafeCalloc(actionURILength,
                                                         sizeof *anActionPair->actionURI);
      Str_Strcpy(anActionPair->actionURI, actionURI.c_str(), actionURILength);
      anActionPair->verb = Util_SafeStrdup("run");

      std::string friendlyName = (*fileTypeIterator)->FriendlyName();
      size_t friendlyNameLength = strlen(friendlyName.c_str()) + 1;
      aHandler->friendlyName = (char *) Util_SafeCalloc(friendlyNameLength,
                                                        sizeof *aHandler->friendlyName);
      Str_Strcpy(aHandler->friendlyName, friendlyName.c_str(), friendlyNameLength);

      /*
       * Store the list of icon dimensions and URIs.
       * TODO: Retrieve the list of icons and their dimensions for this filetype.
       */
   }
#endif // OPEN_VM_TOOLS

   GHIBinaryHandlers message;
   message.ver = GHI_BINARY_HANDLERS_V1;
   message.GHIBinaryHandlers_u.handlersV1 = handlersList;

   ret = xdr_GHIBinaryHandlers(&xdrs, &message);
   if (ret == FALSE) {
      Debug("%s: Failed to serialize binary handlers list.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Failed to serialize binary handlers list.", FALSE);
      goto exitWithXDRCleanup;
   }

   /*
    * If the serialized data exceeds our maximum message size we have little choice
    * but to fail the request and log the oversize message.
    */
   if (xdr_getpos(&xdrs) > GUESTMSG_MAX_IN_SIZE) {
      ret = RPCIN_SETRETVALS(data, "Filetype list too large", FALSE);
      goto exitWithXDRCleanup;
   }

   /*
    * Write the final result into the result out parameters and return!
    */
    data->result = reinterpret_cast<char *>(DynXdr_Get(&xdrs));
    data->resultLen = xdr_getpos(&xdrs);
    data->freeResult = TRUE;
    ret = TRUE;

exitWithXDRCleanup:
   VMX_XDR_FREE(xdr_GHIBinaryHandlers, &message);
    /*
     * Destroy the XDR structure but leave the data buffer alone since it will be
     * freed by the RpcIn layer.
     */
    DynXdr_Destroy(&xdrs, FALSE);
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

gboolean
GHITcloOpenStartMenu(RpcInData *data)        // IN/OUT
{
   char *rootUtf8 = NULL;
   DynBuf *buf = &gTcloUpdate;
   uint32 flags = 0;
   uint32 index = 0;
   Bool ret = TRUE;

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /* Skip the leading space. */
   index++;

   /* The start menu root provided by the VMX is in UTF8. */
   rootUtf8 = StrUtil_GetNextToken(&index, data->args, "");

   if (!rootUtf8) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data,
                             "Invalid arguments. Expected \"root\"",
                             FALSE);
      goto exit;
   }

   /*
    * Skip the NULL after the root, and look for the flags. Old versions of
    * the VMX don't send this parameter, so it's not an error if it is not
    * present in the RPC.
    */
   if (++index < data->argsSize && sscanf(data->args + index, "%u", &flags) != 1) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data,
                             "Invalid arguments. Expected flags",
                             FALSE);
      goto exit;
   }

   DynBuf_SetSize(buf, 0);
   if (!GHI_OpenStartMenuTree(rootUtf8, flags, buf)) {
      Debug("%s: Could not open start menu.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data,
                             "Could not get start menu count",
                             FALSE);
      goto exit;
   }

   /*
    * Write the final result into the result out parameters and return!
    */
   data->result = (char *)DynBuf_Get(buf);
   data->resultLen = DynBuf_GetSize(buf);

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

gboolean
GHITcloGetStartMenuItem(RpcInData *data)        // IN/OUT
{
   DynBuf *buf = &gTcloUpdate;
   uint32 index = 0;
   Bool ret = TRUE;
   uint32 itemIndex = 0;
   uint32 handle = 0;

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /* Parse the handle of the menu tree that VMX wants. */
   if (!StrUtil_GetNextUintToken(&handle, &index, data->args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected handle index",
                              FALSE);
   }

   /* The index of the menu item to be send back. */
   if (!StrUtil_GetNextUintToken(&itemIndex, &index, data->args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected handle index",
                              FALSE);
   }

   DynBuf_SetSize(buf, 0);
   if (!GHI_GetStartMenuItem(handle, itemIndex, buf)) {
      Debug("%s: Could not get start menu item.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Could not get start menu item",
                              FALSE);
   }

   /*
    * Write the final result into the result out parameters and return!
    */
   data->result = (char *)DynBuf_Get(buf);
   data->resultLen = DynBuf_GetSize(buf);

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

gboolean
GHITcloCloseStartMenu(RpcInData *data)        // IN/OUT
{
   uint32 index = 0;
   int32 handle = 0;

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   Debug("%s name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /* Parse the handle of the menu tree that VMX wants. */
   if (!StrUtil_GetNextIntToken(&handle, &index, data->args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected handle",
                              FALSE);
   }

   GHI_CloseStartMenuTree(handle);

   return RPCIN_SETRETVALS(data, "", TRUE);
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

gboolean
GHITcloShellOpen(RpcInData *data)    // IN/OUT
{
   char *fileUtf8 = NULL;
   Bool ret = TRUE;
   unsigned int index = 0;

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   Debug("%s: name: '%s', args: '%s'\n", __FUNCTION__, data->name, data->args);

   /* Skip the leading space. */
   index++;

   /* The file path provided by the VMX is in UTF8. */
   fileUtf8 = StrUtil_GetNextToken(&index, data->args, "");

   if (!fileUtf8) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected file_name",
                              FALSE);
   }

   ret = GHI_ShellOpen(fileUtf8);
   free(fileUtf8);

   if (!ret) {
      Debug("%s: Could not perform the requested shell open action.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Could not perform the requested shell open action.",
                              FALSE);
   }

   return RPCIN_SETRETVALS(data, "", TRUE);
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

gboolean
GHITcloShellAction(RpcInData *data)    // IN/OUT
{
   Bool ret = TRUE;
   GHIShellAction shellActionMsg;
   GHIShellActionV1 *shellActionV1Ptr;
   memset(&shellActionMsg, 0, sizeof shellActionMsg);

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   /*
    * Build an XDR Stream from the argument data which begins are args + 1
    * since there is a space separator between the RPC name and the XDR serialization.
    */
   if (!XdrUtil_Deserialize((char *)data->args + 1, data->argsSize - 1,
                            (void *)xdr_GHIShellAction, &shellActionMsg)) {
      Debug("%s: Failed to deserialize data\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data,
                             "Failed to deserialize data.",
                             FALSE);
      goto exit;
   }

   ASSERT(shellActionMsg.ver == GHI_SHELL_ACTION_V1);
   if (shellActionMsg.ver != GHI_SHELL_ACTION_V1) {
      Debug("%s: Unexpected XDR version = %d\n", __FUNCTION__, shellActionMsg.ver);
      ret = RPCIN_SETRETVALS(data,
                             "Unexpected XDR version.",
                             FALSE);
      goto exit;
   }

   shellActionV1Ptr = shellActionMsg.GHIShellAction_u.actionV1;
   ret = GHI_ShellAction(shellActionV1Ptr->actionURI,
                         shellActionV1Ptr->targetURI,
                         (const char **)shellActionV1Ptr->locations.locations_val,
                         shellActionV1Ptr->locations.locations_len);
   if (!ret) {
      Debug("%s: Could not perform the requested shell action.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data,
                             "Could not perform the requested shell action.",
                             FALSE);
      goto exit;
   }

   ret = RPCIN_SETRETVALS(data, "", TRUE);

exit:
   VMX_XDR_FREE(xdr_GHIShellAction, &shellActionMsg);

   return ret;
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

gboolean
GHITcloSetGuestHandler(RpcInData *data)        // IN/OUT
{
   Bool ret = FALSE;
   XDR xdrs;

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   Debug("%s name:%s args length: %"FMTSZ"u\n", __FUNCTION__, data->name, data->argsSize);

   /*
    * Build an XDR Stream from the argument data which beings are args + 1
    * since there is a space separator between the RPC name and the XDR serialization.
    */
   xdrmem_create(&xdrs, (char *) data->args + 1, data->argsSize - 1, XDR_DECODE);

   GHISetGuestHandler setGuestHandlerMsg;
   GHISetGuestHandlerV1 *setGuestHandlerV1Ptr;
   GHISetGuestHandlerAction *aHandlerAction;

   memset(&setGuestHandlerMsg, 0, sizeof setGuestHandlerMsg);
   if (!xdr_GHISetGuestHandler(&xdrs, &setGuestHandlerMsg)) {
      Debug("%s: Unable to deserialize data\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Unable to deserialize data.", FALSE);
      goto exitWithXDRCleanup;
   }

   ASSERT(setGuestHandlerMsg.ver == GHI_SET_GUEST_HANDLER_V1);
   setGuestHandlerV1Ptr = setGuestHandlerMsg.GHISetGuestHandler_u.guestHandlerV1;
   aHandlerAction = setGuestHandlerV1Ptr->actionURIs.actionURIs_val;

   ret = GHI_SetGuestHandler(setGuestHandlerV1Ptr->suffix,
                             setGuestHandlerV1Ptr->mimetype,
                             setGuestHandlerV1Ptr->UTI,
                             aHandlerAction->actionURI,
                             aHandlerAction->targetURI);

   if (ret == FALSE) {
      Debug("%s: Unable to set guest handler\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Unable to set guest handler", FALSE);
      goto exitWithXDRCleanup;
   }
   /*
    * Write the final result into the result out parameters and return!
    */
    data->result = "";
    data->resultLen = 0;
    data->freeResult = FALSE;
    ret = TRUE;

exitWithXDRCleanup:
   VMX_XDR_FREE(xdr_GHISetGuestHandler, &setGuestHandlerMsg);
   xdr_destroy(&xdrs);
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

gboolean
GHITcloRestoreDefaultGuestHandler(RpcInData *data)        // IN/OUT
{
   Bool ret = FALSE;
   XDR xdrs;

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   Debug("%s name:%s args length: %"FMTSZ"u\n", __FUNCTION__, data->name, data->argsSize);

   /*
    * Build an XDR Stream from the argument data which beings are args + 1
    * since there is a space separator between the RPC name and the XDR serialization.
    */
   xdrmem_create(&xdrs, (char *) data->args + 1, data->argsSize - 1, XDR_DECODE);

   GHIRestoreDefaultGuestHandler restoreMsg;
   GHIRestoreDefaultGuestHandlerV1 *restoreV1Ptr = NULL;

   memset(&restoreMsg, 0, sizeof restoreMsg);
   if (!xdr_GHIRestoreDefaultGuestHandler(&xdrs, &restoreMsg)) {
      Debug("%s: Unable to deserialize data\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Unable to deserialize data", FALSE);
      goto exit;
   }

   ASSERT(restoreMsg.ver == GHI_SET_GUEST_HANDLER_V1);
   restoreV1Ptr = restoreMsg.GHIRestoreDefaultGuestHandler_u.defaultHandlerV1;

   ret = GHI_RestoreDefaultGuestHandler(restoreV1Ptr->suffix,
                                        restoreV1Ptr->mimetype,
                                        restoreV1Ptr->UTI);
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
   xdr_destroy(&xdrs);
   VMX_XDR_FREE(xdr_GHIRestoreDefaultGuestHandler, &restoreMsg);
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
GHILaunchMenuChangeRPC(int numFolderKeys,                // IN
                       const char **folderKeysChanged)   // IN
{
   /* +1 for the space separator */
   char request[sizeof GHI_RPC_LAUNCHMENU_CHANGE + 1];
   XDR xdrs;
   GHIStartMenuChanged startMenuChanged;
   GHIStartMenuChangedV1 smcv1;
   Bool status;

   /*
    * Note: The primary contents of the startMenuChanged - the folder keys,
    * are allocated and tracked outside of this function, there's no need to call
    * VMX_XDR_FREE here for the XDR contents, in fact if called it will delete memory
    * from other parts of the system.
    */

   memset(&startMenuChanged, 0, sizeof startMenuChanged);
   memset(&smcv1, 0, sizeof smcv1);

   ASSERT_MEM_ALLOC(DynXdr_Create(&xdrs));

   smcv1.keys.keys_len = numFolderKeys;
   smcv1.keys.keys_val = (char **) folderKeysChanged;

   startMenuChanged.ver = GHI_STARTMENU_CHANGED_V1;
   startMenuChanged.GHIStartMenuChanged_u.ghiStartMenuChangedV1 = &smcv1;

   Str_Sprintf(request,
               sizeof request,
               "%s ",
               GHI_RPC_LAUNCHMENU_CHANGE);

   /* Write preamble and serialized changed folder keys to XDR stream. */
   if (!DynXdr_AppendRaw(&xdrs, request, strlen(request)) ||
       !xdr_GHIStartMenuChanged(&xdrs, &startMenuChanged)) {
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
      Debug("%s: could not send unity launchmenu change\n", __FUNCTION__);
   }
   return status;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloSetOutlookTempFolder --
 *
 *    Handler for the 'ghi.guest.outlook.set.tempFolder' RPC.
 *
 * Results:
 *    If the RPC fails, return FALSE. Otherwise, returns TRUE.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

gboolean
GHITcloSetOutlookTempFolder(RpcInData *data) // IN/OUT: RPC data
{
   Bool ret = FALSE;
   XDR xdrs;

   Debug("%s: Enter.\n", __FUNCTION__);

   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   ASSERT(data->name);
   ASSERT(data->args);

   if (!data->name || !data->args) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
   }

   Debug("%s: Got RPC, name: \"%s\", argument length: %"FMTSZ"u.\n",
         __FUNCTION__, data->name, data->argsSize);

   /*
    * Build an XDR stream from the argument data.
    *
    * Note that the argument data begins with args + 1 since there is a space
    * between the RPC name and the XDR serialization.
    */
   xdrmem_create(&xdrs, (char*) data->args + 1, data->argsSize - 1, XDR_DECODE);

   /* Deserialize the XDR into a GHISetOutlookTempFolder struct. */
   GHISetOutlookTempFolder folderMsg;
   GHISetOutlookTempFolderV1 *folderV1Ptr = NULL;

   memset(&folderMsg, 0, sizeof folderMsg);
   if (!xdr_GHISetOutlookTempFolder(&xdrs, &folderMsg)) {
      Debug("%s: Unable to deserialize data\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Unable to deserialize data", FALSE);
      goto exit;
   }

   /*
    * Get the structure for v1 of the GHISetOutlookTempFolder XDR from the union
    * in the GHISetOutlookTempFolder struct.
    */
   ASSERT(folderMsg.ver == GHI_SET_OUTLOOK_TEMP_FOLDER_V1);
   folderV1Ptr = folderMsg.GHISetOutlookTempFolder_u.setOutlookTempFolderV1;

   // Call the platform implementation of our RPC.
   ret = GHI_SetOutlookTempFolder(folderV1Ptr->targetURI);
   if (ret == FALSE) {
      Debug("%s: Failed to set Outlook temporary folder.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Failed to set Outlook temporary folder", FALSE);
      goto exit;
   }

   /*
    * We don't have any out parameters, so we write empty values into the
    * result fields of the RpcInData structure.
    */
   RPCIN_SETRETVALS(data, "", FALSE);

   // Set our return value and return to the caller.
   ret = TRUE;

exit:
   // Destroy the XDR stream.
   xdr_destroy(&xdrs);
   VMX_XDR_FREE(xdr_GHISetOutlookTempFolder, &folderMsg);
   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHITcloRestoreOutlookTempFolder --
 *
 *    Handler for the 'ghi.guest.outlook.restore.tempFolder' RPC.
 *
 * Results:
 *    If the RPC fails, return FALSE. Otherwise, returns TRUE.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

gboolean
GHITcloRestoreOutlookTempFolder(RpcInData *data) // IN/OUT: RPC data
{
   Bool ret = FALSE;

   Debug("%s: Enter.\n", __FUNCTION__);

   /*
    * XXX: This RPC is no longer used/required - the RPC handler is left here
    * for compatibility with older hosts.
    */

   /*
    * We don't have any out parameters, so we write empty values into the
    * result fields of the RpcInData structure.
    */
   RPCIN_SETRETVALS(data, "", FALSE);

   // Set our return value and return to the caller.
   ret = TRUE;

   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHITcloTrayIconSendEvent --
 *
 *      Send a mouse or keyboard event to a tray icon.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      XXX.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
GHITcloTrayIconSendEvent(RpcInData *data) // IN/OUT: XXX
{
   Bool ret = FALSE;
   GHITrayIconEventV1 *v1ptr = NULL;
   GHITrayIconEvent eventMsg;

   memset(&eventMsg, 0, sizeof eventMsg);
   Debug("%s: Enter.\n", __FUNCTION__);

   /* Check our arguments. */
   ASSERT(data);
   ASSERT(data->name);
   ASSERT(data->argsSize > 0);

   if (!(data && data->name && data->argsSize > 0)) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      return FALSE;
   }

   Debug("%s: Got RPC, name: \"%s\", argument length: %"FMTSZ"u.\n",
         __FUNCTION__, data->name, data->argsSize);

   /*
    * Deserialize the XDR data. Note that the data begins with args + 1 since
    * there is a space between the RPC name and the XDR serialization.
    */
   if (!XdrUtil_Deserialize((char *)data->args + 1, data->argsSize - 1,
                            (void *)xdr_GHITrayIconEvent, &eventMsg)) {
      Debug("%s: Failed to deserialize data\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Failed to deserialize data.", FALSE);
      goto exit;
   }
   ASSERT(eventMsg.ver == GHI_TRAY_ICON_EVENT_V1);
   v1ptr = eventMsg.GHITrayIconEvent_u.trayIconEventV1;

   /* Call the platform implementation of our RPC. */
   ret = GHI_TrayIconSendEvent(v1ptr->iconID,
                               v1ptr->event,
                               v1ptr->x,
                               v1ptr->y);

   if (ret == FALSE) {
      Debug("%s: RPC failed.\n", __FUNCTION__);
      RPCIN_SETRETVALS(data, "RPC failed", FALSE);
   } else {
      /*
       * We don't have any out parameters, so we write empty values into the
       * result fields of the RpcInData structure.
       */
      RPCIN_SETRETVALS(data, "", TRUE);

      /* Set our return value and return to the caller. */
      ret = TRUE;
   }

exit:
   VMX_XDR_FREE(xdr_GHITrayIconEvent, &eventMsg);
   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHITclOTrayIconStartUpdates --
 *
 *      Start sending tray icon updates to the VMX.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      XXX.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
GHITcloTrayIconStartUpdates(RpcInData *data) // IN/OUT: XXX
{
   Bool ret = FALSE;

   Debug("%s: Enter.\n", __FUNCTION__);

   /* Check our arguments. */
   ASSERT(data);
   ASSERT(data->name);

   if (!(data && data->name)) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      goto exit;
   }

   Debug("%s: Got RPC, name: \"%s\", argument length: %"FMTSZ"u.\n",
         __FUNCTION__, data->name, data->argsSize);

   if (!GHI_TrayIconStartUpdates()) {
      Debug("%s: Failed to start tray icon updates.\n", __FUNCTION__);
      RPCIN_SETRETVALS(data, "Failed to start tray icon updates", FALSE);
      goto exit;
   }

   /*
    * Write the result into the RPC out parameters.
    */
   ret = RPCIN_SETRETVALS(data, "", TRUE);

exit:
   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHITcloTrayIconStopUpdates --
 *
 *      Stop sending tray icon updates to the VMX.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      XXX.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
GHITcloTrayIconStopUpdates(RpcInData *data) // IN/OUT:
{
   Bool ret = FALSE;

   Debug("%s: Enter.\n", __FUNCTION__);

   /* Check our arguments. */
   ASSERT(data);
   ASSERT(data->name);

   if (!(data && data->name)) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      goto exit;
   }

   Debug("%s: Got RPC, name: \"%s\", argument length: %"FMTSZ"u.\n",
         __FUNCTION__, data->name, data->argsSize);

   if (!GHI_TrayIconStopUpdates()) {
      Debug("%s: Failed to start tray icon updates.\n", __FUNCTION__);
      RPCIN_SETRETVALS(data, "Failed to start tray icon updates", FALSE);
      goto exit;
   }

   /*
    * Write the result into the RPC out parameters.
    */
   ret = RPCIN_SETRETVALS(data, "", TRUE);

exit:
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHISendTrayIconUpdateRPC --
 *
 *      Send the ghi.guest.trayIcon.update RPC to the host.
 *
 * Results:
 *      TRUE if sent, FALSE otherwise.
 *
 * Side effects:
 *      XXX.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GHISendTrayIconUpdateRpc(XDR *xdrs) // XXX
{
   Bool ret = FALSE;
   DynBuf outBuf;

   Debug("%s: Enter.\n", __FUNCTION__);

   /* Check our arguments. */
   ASSERT(xdrs);
   if (NULL == xdrs) {
      Debug("%s: Invalid parameter.\n", __FUNCTION__);
      goto exit;
   }

   /* Append our RPC name and a space to the DynBuf. */
   DynBuf_Init(&outBuf);
   if (!DynBuf_Append(&outBuf,
                      GHI_RPC_TRAY_ICON_UPDATE,
                      strlen(GHI_RPC_TRAY_ICON_UPDATE))) {
      Debug("%s: Failed to append RPC name to DynBuf.\n", __FUNCTION__);
      goto exit;
   }

   if (!DynBuf_Append(&outBuf, " ", 1)) {
      Debug("%s: Failed to append space to DynBuf.\n", __FUNCTION__);
      goto exit;
   }

   /* Append the XDR serialized data to the DynBuf. */
   if (!DynBuf_Append(&outBuf, DynXdr_Get(xdrs), xdr_getpos(xdrs)) )
   {
      Debug("%s: Failed to append XDR serialized data to DynBuf.\n",
            __FUNCTION__);
      goto exit;
   }

   if (!RpcOut_SendOneRaw(DynBuf_Get(&outBuf),
                          DynBuf_GetSize(&outBuf),
                          NULL,
                          NULL)) {
      Debug("%s: Failed to send RPC to host!\n", __FUNCTION__);
      goto exit;
   }

   ret = TRUE;

exit:
   DynBuf_Destroy(&outBuf);
   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHITcloSetFocusedWindow --
 *
 *      Set the specified window to be focused (NULL or zero window ID indicates
 *      that no window should be focused).
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      XXX.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
GHITcloSetFocusedWindow(RpcInData *data)
{
   Bool ret = FALSE;
   XDR xdrs;

   Debug("%s: Enter.\n", __FUNCTION__);

   /* Check our arguments. */
   ASSERT(data);
   ASSERT(data->name);

   if (!(data && data->name)) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      goto exit;
   }

   Debug("%s: Got RPC, name: \"%s\", argument length: %"FMTSZ"u.\n",
         __FUNCTION__, data->name, data->argsSize);

   /*
    * Build an XDR stream from the argument data.
    *
    * Note that the argument data begins with args + 1 since there is a space
    * between the RPC name and the XDR serialization.
    */
   xdrmem_create(&xdrs, (char*) data->args + 1, data->argsSize - 1, XDR_DECODE);

   GHISetFocusedWindow setFocusedWindowMsg;
   GHISetFocusedWindowV1 *setFocusedWindowV1Ptr;

   memset(&setFocusedWindowMsg, 0, sizeof setFocusedWindowMsg);
   if (!xdr_GHISetFocusedWindow((XDR *)&xdrs, &setFocusedWindowMsg)) {
      goto exitWithXDRCleanup;
   }

   ASSERT(setFocusedWindowMsg.ver == GHI_SET_FOCUSED_WINDOW_V1);
   if (setFocusedWindowMsg.ver != GHI_SET_FOCUSED_WINDOW_V1) {
      Debug("%s: Unexpected XDR version = %d\n", __FUNCTION__, setFocusedWindowMsg.ver);
      ret = RPCIN_SETRETVALS(data,
                             "Unexpected XDR version.",
                             FALSE);
      goto exitWithXDRCleanup;
   }

   setFocusedWindowV1Ptr = setFocusedWindowMsg.GHISetFocusedWindow_u.setFocusedWindowV1;

   /* Call the platform implementation of our RPC. */
   ret = GHI_SetFocusedWindow(setFocusedWindowV1Ptr->windowId);

   /*
    * Write the result into the RPC out parameters.
    */
   ret = RPCIN_SETRETVALS(data, "", TRUE);

exitWithXDRCleanup:
   /* Destroy the XDR stream. */
   xdr_destroy(&xdrs);
   VMX_XDR_FREE(xdr_GHISetFocusedWindow, &setFocusedWindowMsg);
exit:
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHITcloGetExecInfoHash --
 *
 *      Get the hash (or timestamp) of information returned by
 *      GHITcloGetBinaryInfo.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
GHITcloGetExecInfoHash(RpcInData *data) // IN/OUT:
{
   Bool ret = TRUE;
   GHIGetExecInfoHashRequest requestMsg;
   GHIGetExecInfoHashReply replyMsg;
   XDR xdrs;
   GHIGetExecInfoHashRequestV1 *requestV1;
   GHIGetExecInfoHashReplyV1 replyV1;
   char *execHash = NULL;

   memset(&requestMsg, 0, sizeof requestMsg);
   memset(&replyMsg, 0, sizeof replyMsg);

   /* Check our arguments. */
   ASSERT(data);
   ASSERT(data->name);
   ASSERT(data->args);

   if (!(data && data->name && data->args)) {
      Debug("%s: Invalid arguments.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Invalid arguments.", FALSE);
      goto error;
   }

   Debug("%s: Got RPC, name: \"%s\", argument length: %"FMTSZ"u.\n",
         __FUNCTION__, data->name, data->argsSize);

   /*
    * Deserialize the XDR data. Note that the data begins with args + 1 since
    * there is a space between the RPC name and the XDR serialization.
    */
   if (!XdrUtil_Deserialize((char *)data->args + 1, data->argsSize - 1,
                            (void *)xdr_GHIGetExecInfoHashRequest, &requestMsg)) {
      Debug("%s: Failed to deserialize data\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Failed to deserialize data.", FALSE);
      goto error;
   }

   ASSERT(requestMsg.ver == GHI_GET_EXEC_INFO_HASH_V1);
   if (requestMsg.ver != GHI_GET_EXEC_INFO_HASH_V1) {
      Debug("%s: Unexpected XDR version = %d\n", __FUNCTION__, requestMsg.ver);
      ret = RPCIN_SETRETVALS(data, "Unexpected XDR version.", FALSE);
      goto exit;
   }

   requestV1 = requestMsg.GHIGetExecInfoHashRequest_u.requestV1;

   /*
    * Call the platform implementation of the RPC handler.
    */
   if (!GHI_GetExecInfoHash(requestV1->execPath, &execHash)) {
      ret = RPCIN_SETRETVALS(data, "Could not get executable info hash.", FALSE);
      goto exit;
   }

   replyV1.execHash = execHash;
   replyMsg.ver = GHI_GET_EXEC_INFO_HASH_V1;
   replyMsg.GHIGetExecInfoHashReply_u.replyV1 = &replyV1;

   /*
    * Serialize the result data and return.
    */
   ASSERT_MEM_ALLOC(DynXdr_Create(&xdrs));

   if (!xdr_GHIGetExecInfoHashReply(&xdrs, &replyMsg)) {
      ret = RPCIN_SETRETVALS(data, "Failed to serialize data", FALSE);
      DynXdr_Destroy(&xdrs, TRUE);
      goto exit;
   }

   data->result = reinterpret_cast<char *>(DynXdr_Get(&xdrs));
   data->resultLen = xdr_getpos(&xdrs);
   data->freeResult = TRUE;

   /*
    * Destroy the serialized XDR structure but leave the data buffer alone
    * since it will be freed by the RpcIn layer.
    */
   DynXdr_Destroy(&xdrs, FALSE);

exit:
   VMX_XDR_FREE(xdr_GHIGetExecInfoHashRequest, &requestMsg);
error:
   /* Free the memory allocated in the platform layer for the hash */
   free(execHash);
   return ret;
}
