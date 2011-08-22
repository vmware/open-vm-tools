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

/**
 * @file unityTclo.cpp
 *
 *    Unity: Guest window manager integration service.
 *
 * This file implements parts of the guest-side Unity agent as part of the VMware Tools.
 * It contains entry points for the GuestRpc (TCLO, RPCI) interface.
 *
 * UnityWindowTracker updates are sent to the MKS in two ways:
 *    @li @ref UNITY_RPC_GET_UPDATE GuestRpc (host-to-guest).
 *    @li @ref UNITY_RPC_PUSH_UPDATE_CMD GuestRpc (guest-to-host).
 *
 * @note Looking for the old "unity.get.update" return syntax?  See @ref
 * UNITY_RPC_GET_UPDATE and @ref UnityGetUpdateReturn instead.
 *
 * @sa
 *    @li UnityRpcHG
 *    @li UnityRpcGH
 */

#include "vmware/tools/plugin.h"

extern "C" {
   #include "vmware.h"
   #include "conf.h"
   #include "debug.h"
   #include "dynxdr.h"
   #include "guestrpc/unity.h"
   #include "guestrpc/unityActive.h"
   #include "rpcin.h"
   #include "rpcout.h"
   #include "str.h"
   #include "strutil.h"
#if defined(OPEN_VM_TOOLS)
   #include "unitylib/unity.h"
#else
   #include "unity.h"
#endif // OPEN_VM_TOOLS
   #include "unityDebug.h"
   #include "xdrutil.h"
};

#include "unityPlugin.h"
#include "unityTclo.h"

namespace vmware { namespace tools {

/**
 * Container used to store and send Unity updates.
 */

typedef struct _UnityUpdateChannel {
   DynBuf updates;      ///< See @ref vmtools_unity_uwtGuestRpc.
   size_t cmdSize;      /**< @brief Size of RpcOut command prefix.
                             Used as a convenient offset within #updates when
                             resetting the update buffer. */
   RpcOut *rpcOut;
} UnityTCLOUpdateChannel;

/*
 * Helper Functions
 */

static Bool UnityUpdateState(void);

/* Sends the unity.window.contents.start RPC to the host. */
Bool UnitySendWindowContentsStart(UnityWindowId window,
                                  uint32 width,
                                  uint32 height,
                                  uint32 length);

/* Sends the unity.window.contents.chunk RPC to the host. */
Bool UnitySendWindowContentsChunk(UnityWindowId window,
                                  uint32 seq,
                                  const char *data,
                                  uint32 length);

/* Sends the unity.window.contents.end RPC to the host. */
Bool UnitySendWindowContentsEnd(UnityWindowId window);

/*
 * Callback function used by UnityXdrSendRpc() to encode XDR-serialized
 * arguments.
 */
typedef Bool(*UnityXdrEncodeFunc)(XDR*,void*);

/*
 * Helper function used to send an RPC to the host with XDR-serialized
 * arguments. Calls encodeFn on the XDR* and the provied arg to perform
 * XDR encoding.
 */
Bool UnityXdrSendRpc(const char *rpcName, UnityXdrEncodeFunc encodeFn, void *arg);

/*
 * XXX:
 * According to Adar:
 *    "UnityTcloGetUpdate cannot return the contents of a DynBuf. This will leak
 *     the DynBuf's memory, since nobody at a lower level will ever free it.  It's
 *     a crappy interface, but we make due by using a static buffer to hold the
 *     results."
 *
 * We ideally would not use a static buffer because the maximum size of the
 * update is unknown.  To work around this, make the DynBuf returned in
 * UnityTcloGetUpdate file-global and recycle it across update requests.
 */

static DynBuf gTcloUpdate;


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloInit --
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
UnityTcloInit()
{
   /*
    * Init our global dynbuf used to send results back.
    */
   DynBuf_Init(&gTcloUpdate);

}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloCleanup --
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
UnityTcloCleanup()
{
   DynBuf_Destroy(&gTcloUpdate);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloEnter --
 *
 *     RPC handler for 'unity.enter'. Save and disable certain user
 *     settings. Start Unity updates thread and any other platform
 *     specific threads (like a thread that listens for
 *     the desktop switch event on Windows). Note that we first set
 *     the UI settings, and then start the threads. This way the UI
 *     settings take effect before we start sending Unity updates,
 *     so that we never send things like task bar (see bug 166085).
 *
 * Results:
 *     TRUE if helper threads were started.
 *     FALSE otherwise.
 *
 * Side effects:
 *     Certain UI system settings will be disabled.
 *     Unity update thread will be started.
 *     Any other platform specific helper threads will be started as well.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloEnter(RpcInData *data)         //  IN/OUT
{
   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   Debug("%s\n", __FUNCTION__);

   if (!Unity_Enter()) {
      return RPCIN_SETRETVALS(data, "Could not enter unity", FALSE);
   }

   UnityUpdateState();

   return RPCIN_SETRETVALS(data, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloExit --
 *
 *     RPC handler for 'unity.exit'.
 *
 * Results:
 *     Always TRUE.
 *
 * Side effects:
 *     Same as side effects of Unity_Exit().
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloExit(RpcInData *data)   // IN/OUT
{
   /* Check our arguments. */
   ASSERT(data);
   if (!data) {
      return FALSE;
   }

   Debug("UnityTcloExit.\n");

   Unity_Exit();

   UnityUpdateState();
   return RPCIN_SETRETVALS(data, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloGetWindowPath --
 *
 *      RPC handler for UNITY_RPC_GET_WINDOW_PATH.
 *
 *      Get the information needed to re-launch a window and retrieve further
 *      information on it.  Returns double-NUL-terminated buffer consisting of
 *      NUL-terminated strings "windowPath" and "execPath" strings, the first
 *      uniquely identifying the window and the second uniquely identifying the
 *      window's owning executable.
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
UnityTcloGetWindowPath(RpcInData *data)   // IN/OUT
{
   UnityWindowId window;
   DynBuf windowPathUtf8;
   DynBuf execPathUtf8;

   unsigned int index = 0;
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

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /* Parse the command & window id.*/

   if (!StrUtil_GetNextIntToken((int32*) &window, &index, data->args, " ")) {
      Debug("UnityTcloGetWindowInfo: Invalid RPC arguments.\n");
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected \"windowId\"",
                              FALSE);
   }

   Debug("%s: window %d\n", __FUNCTION__, window);

   /*
    * Please note that the Unity_GetWindowPath implementations assume that the
    * dynbuf passed in does not contain any existing data that needs to be appended to,
    * so this code should continue to accomodate that assumption.
    */
   DynBuf_Destroy(&gTcloUpdate);
   DynBuf_Init(&gTcloUpdate);
   DynBuf_Init(&windowPathUtf8);
   DynBuf_Init(&execPathUtf8);
   if (!Unity_GetWindowPath(window, &windowPathUtf8, &execPathUtf8)) {
      Debug("%s: Could not get window path.\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data,
                             "Could not get window path",
                             FALSE);
      goto exit;
   }

   /*
    * Construct the buffer holding the result. Note that we need to use gTcloUpdate
    * here to avoid leaking during the RPC handler.
    */
   DynBuf_Copy(&windowPathUtf8, &gTcloUpdate);
   DynBuf_Append(&gTcloUpdate, DynBuf_Get(&execPathUtf8), DynBuf_GetSize(&execPathUtf8));

   /*
    * Write the final result into the result out parameters and return!
    */
   data->result = (char *)DynBuf_Get(&gTcloUpdate);
   data->resultLen = DynBuf_GetSize(&gTcloUpdate);

exit:
   DynBuf_Destroy(&windowPathUtf8);
   DynBuf_Destroy(&execPathUtf8);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloWindowCommand --
 *
 *     RPC handler for 'unity.window.*' (excluding 'unity.window.settop')
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
UnityTcloWindowCommand(RpcInData *data)   // IN/OUT
{
   UnityWindowId window;
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

   Debug("UnityTcloWindowCommand: name:%s args:'%s'\n", data->name, data->args);

   /* Parse the command & window id.*/

   if (!StrUtil_GetNextIntToken((int32*) &window, &index, data->args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected \"windowId\"",
                              FALSE);

   }

   Debug("%s: %s window %d\n", __FUNCTION__, data->name, window);

   if (!Unity_WindowCommand(window, data->name)) {
      return RPCIN_SETRETVALS(data,
                             "Could not execute window command",
                             FALSE);
   } else {
      return RPCIN_SETRETVALS(data, "", TRUE);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetDesktopWorkArea --
 *
 *     RPC handler for 'unity.desktop.work_area.set'.
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
UnityTcloSetDesktopWorkArea(RpcInData *data)    // IN/OUT
{
   Bool success = FALSE;
   const char *argList;
   unsigned int count;
   unsigned int i;
   UnityRect *workAreas = NULL;

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
    * The argument string will look something like:
    *   <count> [ , <x> <y> <w> <h> ] * count.
    *
    * e.g.
    *    3 , 0 0 640 480 , 640 0 800 600 , 0 480 640 480
    */

   if (sscanf(data->args, "%u", &count) != 1) {
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected \"count\"",
                              FALSE);
   }

   if (count != 0) {
      workAreas = (UnityRect *)malloc(sizeof *workAreas * count);
      if (!workAreas) {
         RPCIN_SETRETVALS(data,
                          "Failed to alloc buffer for work areas",
                          FALSE);
         goto out;
      }
   }

   for (argList = data->args, i = 0; i < count; i++) {
      argList = strchr(argList, ',');
      if (!argList) {
         RPCIN_SETRETVALS(data,
                          "Expected comma separated display list",
                          FALSE);
         goto out;
      }
      argList++; /* Skip past the , */

      if (sscanf(argList, " %d %d %d %d ",
                 &workAreas[i].x, &workAreas[i].y,
                 &workAreas[i].width, &workAreas[i].height) != 4) {
         RPCIN_SETRETVALS(data,
                          "Expected x, y, w, h in display entry",
                          FALSE);
         goto out;
      }

      if (workAreas[i].x < 0 || workAreas[i].y < 0 ||
          workAreas[i].width <= 0 || workAreas[i].height <= 0) {
         RPCIN_SETRETVALS(data, "Invalid argument", FALSE);
         goto out;
      }
   }

   if (!Unity_SetDesktopWorkAreas(workAreas, count)) {
      RPCIN_SETRETVALS(data,
                       "Unity_SetDesktopWorkAreas failed",
                       FALSE);
      goto out;
   }

   success = RPCIN_SETRETVALS(data, "", TRUE);

out:
   free(workAreas);
   return success;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetTopWindowGroup --
 *
 *     RPC handler for 'unity.window.settop'.
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
UnityTcloSetTopWindowGroup(RpcInData *data)  // IN/OUT
{
   UnityWindowId window;
   unsigned int index = 0;
   unsigned int windowCount = 0;
   UnityWindowId windows[UNITY_MAX_SETTOP_WINDOW_COUNT];

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

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /* Parse the command & window ids.*/

   while (StrUtil_GetNextUintToken(&window, &index, data->args, " ")) {
      windows[windowCount] = window;
      windowCount++;
      if (windowCount == UNITY_MAX_SETTOP_WINDOW_COUNT) {
         Debug("%s: Too many windows.\n", __FUNCTION__);
         return RPCIN_SETRETVALS(data,
                                 "Invalid arguments. Too many windows",
                                 FALSE);
      }
   }

   if (windowCount == 0) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected at least one windowId",
                              FALSE);
   }

   if (!Unity_SetTopWindowGroup(windows, windowCount)) {
      return RPCIN_SETRETVALS(data,
                              "Could not execute window command",
                              FALSE);
   }

   return RPCIN_SETRETVALS(data, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloGetUpdate --
 *
 *     RPC handler for 'unity.get.update'.  Ask the unity window tracker
 *     to give us an update (either incremental or non-incremental based
 *     on whether the 'incremental' arg is present) and send the result
 *     back to the VMX.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Clearly.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloGetUpdate(RpcInData *data)    // IN/OUT
{
   Bool incremental = FALSE;

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

   Debug("%s: name:%s args:'%s'", __FUNCTION__, data->name, data->args);

   /*
    * Specify incremental or non-incremetal updates based on whether or
    * not the client set the "incremental" arg.
    */
   if (strstr(data->name, "incremental")) {
      incremental = TRUE;
   }

   /*
    * Call into platform-specific implementation to gather and send updates
    * back via RPCI.  (This is done to ensure all updates are sent to the
    * Unity server in sequence via the same channel.)
    */
   Unity_GetUpdate(incremental);

   /*
    * To maintain compatibility, we'll return a successful but empty response.
    */
   data->result = "";
   data->resultLen = 0;

   /*
    * Give the debugger a crack to do something interesting at this point
    *
    * XXX Not sure if this is worth keeping around since this routine no
    * longer returns updates directly.
    */
   UnityDebug_OnUpdate();

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloConfirmOperation --
 *
 *     RPC handler for 'unity.operation.confirm'.
 *
 * Results:
 *     TRUE if the confirmation could be handled sucessfully.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloConfirmOperation(RpcInData *data)   // IN/OUT
{
   UnityConfirmOperation unityConfirmOpMsg;
   UnityConfirmOperationV1 *confirmV1 = NULL;
   Bool retVal = FALSE;
   unsigned int ret;

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

   Debug("%s: Enter.\n", __FUNCTION__);

   memset(&unityConfirmOpMsg, 0, sizeof unityConfirmOpMsg);

   /*
    * Deserialize the XDR data. Note that the data begins with args + 1 since
    * there is a space between the RPC name and the XDR serialization.
    */
   if (!XdrUtil_Deserialize(data->args + 1, data->argsSize - 1,
                            (void *)xdr_UnityConfirmOperation, &unityConfirmOpMsg)) {
      ret = RPCIN_SETRETVALS(data, "Failed to deserialize data", FALSE);
      goto exit;
   }

   confirmV1 = unityConfirmOpMsg.UnityConfirmOperation_u.unityConfirmOpV1;
   retVal = Unity_ConfirmOperation(confirmV1->details.op,
                                   confirmV1->windowId,
                                   confirmV1->sequence,
                                   confirmV1->allow);

   /* Free any memory allocated by XDR - we're done with unityConfirmOpMsg */
   VMX_XDR_FREE(xdr_UnityConfirmOperation, &unityConfirmOpMsg);
   ret = RPCIN_SETRETVALS(data, "", retVal);

exit:
   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSendMouseWheel --
 *
 *     RPC handler for 'unity.sendMouseWheel'.
 *
 * Results:
 *     TRUE on success, FALSE on failure.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloSendMouseWheel(RpcInData *data)   // IN/OUT
{
   UnityMouseWheel unityMouseWheelMsg;
   UnityMouseWheelV1 *mouseWheelV1 = NULL;
   Bool retVal = FALSE;
   unsigned int ret;
   Debug("%s: Enter.\n", __FUNCTION__);

   memset(&unityMouseWheelMsg, 0, sizeof unityMouseWheelMsg);

   /*
    * Deserialize the XDR data. Note that the data begins with args + 1 since
    * there is a space between the RPC name and the XDR serialization.
    */
   if (!XdrUtil_Deserialize(data->args + 1, data->argsSize - 1,
                            (void *)xdr_UnityMouseWheel, &unityMouseWheelMsg)) {
      ret = RPCIN_SETRETVALS(data, "Failed to deserialize data", FALSE);
      goto exit;
   }

   mouseWheelV1 = unityMouseWheelMsg.UnityMouseWheel_u.mouseWheelV1;
   retVal = Unity_SendMouseWheel(mouseWheelV1->deltaX,
                                 mouseWheelV1->deltaY,
                                 mouseWheelV1->deltaZ,
                                 mouseWheelV1->modifierFlags);

   /* Free any memory allocated by XDR - we're done with unityMouseWheelMsg */
   VMX_XDR_FREE(xdr_UnityMouseWheel, &unityMouseWheelMsg);
   ret = RPCIN_SETRETVALS(data, "", retVal);

exit:
   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityUpdateCallbackFn --
 *
 *     Callback from the unity window tracker indicating something's
 *     changed.
 *
 *     Write the update string into our dynbuf accumlating the update
 *     and return.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Clearly.
 *
 *----------------------------------------------------------------------------
 */

void
UnityUpdateCallbackFn(void *param,          // IN: updateChannel
                      UnityUpdate *update)  // IN
{
   UnityUpdateChannel *updateChannel = reinterpret_cast<UnityUpdateChannel *>(param);
   DynBuf *buf = NULL;
   char data[1024];
   int i, n, count = 0;
   RegionPtr region;
   char *titleUtf8 = NULL;
   char *windowPathUtf8 = "";
   char *execPathUtf8 = "";

   ASSERT(updateChannel);
   buf = &updateChannel->updates;

   switch (update->type) {

   case UNITY_UPDATE_ADD_WINDOW:
      if (DynBuf_GetSize(&update->u.addWindow.windowPathUtf8) > 0) {
         windowPathUtf8 =
            static_cast<char*>(DynBuf_Get(&update->u.addWindow.windowPathUtf8));
      }
      if (DynBuf_GetSize(&update->u.addWindow.execPathUtf8) > 0) {
         execPathUtf8 =
            static_cast<char*>(DynBuf_Get(&update->u.addWindow.execPathUtf8));
      }

      Str_Sprintf(data, sizeof data, "add %u windowPath=%s execPath=%s",
                  update->u.addWindow.id,
                  windowPathUtf8,
                  execPathUtf8);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_MOVE_WINDOW:
      Str_Sprintf(data, sizeof data, "move %u %d %d %d %d",
                  update->u.moveWindow.id,
                  update->u.moveWindow.rect.x1,
                  update->u.moveWindow.rect.y1,
                  update->u.moveWindow.rect.x2,
                  update->u.moveWindow.rect.y2);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_REMOVE_WINDOW:
      Str_Sprintf(data, sizeof data, "remove %u", update->u.removeWindow.id);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_REGION:
      /*
       * A null region indicates that the region should be deleted.
       * Make sure we write "region <id> 0" for the reply.
       */
      region = update->u.changeWindowRegion.region;
      if (region) {
         count = REGION_NUM_RECTS(region);
      }
      Str_Sprintf(data, sizeof data, "region %u %d",
                  update->u.changeWindowRegion.id, count);
      DynBuf_AppendString(buf, data);

      for (i = 0; i < count; i++) {
         BoxPtr p = REGION_RECTS(region) + i;
         Str_Sprintf(data, sizeof data, "rect %d %d %d %d",
                     p->x1, p->y1, p->x2, p->y2);
         DynBuf_AppendString(buf, data);
      }
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_TITLE:
      titleUtf8 =
         reinterpret_cast<char*>(DynBuf_Get(&update->u.changeWindowTitle.titleUtf8));

      if (titleUtf8 &&
          (DynBuf_GetSize(&update->u.changeWindowTitle.titleUtf8) ==
           strlen(titleUtf8) + 1)) {
           Str_Sprintf(data, sizeof data, "title %u ",
                       update->u.changeWindowTitle.id);
           Str_Strncat(data, sizeof data, titleUtf8, sizeof data - strlen(data) - 1);
           data[sizeof data - 1] = '\0';
      } else {
         Str_Sprintf(data, sizeof data, "title %u",
                     update->u.changeWindowTitle.id);
      }
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_ZORDER:
      n = Str_Snprintf(data, sizeof data, "zorder %d", update->u.zorder.count);
      DynBuf_Append(buf, data, n);
      for (unsigned int zOrderIndex = 0;
           zOrderIndex < update->u.zorder.count;
           zOrderIndex++) {
         n = Str_Snprintf(data, sizeof data, " %d", update->u.zorder.ids[zOrderIndex]);
         DynBuf_Append(buf, data, n);
      }
      DynBuf_AppendString(buf, ""); // for appending NULL
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_STATE:
      Str_Sprintf(data, sizeof data, "state %u %u",
                  update->u.changeWindowState.id,
                  update->u.changeWindowState.state);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_ATTRIBUTE:
      Str_Sprintf(data, sizeof data, "attr %u %u %u",
                  update->u.changeWindowAttribute.id,
                  update->u.changeWindowAttribute.attr,
                  update->u.changeWindowAttribute.value);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_TYPE:
      Str_Sprintf(data, sizeof data, "type %u %d",
                  update->u.changeWindowType.id,
                  update->u.changeWindowType.winType);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_ICON:
      Str_Sprintf(data, sizeof data, "icon %u %u",
                  update->u.changeWindowIcon.id,
                  update->u.changeWindowIcon.iconType);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_DESKTOP:
      Str_Sprintf(data, sizeof data, "desktop %u %d",
                  update->u.changeWindowDesktop.id,
                  update->u.changeWindowDesktop.desktopId);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_ACTIVE_DESKTOP:
      Str_Sprintf(data, sizeof data, "activedesktop %d",
                  update->u.changeActiveDesktop.desktopId);
      DynBuf_AppendString(buf, data);
      break;

   default:
      NOT_IMPLEMENTED();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityUpdateChannelInit --
 *
 *      Initialize the state for the update channel.
 *
 * Return value:
 *      Pointer to UpdateChannel structure or NULL
 *
 * Side effects:
 *      RpcOut channel might be open.
 *      Memory for the update buffer might be allocated.
 *
 *-----------------------------------------------------------------------------
 */

UnityUpdateChannel *
UnityUpdateChannelInit()
{
   UnityUpdateChannel *updateChannel = NULL;
   updateChannel = reinterpret_cast<UnityUpdateChannel *>(Util_SafeCalloc(1, sizeof *updateChannel));

   DynBuf_Init(&updateChannel->updates);
   DynBuf_AppendString(&updateChannel->updates, UNITY_RPC_PUSH_UPDATE_CMD " ");

   /* Exclude the null. */
   updateChannel->cmdSize = DynBuf_GetSize(&updateChannel->updates) - 1;
   DynBuf_SetSize(&updateChannel->updates, updateChannel->cmdSize);

   updateChannel->rpcOut = RpcOut_Construct();
   if (updateChannel->rpcOut == NULL) {
      Warning("%s: RpcOut_Construct() failed\n", __FUNCTION__);
      goto error;
   }

   if (!RpcOut_start(updateChannel->rpcOut)) {
      Warning("%s: RpcOut_start() failed\n", __FUNCTION__);
      RpcOut_Destruct(updateChannel->rpcOut);
      goto error;
   }

   return updateChannel;

error:
   DynBuf_Destroy(&updateChannel->updates);
   vm_free(updateChannel);

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityUpdateChannelCleanup --
 *
 *      Cleanup the unity update channel.
 *
 * Return value:
 *      None.
 *
 * Side effects:
 *      RpcOut channel will be closed.
 *      Memory will be freed.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityUpdateChannelCleanup(UnityUpdateChannel *updateChannel) // IN
{
   if (updateChannel && updateChannel->rpcOut) {
      RpcOut_stop(updateChannel->rpcOut);
      RpcOut_Destruct(updateChannel->rpcOut);
      updateChannel->rpcOut = NULL;

      DynBuf_Destroy(&updateChannel->updates); // Avoid double-free by guarding this as well
      vm_free(updateChannel);
   }
}


#ifdef VMX86_DEVEL
/*
 *-----------------------------------------------------------------------------
 *
 * DumpUpdate --
 *
 *      Prints a Unity update via debug output.  NUL is represented as '!'.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
DumpUpdate(UnityUpdateChannel *updateChannel)   // IN
{
   int i, len;
   char *buf = NULL;

   len = updateChannel->updates.size;
   buf = reinterpret_cast<char*>(Util_SafeMalloc(len + 1));
   memcpy(buf, updateChannel->updates.data, len);
   buf[len] = '\0';
   for (i = 0 ; i < len; i++) {
      if (buf[i] == '\0') {
         buf[i] = '!';
      }
   }

   Debug("%s: Sending update: %s\n", __FUNCTION__, buf);

   vm_free(buf);
}
#endif // ifdef VMX86_DEVEL


/*
 *-----------------------------------------------------------------------------
 *
 * UnitySendUpdates --
 *
 *      Send a round of unity updates. The caller is responsible
 *      for gathering updates into updateChannel->updates buffer prior to the
 *      function call. This function should only be called if there's data
 *      in the update buffer to avoid sending empty update string to the VMX.
 *
 * Return value:
 *      TRUE if the update was sent,
 *      FALSE if something went wrong (an invalid RPC channel, for example).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnitySendUpdates(void *param) // IN
{
   char const *myReply;
   size_t myRepLen;
   Bool retry = FALSE;
   UnityUpdateChannel *updateChannel = reinterpret_cast<UnityUpdateChannel*>(param);

   ASSERT(updateChannel);
   ASSERT(updateChannel->rpcOut);

   /* Send 'tools.unity.push.update <updates>' to the VMX. */

#ifdef VMX86_DEVEL
   DumpUpdate(updateChannel);
#endif

retry_send:
   if (!RpcOut_send(updateChannel->rpcOut,
                    (char *)DynBuf_Get(&updateChannel->updates),
                    DynBuf_GetSize(&updateChannel->updates),
                    &myReply, &myRepLen)) {

      /*
       * We could not send the RPC. If we haven't tried to reopen
       * the channel, try to reopen and resend. If we already
       * tried to resend, then it's time to give up. I hope that
       * trying to resend once is enough.
       */

      if (!retry) {
         retry = TRUE;
         Debug("%s: could not send rpc. Reopening channel.\n", __FUNCTION__);
         RpcOut_stop(updateChannel->rpcOut);
         if (!RpcOut_start(updateChannel->rpcOut)) {
            Debug("%s: could not reopen rpc channel. Exiting...\n", __FUNCTION__);
            return FALSE;
         }
         goto retry_send;

      } else {
         Debug("%s: could not resend rpc. Giving up and exiting...\n", __FUNCTION__);
         return FALSE;
      }
   }

   /*
    * With the update queue sent, purge the DynBuf by trimming it to the length
    * of the command preamble.
    */
   DynBuf_SetSize(&updateChannel->updates, updateChannel->cmdSize);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloGetWindowContents --
 *
 *     RPC handler for 'unity.get.window.contents'. Suck the bits off the
 *     window and return a .png image over the backdoor.
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
UnityTcloGetWindowContents(RpcInData *data)     // IN/OUT
{
   unsigned int window;
   unsigned int index = 0;
   DynBuf *imageData = &gTcloUpdate;
   uint32 width;
   uint32 height;

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

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /*
    * Parse the command & window id.
    */
   if (!StrUtil_GetNextIntToken((int32*) &window, &index, data->args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "failed: arguments. Expected \"windowId\"",
                              FALSE);

   }
   Debug("%s: window %d\n", __FUNCTION__, window);

   /*
    * Read the contents of the window, compress it as a .png and
    * send the .png back to the vmx as the RPC result.
    */
   DynBuf_SetSize(imageData, 0);
   if (!Unity_GetWindowContents(window, imageData, &width, &height)) {
      return RPCIN_SETRETVALS(data,
                              "failed: Could not read window contents",
                              FALSE);
   }

   data->result = (char *)DynBuf_Get(imageData);
   data->resultLen = DynBuf_GetSize(imageData);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloGetIconData --
 *
 *     RPC handler for 'unity.get.icon.data'. Suck the bits off the
 *     window and return a .png image over the backdoor.
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
UnityTcloGetIconData(RpcInData *data)  // IN/OUT
{
   UnityWindowId window;
   UnityIconType iconType;
   UnityIconSize iconSize;
   unsigned int dataOffset, dataLength;
   uint32 fullLength;
   size_t retLength;
   DynBuf *results = &gTcloUpdate, imageData;
   char bitmapData[1024];

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

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   /*
    * Parse the arguments.
    */
   if ((sscanf(data->args, "%u %u %u %u %u",
               &window,
               (unsigned int*) &iconType,
               &iconSize,
               &dataOffset,
               &dataLength) != 5)
       || (dataLength > UNITY_MAX_ICON_DATA_CHUNK)) {
      Debug("UnityTcloGetIconData: Invalid RPC arguments.\n");
      return RPCIN_SETRETVALS(data,
                              "failed: arguments missing",
                              FALSE);
   }

   Debug("%s: window %u iconType %u" \
         " iconSize %u dataOffset %u dataLength %u\n",
         __FUNCTION__,
         window, iconType, iconSize, dataOffset, dataLength);

   /*
    * Retrieve part/all of the icon in PNG format.
    */
   DynBuf_Init(&imageData);
   if (!Unity_GetIconData(window, iconType, iconSize,
                          dataOffset, dataLength, &imageData, &fullLength)) {
      return RPCIN_SETRETVALS(data,
                              "failed: Could not read icon data properly",
                              FALSE);
   }


   DynBuf_SetSize(results, 0);
   retLength = DynBuf_GetSize(&imageData);
   retLength = MIN(retLength, UNITY_MAX_ICON_DATA_CHUNK);
   DynBuf_Append(results, bitmapData, Str_Snprintf(bitmapData,
                                                   sizeof bitmapData,
                                                   "%u %" FMTSZ "u ",
                                                   fullLength, retLength));
   DynBuf_Append(results, DynBuf_Get(&imageData), retLength);

   /*
    * Guarantee that the results have a trailing \0 in case anything does a strlen...
    */
   DynBuf_AppendString(results, "");
   data->result = (char *)DynBuf_Get(results);
   data->resultLen = DynBuf_GetSize(results);
   DynBuf_Destroy(&imageData);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloShowTaskbar --
 *
 *     RPC handler for 'unity.show.taskbar'.
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
UnityTcloShowTaskbar(RpcInData *data)     // IN/OUT
{
   uint32 command = 0;
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

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   if (!StrUtil_GetNextUintToken(&command, &index, data->args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments.",
                              FALSE);
   }

   Debug("%s: command %d\n", __FUNCTION__, command);

   Unity_ShowTaskbar((command == 0) ? FALSE : TRUE);

   return RPCIN_SETRETVALS(data, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloMoveResizeWindow --
 *
 *     RPC handler for 'unity.window.move_resize'.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *     If successful adds null terminated strings for each output coordinates.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloMoveResizeWindow(RpcInData *data)      // IN/OUT
{
   DynBuf *buf = &gTcloUpdate;
   UnityWindowId window;
   UnityRect moveResizeRect = {0};
   char temp[1024];

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

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   if (sscanf(data->args, "%u %d %d %d %d",
              &window,
              &moveResizeRect.x,
              &moveResizeRect.y,
              &moveResizeRect.width,
              &moveResizeRect.height) != 5) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments.",
                              FALSE);
   }

   if (!Unity_MoveResizeWindow(window, &moveResizeRect)) {
      Debug("%s: Could not read window coordinates.\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Could not read window coordinates",
                              FALSE);
   }

   /*
    *  Send back the new (post move/resize operation) window coordinates.
    */

   DynBuf_SetSize(buf, 0);
   Str_Sprintf(temp, sizeof temp, "%d %d %d %d", moveResizeRect.x,
               moveResizeRect.y, moveResizeRect.width, moveResizeRect.height);
   DynBuf_AppendString(buf, temp);

   /*
    * Write the final result into the result out parameters and return!
    */

   data->result = (char *)DynBuf_Get(buf);
   data->resultLen = DynBuf_GetSize(buf);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetDesktopConfig --
 *
 *     RPC handler for 'unity.set.desktop.config'. The RPC takes the form of:
 *     {1,1} {1,2} {2,1} {2,2} 1
 *     for a 2 x 2 virtual desktop where the upper right {1,2} is the currently
 *     active desktop.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     Might change virtual desktop configuration in the guest.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloSetDesktopConfig(RpcInData *data)      // IN/OUT
{
   unsigned int index = 0;
   char *desktopStr = NULL;
   char *errorMsg;
   uint32 initialDesktopIndex = 0;
   UnityVirtualDesktopArray desktopConfig;

   memset(&desktopConfig, 0, sizeof desktopConfig);

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

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   if (data->argsSize == 0) {
      errorMsg = "Invalid arguments: desktop config is expected";
      goto error;
   }

   /* Read the virtual desktop configuration. */
   while ((desktopStr = StrUtil_GetNextToken(&index, data->args, " ")) != NULL) {

      if (sscanf(desktopStr, "{%d,%d}",
                 &desktopConfig.desktops[desktopConfig.desktopCount].x,
                 &desktopConfig.desktops[desktopConfig.desktopCount].y) == 2) {
         if (desktopConfig.desktopCount >= MAX_VIRT_DESK - 1) {
            errorMsg = "Invalid arguments: too many desktops";
            goto error;
         }
         desktopConfig.desktopCount++;
      } else if (sscanf(desktopStr, "%u", &initialDesktopIndex) == 1) {
         if (initialDesktopIndex >= desktopConfig.desktopCount) {
            errorMsg = "Invalid arguments: current desktop is out of bounds";
            goto error;
         }
         /* All done with arguments at this point - stop processing */
         free(desktopStr);
         break;
      } else {
         errorMsg = "Invalid arguments: invalid desktop config";
         goto error;
      }
      free(desktopStr);
      desktopStr = NULL;
   }

   /*
    * Call the platform specific function to set the desktop configuration.
    */

   if (!Unity_SetDesktopConfig(&desktopConfig)) {
      errorMsg = "Could not set desktop configuration";
      goto error;
   }

   if (!Unity_SetInitialDesktop(initialDesktopIndex)) {
      errorMsg = "Could not set initial desktop";
      goto error;
   }

   return RPCIN_SETRETVALS(data,
                           "",
                           TRUE);
error:
   free(desktopStr);
   Debug("%s: %s\n", __FUNCTION__, errorMsg);

   return RPCIN_SETRETVALS(data,
                           errorMsg,
                           FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetDesktopActive --
 *
 *     RPC handler for 'unity.set.desktop.active'.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     Might change the active virtual desktop in the guest.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloSetDesktopActive(RpcInData *data)      // IN/OUT
{
   UnityDesktopId desktopId = 0;
   char *errorMsg;

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

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   if (Unity_IsActive() == FALSE) {
      errorMsg = "Unity not enabled - cannot change active desktop";
      goto error;
   }

   if (sscanf(data->args, " %d", &desktopId) != 1) {
      errorMsg = "Invalid arguments: expected \"desktopId\"";
      goto error;
   }

   /*
    * Call the platform specific function to set the desktop active.
    */

   if (!Unity_SetDesktopActive(desktopId)) {
      errorMsg = "Could not set active desktop";
      goto error;
   }

   return RPCIN_SETRETVALS(data,
                           "",
                           TRUE);
error:
   Debug("%s: %s\n", __FUNCTION__, errorMsg);
   return RPCIN_SETRETVALS(data,
                           errorMsg,
                           FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetWindowDesktop --
 *
 *     RPC handler for 'unity.set.window.desktop'.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     Might change the active virtual desktop in the guest.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloSetWindowDesktop(RpcInData *data)   // IN/OUT
{
   UnityWindowId windowId;
   uint32 desktopId = 0;
   char *errorMsg;

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

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, data->name, data->args);

   if (Unity_IsActive() == FALSE) {
      errorMsg = "Unity not enabled - cannot set window desktop";
      goto error;
   }

   if (sscanf(data->args, " %u %d", &windowId, &desktopId) != 2) {
      errorMsg = "Invalid arguments: expected \"windowId desktopId\"";
      goto error;
   }

   /*
    * Call the platform specific function to move the window to the
    * specified desktop.
    */

   if (!Unity_SetWindowDesktop(windowId, desktopId)) {
      errorMsg = "Could not move the window to the desktop";
      goto error;
   }

   return RPCIN_SETRETVALS(data,
                           "",
                           TRUE);
error:
   Debug("%s: %s\n", __FUNCTION__, errorMsg);
   return RPCIN_SETRETVALS(data,
                           errorMsg,
                           FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetUnityOptions --
 *
 *     Set the Unity options - must be be called before entering Unity mode.
 *
 * Results:
 *     TRUE if RPC was succesfully handled.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloSetUnityOptions(RpcInData *data)
{
   Bool ret = TRUE;
   UnityOptions optionsMsg;

   memset(&optionsMsg, 0, sizeof optionsMsg);

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
    * Deserialize the XDR data. Note that the data begins with args + 1 since
    * there is a space between the RPC name and the XDR serialization.
    */
   if (!XdrUtil_Deserialize((char *)data->args + 1, data->argsSize - 1,
                            (void *)xdr_UnityOptions, &optionsMsg)) {
      Debug("%s: Failed to deserialize data\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Failed to deserialize data.", FALSE);
      goto exit;
   }

   Unity_SetUnityOptions(optionsMsg.UnityOptions_u.unityOptionsV1->featureMask);

   ret = RPCIN_SETRETVALS(data,
                          "",
                          TRUE);
exit:
   VMX_XDR_FREE(xdr_UnityOptions, &optionsMsg);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloRequestWindowContents --
 *
 *     Request the window contents for a set of windows.
 *
 * Results:
 *     TRUE if all the window IDs are valid.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

gboolean
UnityTcloRequestWindowContents(RpcInData *data)    // IN
{
   Bool ret = TRUE;
   UnityWindowContentsRequest requestMsg;
   UnityWindowContentsRequestV1 *requestV1 = NULL;
   memset(&requestMsg, 0, sizeof requestMsg);

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
    * Deserialize the XDR data. Note that the data begins with args + 1 since
    * there is a space between the RPC name and the XDR serialization.
    */
   if (!XdrUtil_Deserialize((char *)data->args + 1, data->argsSize - 1,
                            (void *)xdr_UnityWindowContentsRequest, &requestMsg)) {
      Debug("%s: Failed to deserialize data\n", __FUNCTION__);
      ret = RPCIN_SETRETVALS(data, "Failed to deserialize data.", FALSE);
      goto exit;
   }

   if (requestMsg.ver != UNITY_WINDOW_CONTENTS_V1) {
      Debug("%s: Unexpected XDR version = %d\n", __FUNCTION__, requestMsg.ver);
      goto exit;
   }

   requestV1 = requestMsg.UnityWindowContentsRequest_u.requestV1;

   /*
    * Call the platform implementation of the RPC handler.
    */
   if (!Unity_RequestWindowContents(requestV1->windowID.windowID_val,
                                    requestV1->windowID.windowID_len)) {
      ret = RPCIN_SETRETVALS(data, "Invalid list of windows.", FALSE);
      goto exit;
   }

   ret = RPCIN_SETRETVALS(data,
                          "",
                          TRUE);
exit:
   VMX_XDR_FREE(xdr_UnityWindowContentsRequest, &requestMsg);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityUpdateState --
 *
 *     Communicate unity state changes to vmx.
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
UnityUpdateState(void)
{
   Bool ret = TRUE;
   XDR xdrs;
   UnityActiveProto message;
   char *val;

   if (DynXdr_Create(&xdrs) == NULL) {
      return FALSE;
   }

   val = Str_Asprintf(NULL, "%s ", UNITY_RPC_UNITY_ACTIVE);
   if (!val || !DynXdr_AppendRaw(&xdrs, val, strlen(val))) {
      Debug("%s: Failed to create state string.\n", __FUNCTION__);
      ret = FALSE;
      goto out;
   }
   memset(&message, 0, sizeof message);
   message.ver = UNITY_ACTIVE_V1;
   message.UnityActiveProto_u.unityActive = Unity_IsActive();
   if (!xdr_UnityActiveProto(&xdrs, &message)) {
      Debug("%s: Failed to append message content.\n", __FUNCTION__);
      ret = FALSE;
      goto out;
   }

   if (!RpcOut_SendOneRaw(DynXdr_Get(&xdrs), xdr_getpos(&xdrs), NULL, NULL)) {
      Debug("%s: Failed to send Unity state RPC.\n", __FUNCTION__);
      ret = FALSE;
   } else {
      Debug("%s: success\n", __FUNCTION__);
   }
out:
   free(val);
   DynXdr_Destroy(&xdrs, TRUE);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityXdrRequestOperation --
 *
 *    XDR encoder function for UnityRequestOperation.
 *
 *    See UnityXdrSendRpc().
 *
 * Results:
 *    Returns true if the XDR struct was encoded successfully.
 *
 * Side-effects:
 *    None.
 *------------------------------------------------------------------------------
 */

Bool
UnityXdrRequestOperation(XDR *xdrs,    // IN
                         void *arg)    // IN
{
   ASSERT(xdrs);
   ASSERT(arg);
   return xdr_UnityRequestOperation(xdrs, (UnityRequestOperation *) arg);
}


/*
 *------------------------------------------------------------------------------
 *
 * UnitySendRequestMinimizeOperation --
 *
 *     Send a request for a minimize operation to the host.
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

Bool
UnitySendRequestMinimizeOperation(UnityWindowId windowId,   // IN
                                  uint32 sequence)          // IN
{
   Bool ret = FALSE;
   UnityRequestOperation msg;
   UnityRequestOperationV1 v1;
   memset(&msg, 0, sizeof msg);
   memset(&v1, 0, sizeof v1);

   Debug("%s: Enter.\n", __FUNCTION__);

   v1.windowId = windowId;
   v1.sequence = sequence;
   v1.details.op = MINIMIZE;

   msg.ver = UNITY_OP_V1;
   msg.UnityRequestOperation_u.unityRequestOpV1 = &v1;

   ret = UnityXdrSendRpc(UNITY_RPC_REQUEST_OPERATION,
                         &UnityXdrRequestOperation,
                         &msg);

   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnitySendWindowContents --
 *
 *     Sends the content of a window to the host, as a PNG encoded image. If the
 *     image is larger than the maximum size of a GuestMsg, this function breaks
 *     the image down into a number of chunks, then transfers each of the chunks
 *     independently. See guest_msg_def.h and unity.x.
 *
 * Results:
 *     Returns true if the image was transferred successfully.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnitySendWindowContents(UnityWindowId windowID, // IN
                        uint32 imageWidth,      // IN
                        uint32 imageHeight,     // IN
                        const char *imageData,  // IN
                        uint32 imageLength)     // IN
{
   Bool ret = FALSE;
   uint32 count = 0;                /* count of chunks sent */
   uint32 len = 0;                  /* length of the next chunk */
   const char *readptr = imageData; /* pointer to start of next chunk in imageData */

   ASSERT(imageWidth > 0);
   ASSERT(imageHeight > 0);
   ASSERT(imageLength > 0);
   ASSERT(imageData);

   Debug("%s: Enter.\n", __FUNCTION__);
   Debug("%s: Sending contents of window 0x%x.\n", __FUNCTION__, windowID);
   Debug("%s: Contents are (%u x %u) image, %u bytes.\n", __FUNCTION__,
         imageWidth, imageHeight, imageLength);

   /* Send the unity.window.contents.start RPC to the host. */
   if (!UnitySendWindowContentsStart(windowID,
                                     imageWidth,
                                     imageHeight,
                                     imageLength)) {
      goto exit;
   }

   /* Send the image data. */
   while (imageLength > 0) {
      /*
       * Get the length of the next chunk to send, up to a maximum of
       * UNITY_WINDOW_CONTENTS_MAX_CHUNK_SIZE bytes.
       */
      len = MIN(UNITY_WINDOW_CONTENTS_MAX_CHUNK_SIZE, imageLength);

      Debug("%s: Sending chunk %u at offset 0x%p, size %u.\n", __FUNCTION__,
            count, readptr, len);

      /* Send the next chunk to the host. */
      if (!UnitySendWindowContentsChunk(windowID, count, readptr, len)) {
         goto exit;
      }

      count++;
      readptr += len;
      imageLength -= len;
   }

   /* Send the unity.window.contents.end RPC to the host. */
   if (!UnitySendWindowContentsEnd(windowID)) {
      goto exit;
   }

   ret = TRUE;

exit:
   return ret;
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityXdrEncodeWindowContentsStart --
 *
 *    XDR encoder function for UnityWindowContentsStart.
 *
 *    See UnityXdrSendRpc().
 *
 * Results:
 *    Returns true if the XDR struct was encoded successfully.
 *
 * Side effects:
 *    None.
 *------------------------------------------------------------------------------
 */

Bool
UnityXdrEncodeWindowContentsStart(XDR *xdrs,
                                  void *arg)
{
   ASSERT(xdrs);
   ASSERT(arg);
   return xdr_UnityWindowContentsStart(xdrs, (UnityWindowContentsStart *) arg);
}


/*
 *------------------------------------------------------------------------------
 *
 * UnitySendWindowContentsStart --
 *
 *    Sends the unity.window.contents.start RPC to the host.
 *
 * Results:
 *    Returns true if the RPC was sent successfully.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnitySendWindowContentsStart(UnityWindowId windowID, // IN
                             uint32 imageWidth,      // IN
                             uint32 imageHeight,     // IN
                             uint32 imageLength)     // IN
{
   Bool ret = FALSE;
   UnityWindowContentsStart msg;
   UnityWindowContentsStartV1 v1;

   memset(&msg, 0, sizeof msg);
   memset(&v1, 0, sizeof v1);

   Debug("%s: Enter.\n", __FUNCTION__);

   v1.windowID = windowID;
   v1.imageWidth  = imageWidth;
   v1.imageHeight = imageHeight;
   v1.imageLength = imageLength;

   msg.ver = UNITY_WINDOW_CONTENTS_V1;
   msg.UnityWindowContentsStart_u.startV1 = &v1;

   ret = UnityXdrSendRpc(UNITY_RPC_WINDOW_CONTENTS_START,
                         &UnityXdrEncodeWindowContentsStart,
                         &msg);

   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityXdrEncodeWindowContentsChunk --
 *
 *    XDR encoder function for UnityWindowContentsChunk.
 *
 *    See UnityXdrSendRpc().
 *
 * Results:
 *    Returns true if the XDR struct was encoded successfully.
 *
 * Side-effects:
 *    None.
 *------------------------------------------------------------------------------
 */

Bool
UnityXdrEncodeWindowContentsChunk(XDR *xdrs,
                                  void *arg)
{
   ASSERT(xdrs);
   ASSERT(arg);
   return xdr_UnityWindowContentsChunk(xdrs, (UnityWindowContentsChunk *) arg);
}


/*
 *------------------------------------------------------------------------------
 *
 * UnitySendWindowContentsChunk --
 *
 *    Sends a unity.window.contents.chunk RPC to the host.
 *
 * Results:
 *    Returns true if the RPC was sent successfully.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnitySendWindowContentsChunk(UnityWindowId windowID,
                             uint32 chunkID,
                             const char *data,
                             uint32 len)
{
   Bool ret = FALSE;
   UnityWindowContentsChunk msg;
   UnityWindowContentsChunkV1 v1;
   memset(&msg, 0, sizeof msg);
   memset(&v1, 0, sizeof v1);

   Debug("%s: Enter.\n", __FUNCTION__);

   v1.windowID = windowID;
   v1.chunkID = chunkID;
   v1.data.data_val = (char *) data;
   v1.data.data_len = len;

   msg.ver = UNITY_WINDOW_CONTENTS_V1;
   msg.UnityWindowContentsChunk_u.chunkV1 = &v1;

   ret = UnityXdrSendRpc(UNITY_RPC_WINDOW_CONTENTS_CHUNK,
                         &UnityXdrEncodeWindowContentsChunk,
                         &msg);

   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityXdrEncodeWindowContentsEnd --
 *
 *    XDR encoder function for UnityWindowContentsEnd.
 *
 * Results:
 *    Returns true if the XDR struct was encoded successfully.
 *
 * Side effects:
 *    None.
 *------------------------------------------------------------------------------
 */

Bool
UnityXdrEncodeWindowContentsEnd(XDR *xdrs,
                                void *arg)
{
   ASSERT(xdrs);
   ASSERT(arg);
   return xdr_UnityWindowContentsEnd(xdrs, (UnityWindowContentsEnd*) arg);
}


/*
 *------------------------------------------------------------------------------
 *
 * UnitySendWindowContentsEnd --
 *
 *    Sends a unity.window.contents.end RPC to the host.
 *
 * Results:
 *    Returns true if the RPC was sent successfully.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnitySendWindowContentsEnd(UnityWindowId windowID)
{
   Bool ret = FALSE;
   UnityWindowContentsEnd msg;
   UnityWindowContentsEndV1 v1;
   memset(&msg, sizeof msg, 0);
   memset(&v1, sizeof v1, 0);

   Debug("%s: Enter.\n", __FUNCTION__);

   v1.windowID = windowID;

   msg.ver = UNITY_WINDOW_CONTENTS_V1;
   msg.UnityWindowContentsEnd_u.endV1 = &v1;

   ret = UnityXdrSendRpc(UNITY_RPC_WINDOW_CONTENTS_END,
                         &UnityXdrEncodeWindowContentsEnd,
                         &msg);

   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityXdrSendRpc --
 *
 *    Sends an RPC with XDR-serialized arguments to the host. The provided
 *    encodeFn will be called to perform XDR encoding of the RPC, with the XDR
 *    struct and the provided data pointer as its parameters.
 *
 * Returns:
 *    True if the RPC was sent successfully.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnityXdrSendRpc(const char *rpcName,
                UnityXdrEncodeFunc encodeFn,
                void *data)
{
   Bool ret = FALSE;
   XDR xdrs;
   memset(&xdrs, 0, sizeof xdrs);

   ASSERT(rpcName);

   Debug("%s: Enter.\n", __FUNCTION__);

   if (!DynXdr_Create(&xdrs)) {
      Debug("%s: Failed to create DynXdr.\n", __FUNCTION__);
      goto exit;
   }

   if (!DynXdr_AppendRaw(&xdrs, rpcName, strlen(rpcName))) {
      Debug("%s: Failed to append RPC name to DynXdr.\n", __FUNCTION__);
      goto dynxdr_destroy;
   }

   if (!DynXdr_AppendRaw(&xdrs, " ", 1)) {
      Debug("%s: Failed to append space to DynXdr.\n", __FUNCTION__);
      goto dynxdr_destroy;
   }

   if (!(*encodeFn)(&xdrs, data)) {
      Debug("%s: Failed to serialize RPC data.\n", __FUNCTION__);
      goto dynxdr_destroy;
   }

   if (!RpcOut_SendOneRaw(DynXdr_Get(&xdrs), xdr_getpos(&xdrs), NULL, NULL)) {
      Debug("%s: Failed to send RPC.\n", __FUNCTION__);
      goto dynxdr_destroy;
   }

   ret = TRUE;

dynxdr_destroy:
   DynXdr_Destroy(&xdrs, TRUE);

exit:
   Debug("%s: Exit.\n", __FUNCTION__);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityBuildUpdates --
 *
 *     Gather a round of Unity Updates from the Window Tracker. Initialize our
 *     dynbuf used to hold the updates, call to get the updates into our channel
 *     and add the terminating NULL. If updates were added to the buffer send them
 *     to the host.
 *
 * Results:
 *     TRUE if updates were added to the buffer and succesfully sent to the host.
 *
 * Side effects:
 *     DynBuf is updated with serialized form of the updates.
 *
 *----------------------------------------------------------------------------
 */

Bool
UnityBuildUpdates(void *param,         // IN
                  int flags)           // IN
{
   Bool retVal = TRUE;
   UnityUpdateChannel *updateChannel = reinterpret_cast<UnityUpdateChannel*>(param);

   ASSERT(updateChannel);

   DynBuf_SetSize(&updateChannel->updates, updateChannel->cmdSize);
   Unity_GetUpdates(flags);
   /*
    * Write the final '\0' to the DynBuf to signal that we're all out of
    * updates.
    */
   DynBuf_AppendString(&updateChannel->updates, "");

   /*
    * If there are no updates, Unity_GetUpdates appended an empty string, i.e. null.
    */

   if (DynBuf_GetSize(&updateChannel->updates) > updateChannel->cmdSize + 1) {
      if (!UnitySendUpdates(updateChannel)) {
         retVal = FALSE;
      }
   }
   return retVal;
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityShouldShowTaskbar --
 *
 *    Sends an RPC to retrieve whether the guest should show the taskbar and
 *    returns the value.
 *
 * Results:
 *    TRUE if the taskbar should be visible.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnityShouldShowTaskbar()
{
   char *reply = NULL;
   size_t replyLen;
   Bool showTaskbar = FALSE;

   if (!RpcOut_sendOne(&reply, &replyLen, UNITY_RPC_VMX_SHOW_TASKBAR)) {
      Debug("%s: could not get the VMX show taskbar setting, assuming FALSE\n",
            __FUNCTION__);
      showTaskbar = FALSE;
   } else {
      uint32 value = 0;

      if (StrUtil_StrToUint(&value, reply)) {
         showTaskbar = (value == 0) ? FALSE : TRUE;
      } else {
         showTaskbar = FALSE;
      }
   }
   return showTaskbar;
}

} /* namespace tools */ } /* namespace vmware */
