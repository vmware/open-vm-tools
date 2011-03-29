/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 * @file sessionMgr.c
 *
 * Support for X session management using the, well, X Session Management
 * Library (libSM) with a little help from the Inter-Client Exchange Library
 * (libICE).  This allows vmusr to receive X session lifecycle events and clean
 * up appropriately upon session termination.  For now, cleanup upon shutdown
 * is the only activity we're interested in.
 *
 * A custom event source is used to bind libICE connections with GLib's main
 * loop and dispatch messages.  As this is the first (and hopefully only)
 * libICE client, all ICE interaction is handled here (as opposed to in a
 * provider application).
 *
 * This should work with any session manager implementing XSMP, covering all
 * of our supported desktop environments.
 *
 * @todo Scrutinize libICE error handling.  I/O errors should be handled here,
 *       but errors handled by libICE's default may exit().
 */


/* Include first.  Sets G_LOG_DOMAIN. */
#include "desktopEventsInt.h"
#include "vmware.h"

#include <errno.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/SM/SMlib.h>
#include <X11/ICE/ICElib.h>


/*
 * Identifier used for SessionMgr private data in DesktopEvents private hash
 * table.
 */
#define DE_FEATURE_KEY  "sessionMgr"


/*
 * libICE integration declarations.
 */


typedef struct {
   GSource *iceSource;
   IceConn  iceCnx;
} ICEWatchCtx;

static gboolean ICEDispatch(GIOChannel *chn, GIOCondition cond, gpointer cbData);
static void ICEIOErrorHandler(IceConn iceCnx);
static void ICEWatch(IceConn iceCnx, IcePointer clientData, Bool opening,
                     IcePointer *watchData);


/*
 * libSM callbacks.
 */


static void SMDieCb(SmcConn smcCnx, SmPointer cbData);
static void SMSaveYourselfCb(SmcConn smcCnx, SmPointer cbData, int saveType,
                             Bool shutdown, int interactStyle, Bool fast);
static void SMSaveCompleteCb(SmcConn smcCnx, SmPointer cbData);
static void SMShutdownCancelledCb(SmcConn smcCnx, SmPointer cbData);


/*
 ******************************************************************************
 * SessionMgr_Init --                                                    */ /**
 *
 * Register custom ICE connection source and sign up with the session manager.
 *
 * @param[in] ctx   Application context.
 * @param[in] pdata Plugin data.
 *
 * @retval TRUE Always "succeeds".  Session management is optional.
 *
 ******************************************************************************
 */

gboolean
SessionMgr_Init(ToolsAppCtx *ctx,
                ToolsPluginData *pdata)
{
   SmcCallbacks smCallbacks;
   unsigned long cbMask =
        SmcSaveYourselfProcMask
      | SmcDieProcMask
      | SmcSaveCompleteProcMask
      | SmcShutdownCancelledProcMask;
   SmcConn smcCnx;
   char errorBuf[128];
   char *clientID = NULL;

   IceSetIOErrorHandler(ICEIOErrorHandler);
   IceAddConnectionWatch(ICEWatch, pdata);

   memset(&smCallbacks, 0, sizeof smCallbacks);
   smCallbacks.save_yourself.callback = &SMSaveYourselfCb;
   smCallbacks.save_complete.callback = &SMSaveCompleteCb;
   smCallbacks.shutdown_cancelled.callback = &SMShutdownCancelledCb;
   smCallbacks.die.callback = &SMDieCb;
   smCallbacks.die.client_data = pdata;

   smcCnx =
      SmcOpenConnection(NULL, NULL, SmProtoMajor, SmProtoMinor, cbMask,
                        &smCallbacks, NULL, &clientID, sizeof errorBuf, errorBuf);
   if (smcCnx != NULL) {
      g_hash_table_insert(pdata->_private, DE_FEATURE_KEY, smcCnx);
      g_debug("Registered with session manager as %s\n", clientID);
      free(clientID);
   } else {
      g_message("Failed to register with session manager.\n");
      g_message("SmcOpenConnection: %s\n", errorBuf);
      IceRemoveConnectionWatch(ICEWatch, pdata);
   }

   return TRUE;
}


/*
 ******************************************************************************
 * SessionMgr_Shutdown --                                                */ /**
 *
 * Unregisters signal callbacks and disappears from the message bus.
 *
 * @param[in] ctx   Application context.
 * @param[in] pdata Plugin data.
 *
 ******************************************************************************
 */

void
SessionMgr_Shutdown(ToolsAppCtx *ctx,
                    ToolsPluginData *pdata)
{
   GHashTable *desktopData = pdata->_private;
   SmcConn smcCnx = g_hash_table_lookup(desktopData, DE_FEATURE_KEY);
   if (smcCnx != NULL) {
      SmcCloseConnection(smcCnx, 0, NULL);
      IceRemoveConnectionWatch(ICEWatch, pdata);
      g_hash_table_remove(desktopData, DE_FEATURE_KEY);
   }
}


/*
 ******************************************************************************
 * BEGIN libICE stuff.
 */


/*
 * A note on source reference counting:
 *
 * There are two entities that maintain references on ICEWatchCtx's GSource.
 *    - GLib's GMainContext, and
 *    - libICE's IceConn (via ICEWatch's watchData).
 *
 * A source's initial reference created in ICEWatch may be considered as
 * transferred to libICE until ICEWatch is again called upon connection close.
 * The second reference comes from attaching the source to the GLib main loop.
 */


/*
 ******************************************************************************
 * ICEIOErrorHandler --                                                  */ /**
 *
 * Handler for libICE I/O errors.
 *
 * Does nothing but replaces libICE's default handler which would've caused us
 * to exit.
 *
 * @param[in]  iceCnx    Opaque ICE connection descriptor.
 *
 ******************************************************************************
 */

static void
ICEIOErrorHandler(IceConn iceCnx)
{
   g_message("%s: %s\n", __func__, strerror(errno));
}


/*
 ******************************************************************************
 * ICEDispatch --                                                        */ /**
 *
 * GSource dispatch routine.  Calls IceProcessMessages on the ICE connection.
 *
 * @param[in]  chn      GIOChannel event source
 * @param[in]  cond     condition satisfied (ignored)
 * @param[in]  cbData   (ICEWatchCtx*) Channel context.
 *
 * @retval TRUE  Underlying iceCnx is still valid, so do not kill this source.
 * @retval FALSE Underlying iceCnx was destroyed, so remove this source.
 *
 ******************************************************************************
 */

static gboolean
ICEDispatch(GIOChannel *chn,
            GIOCondition cond,
            gpointer cbData)
{
   IceProcessMessagesStatus status;
   ICEWatchCtx *watchCtx = cbData;
   ASSERT(watchCtx);

   /*
    * We ignore the error conditions here and let IceProcessMessages return
    * an IceProcessMessagesIOError, resulting in a single, shared error code
    * path.
    */

   status = IceProcessMessages(watchCtx->iceCnx, NULL, NULL);
   switch (status) {
   case IceProcessMessagesSuccess:
      return TRUE;
   case IceProcessMessagesIOError:
      IceCloseConnection(watchCtx->iceCnx);    // Let ICEWatch kill the source.
      return TRUE;
   case IceProcessMessagesConnectionClosed:
      /*
       * iceCnx was invalidated, so we won't see another call to ICEWatch,
       * so we'll return FALSE and let GLib destroy the source for us.
       */
      watchCtx->iceCnx = NULL;
      g_source_unref(watchCtx->iceSource);
      return FALSE;
   }

   NOT_REACHED();
}


/*
 ******************************************************************************
 * ICEWatch --                                                           */ /**
 *
 * libICE connection watching callback.
 *
 * Creates or removes GLib event sources upon ICE connection creation/
 * destruction.
 *
 * From ICElib.xml:
 *    "If opening is True the client should set the *watch_data pointer to
 *    any data it may need to save until the connection is closed and the
 *    watch procedure is invoked again with opening set to False."
 *
 * @param[in]  iceCnx    Opaque ICE connection descriptor.
 * @parma[in]  cbData    (ToolsPluginData*) plugin data
 * @param[in]  opening   True if creating a connection, False if closing.
 * @param[in]  watchData See above.  New source will be stored here.
 *
 ******************************************************************************
 */

static void
ICEWatch(IceConn iceCnx,
         IcePointer cbData,
         Bool opening,
         IcePointer *watchData)
{
   ToolsPluginData *pdata = cbData;
   ToolsAppCtx *ctx;

   ctx = g_hash_table_lookup(pdata->_private, DE_PRIVATE_CTX);
   ASSERT(ctx);

   if (opening) {
      GIOChannel *iceChannel;
      GSource *iceSource;
      ICEWatchCtx *watchCtx;
      GError *error = NULL;

      iceChannel = g_io_channel_unix_new(IceConnectionNumber(iceCnx));
      if (g_io_channel_set_encoding(iceChannel, NULL, &error) != G_IO_STATUS_NORMAL) {
         g_warning("%s: g_io_channel_set_encoding: %s\n", __func__, error->message);
         g_clear_error(&error);
         g_io_channel_unref(iceChannel);
         return;
      }
      g_io_channel_set_buffered(iceChannel, FALSE);

      iceSource = g_io_create_watch(iceChannel, G_IO_IN|G_IO_HUP|G_IO_ERR);
      g_io_channel_unref(iceChannel);   // Ownership transferred to iceSource.

      watchCtx = g_new(ICEWatchCtx, 1);
      watchCtx->iceSource = iceSource;
      watchCtx->iceCnx = iceCnx;
      *watchData = watchCtx;

      VMTOOLSAPP_ATTACH_SOURCE(ctx, iceSource, &ICEDispatch, watchCtx, NULL);
   } else {
      ICEWatchCtx *watchCtx = *watchData;
      if (watchCtx) {
         watchCtx->iceCnx = NULL;
         if (watchCtx->iceSource) {
            g_source_destroy(watchCtx->iceSource);
            g_source_unref(watchCtx->iceSource);
         }
         g_free(watchCtx);
         *watchData = NULL;
      }
   }
}


/*
 * END libICE stuff.
 ******************************************************************************
 */


/*
 ******************************************************************************
 * BEGIN libSM stuff.
 */


/*
 ******************************************************************************
 * SMDieCb --                                                            */ /**
 *
 * Callback for a XSM "Die" event.
 *
 * Instructs the main loop to quit, then acknowledges the session manager's
 * request.
 *
 * @param[in]  smcCnx   Opaque XSM connection object.
 * @param[in]  cbData   (ToolsPluginData*) Plugin data.
 *
 ******************************************************************************
 */

static void
SMDieCb(SmcConn smcCnx,
        SmPointer cbData)
{
   ToolsPluginData *pdata = cbData;
   ToolsAppCtx *ctx = g_hash_table_lookup(pdata->_private, DE_PRIVATE_CTX);
   ASSERT(ctx);

   g_message("Session manager says our time is up.  Exiting.\n");
   g_main_loop_quit(ctx->mainLoop);
   SmcCloseConnection(smcCnx, 0, NULL);
}


/*
 ******************************************************************************
 * SMSaveYourselfCb --                                                   */ /**
 *
 * Callback for XSM "SaveYourself" event.
 *
 * This event is sent to all XSM clients either to checkpoint a session
 * or in advance of a (cancellable) session  shutdown event.  If we needed
 * time to persist state, now would be the time to do it.  Since we don't,
 * however, this is nearly a no-op -- we only acknowledge the manager.
 *
 * @param[in]  smcCnx   Opaque XSM connection object.
 * @param[in]  cbData   (ToolsPluginData*) Plugin data.
 * @param[in]  saveType Refer to SMlib.xml.
 * @param[in]  shutdown Checkpoint or shutdown?
 * @param[in]  interactStyle May interact with user?
 * @param[in]  fast     Shutdown as quickly as possible.
 *
 * @todo Consider whether it'd make sense to unregister capabilities and pause
 *       other plugins if shutdown == True until either we receive a 'die' or
 *       'shutdown cancelled' event.
 *
 ******************************************************************************
 */

static void
SMSaveYourselfCb(SmcConn smcCnx,
                 SmPointer cbData,
                 int saveType,
                 Bool shutdown,
                 int interactStyle,
                 Bool fast)
{
   SmcSaveYourselfDone(smcCnx, True);
}


/*
 ******************************************************************************
 * SMSaveCompleteCb --                                                   */ /**
 *
 * Callback for XSM "SaveComplete" event.
 *
 * State has been checkpointed.  Application may resume normal operations.
 * Total no-op.
 *
 * @param[in]  smcCnx   Opaque XSM connection object.
 * @param[in]  cbData   (ToolsPluginData*) Plugin data.
 *
 ******************************************************************************
 */

static void
SMSaveCompleteCb(SmcConn smcCnx,
                 SmPointer cbData)
{
}


/*
 ******************************************************************************
 * SMShutdownCancelledCb --                                              */ /**
 *
 * Callback for XSM "ShutdownCancelled" event.
 *
 * User cancelled shutdown.  May resume normal operations.  Again, total no-op.
 *
 * @param[in]  smcCnx   Opaque XSM connection object.
 * @param[in]  cbData   (ToolsPluginData*) Plugin data.
 *
 ******************************************************************************
 */

static void
SMShutdownCancelledCb(SmcConn smcCnx,
                      SmPointer cbData)
{
}


/*
 * END libSM stuff.
 ******************************************************************************
 */
