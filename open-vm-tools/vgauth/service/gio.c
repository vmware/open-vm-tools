/*********************************************************
 * Copyright (C) 2011-2016,2018-2019 VMware, Inc. All rights reserved.
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
 * @file gio.c --
 *
 *    Uses gio for setting up the main loop and watching for I/O.
 */


#include <signal.h>

#include "VGAuthLog.h"
#include "serviceInt.h"
#include "service.h"

static GMainLoop *mainLoop = NULL;

#define  VERBOSE_IO_DEBUG  1


static gboolean ServiceEndMainLoop(gpointer userData);


#ifdef _WIN32
typedef struct HandleGSource_ {
   /*
    * Do not put any new member above the gSource member
    * Add future new member after the gSource member
    */
   GSource gSource;
   GPollFD gPollFd;
} HandleGSource;


/**
 * Callback for the "prepare()" event source function. Does nothing.
 *
 * @param[in]  src         Unused.
 * @param[out] timeout     Set to -1.
 *
 * @return FALSE
 */

static gboolean
HandleGSourcePrepare(GSource *src,
                     gint *timeout)
{
   *timeout = -1;
   return FALSE;
}


/**
 * Checks whether the handle has been signaled.
 *
 * @param[in]  _src     The event source.
 *
 * @return Whether the handle is signaled.
 */

static gboolean
HandleGSourceCheck(GSource *_src)
{
   HandleGSource *src = (HandleGSource *) _src;
   return (src->gPollFd.revents & G_IO_IN);
}


/**
 * Calls the callback associated with the handle, if any.
 *
 * @param[in]  src         Unused.
 * @param[in]  callback    The callback to be called.
 * @param[in]  data        User-supplied data.
 *
 * @return The return value of the callback, or FALSE if the callback is NULL.
 */

static gboolean
HandleGSourceDispatch(GSource *src,
                      GSourceFunc callback,
                      gpointer data)
{
   return (callback != NULL) ? callback(data) : FALSE;
}


/**
 * Does nothing. The main glib code already does all the cleanup needed.
 *
 * @param[in]  _src     The event source.
 */

static void
HandleGSourceFinalize(GSource *_src)
{
}


/*
 ******************************************************************************
 * ServiceIONewHandleGSource --                                          */ /**
 *
 * Creates a new glib event source based on a given Windows handle
 *
 * @param[in]   h                 The Windows handle
 * @param[in]   func              The callback function of the event souce
 * @param[in]   data              The callback param of the event source
 *
 * @code
 *
 *    GSource *src = ServiceIONewHandleGSource(h);
 *    g_source_attach(src, glibContext);
 * @return      Pointer to the new glib event source
 *
 ******************************************************************************
 */

static GSource *
ServiceIONewHandleGSource(HANDLE h,
                          GSourceFunc func,
                          gpointer data)
{
   static GSourceFuncs srcFuncs = {
      HandleGSourcePrepare,
      HandleGSourceCheck,
      HandleGSourceDispatch,
      HandleGSourceFinalize,
      NULL,
      NULL
   };

   HandleGSource *ret;

   ASSERT(h != NULL && h != INVALID_HANDLE_VALUE);

   ret = (HandleGSource *) g_source_new(&srcFuncs, sizeof *ret);
   ret->gPollFd.fd = (intptr_t) h;
   ret->gPollFd.events = G_IO_IN;
   ret->gPollFd.revents = 0;

   g_source_add_poll(&ret->gSource, &ret->gPollFd);
   g_source_set_callback(&ret->gSource, func, data, NULL);

   return &ret->gSource;
}
#endif

#ifndef _WIN32

/*
 ******************************************************************************
 * ServiceSighupHandler --                                               */ /**
 *
 * Glib-wrapped signal handler for SIGHUP.
 *
 ******************************************************************************
 */

gboolean
ServiceSighupHandler(gpointer data)
{
   Log("Processing SIGHUP\n");
   Pref_Shutdown(gPrefs);
   gPrefs = Pref_Init(VGAUTH_PREF_CONFIG_FILENAME);
   Service_InitLogging(FALSE, TRUE);
   Service_ReloadPrefs();

   return TRUE;      // don't remove source
}


/*
 ******************************************************************************
 * ServiceSigtermHandler --                                              */ /**
 *
 * Glib-wrapped signal handler for SIGTERM.
 *
 ******************************************************************************
 */

gboolean
ServiceSigtermHandler(gpointer data)
{
   Log("Processing SIGTERM; service exiting\n");
   Pref_Shutdown(gPrefs);
   (void) ServiceEndMainLoop(NULL);
   Service_Shutdown();
   Log("END SERVICE");
   VMXLog_Log(VMXLOG_LEVEL_INFO, "%s END SERVICE",
              VGAUTH_SERVICE_NAME);
   VMXLog_Shutdown();

   /*
    * It's safe to just exit here, since we've been called by the glib
    * mainloop so cannot be in the middle of processing a request.
    */
   exit(0);

   /* NOTREACHED */
   return FALSE;
}


/*
 ******************************************************************************
 * ServiceSetSignalHandlers --                                           */ /**
 *
 * Sets up the signal handlers we care about.
 *
 ******************************************************************************
 */

void
ServiceSetSignalHandlers(void)
{
#define CATCH_SIGNAL(signum, fn)                                        \
   do {                                                                 \
      GSource *src;                                                     \
      src = ServiceNewSignalSource(signum);                             \
      g_source_set_callback(src, fn, NULL, NULL);                       \
      g_source_attach(src, NULL);                                       \
      g_source_unref(src);                                              \
   } while (0)

   /*
    * HUP means re-read prefs.
    */
   CATCH_SIGNAL(SIGHUP, ServiceSighupHandler);

   /*
    * TERM, QUIT, INT all exit cleanly.
    */
   CATCH_SIGNAL(SIGTERM, ServiceSigtermHandler);
   CATCH_SIGNAL(SIGQUIT, ServiceSigtermHandler);
   CATCH_SIGNAL(SIGINT, ServiceSigtermHandler);

#undef CATCH_SIGNAL
}

#endif   // !_WIN32


/*
 ******************************************************************************
 * ServiceIOHandleIO --                                                  */ /**
 * ServiceIOHandleIOGSource --                                           */ /**
 *
 * GIO callback function for IO on a socket.
 *
 * @param[in]   chan              The GIO channel with activity.
 * @param[in]   cond              The type of GIO activity.
 * @param[in]   userData          The userData specified when the callback
 *                                was set up.  The ServiceConnection.
 *
 * @return gboolean
 *
 ******************************************************************************
 */

static gboolean
ServiceIOHandleIOGSource(gpointer userData)
{
   ServiceConnection *conn = (ServiceConnection *) userData;
   VGAuthError err;

   /*
    * read data and try to parse it.  may be a partial.
    */
   err = ServiceProtoReadAndProcessRequest(conn);

   if (err != VGAUTH_E_OK) {
      return FALSE;
   }

   /*
    * Windows needs to initiate a new async read IO before polling again.
    * Do it here instead of immediately after the read() since we like to
    * do copyless buffer optimization later.
    * Read buffer needs to be processed before issuing another async read.
    */
#ifdef _WIN32
   ServiceNetworkStartRead(conn);
#endif

   return TRUE;
}


static gboolean
ServiceIOHandleIO(GIOChannel *chan,
                  GIOCondition cond,
                  gpointer userData)
{
   return ServiceIOHandleIOGSource(userData);
}


/*
 ******************************************************************************
 * ServiceIOAccept --                                                    */ /**
 * ServiceIOAcceptGSource --                                             */ /**
 *
 * GIO callback function for IO on a listening socket.
 * This should be called whenever there is activity on a listen socket.
 * It will accept() the new connection, creating a new ServiceConnection,
 * and start watching for IO on that new connection.
 *
 * @param[in]   chan              The GIO channel with activity.
 * @param[in]   cond              The type of GIO activity.
 * @param[in]   userData          The userData specified when the callback
 *                                was set up.  The ServiceConnection.
 *
 * @return gboolean
 *
 ******************************************************************************
 */


static gboolean
ServiceIOAcceptGSource(gpointer userData)
{
   ServiceConnection *newConn = NULL;
   ServiceConnection *lConn = (ServiceConnection *) userData;
   VGAuthError err = VGAUTH_E_OK;
#ifdef _WIN32
   GSource *gSourceData;
#else
   GIOChannel *echan;
#endif

   err = ServiceConnectionClone(lConn, &newConn);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to clone a new connection\n", __FUNCTION__);
      return FALSE;
   }

   err = ServiceAcceptConnection(lConn, newConn);
   if (VGAUTH_E_OK == err) {
      VGAUTH_LOG_DEBUG("Established a new pipe connection %d on %s", newConn->connId,
                       newConn->pipeName);
#ifdef _WIN32
      gSourceData = ServiceIONewHandleGSource(newConn->ol.hEvent,
                                              ServiceIOHandleIOGSource,
                                              (gpointer) newConn);
      newConn->gioId = g_source_attach(gSourceData, NULL);
      g_source_unref(gSourceData);
#else
      echan = g_io_channel_unix_new(newConn->sock);
      newConn->gioId = g_io_add_watch(echan, G_IO_IN, ServiceIOHandleIO,
                            (gpointer) newConn);
      g_io_channel_unref(echan);
#endif
   } else if (VGAUTH_E_TOO_MANY_CONNECTIONS == err) {
      ServiceConnectionShutdown(newConn);
   } else {
      ServiceConnectionShutdown(lConn);
      ServiceConnectionShutdown(newConn);
   }

   return TRUE;
}

static gboolean
ServiceIOAccept(GIOChannel *chan,
                GIOCondition cond,
                gpointer userData)
{
#if VERBOSE_IO_DEBUG
   Debug("%s: condition: %d\n", __FUNCTION__, cond);
#endif

   return ServiceIOAcceptGSource(userData);

}

/*
 ******************************************************************************
 * ServiceIOStartListen --                                            */ /**
 *
 * Starts listening on a ServiceConnection.
 * Creates a GIO channel to watch for activity
 *
 * @param[in]   conn              The ServiceConnection on which to start
 *                                listening.
 *
 * @return VGAuthError
 *
 ******************************************************************************
 */


VGAuthError
ServiceIOStartListen(ServiceConnection *conn)
{
   VGAuthError err = VGAUTH_E_OK;
#ifdef _WIN32
   GSource *gSource;
#else
   GIOChannel *lChan;
#endif

#ifdef _WIN32
   gSource = ServiceIONewHandleGSource(conn->ol.hEvent, ServiceIOAcceptGSource,
                                       (gpointer) conn);
   conn->gioId = g_source_attach(gSource, NULL);
   g_source_unref(gSource);
#else
   lChan = g_io_channel_unix_new(conn->sock);
   conn->gioId = g_io_add_watch(lChan, G_IO_IN, ServiceIOAccept, (gpointer) conn);
   g_io_channel_unref(lChan);
#endif

   return err;
}


/*
 ******************************************************************************
 * ServiceIOStopIO --                                                  */ /**
 *
 * Removes a GIO callback for a connection.
 *
 * @param[in]   conn           The connection to stop watching for activity.
 *
 * @return VGAuthError
 *
 ******************************************************************************
 */

VGAuthError
ServiceStopIO(ServiceConnection *conn)
{
   /*
    * The connection may not have been added to gpoll
    * This also make it idempotent
    */
   if (conn->gioId > 0) {
      g_source_remove(conn->gioId);
      conn->gioId = 0;
   }

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * ServiceIOPrepareMainLoop --                                         */ /**
 *
 * Prepares the glib main loop.
 *
 * @return VGAuthError
 *
 ******************************************************************************
 */

VGAuthError
ServiceIOPrepareMainLoop(void)
{
   VGAuthError err = VGAUTH_E_OK;

   mainLoop = g_main_loop_new(NULL, FALSE);

   return err;
}


/*
 ******************************************************************************
 * ServiceEndMainLoop --                                                 */ /**
 *
 * Tells the main to loop to exit.
 *
 * @param[in]  userData  The callback data (unused).
 *
 * @return FALSE to remove the source -- should only ever be called once.
 ******************************************************************************
 */

static gboolean
ServiceEndMainLoop(gpointer userData)
{
   Log("%s: about to stop main loop\n", __FUNCTION__);

   g_main_loop_quit(mainLoop);

   return FALSE;
}


#ifdef _WIN32
/*
 ******************************************************************************
 * ServiceIORegisterQuitEvent --                                         */ /**
 *
 * Registers an event that tells the main loop to exit.
 *
 * @param[in]  hQuitEvent The event that will be signaled when it's time to exit.
 *
 * @return VGAuthError
 *
 ******************************************************************************
 */

VGAuthError
ServiceIORegisterQuitEvent(HANDLE hQuitEvent)
{
   GSource *source;

   source = ServiceIONewHandleGSource(hQuitEvent, ServiceEndMainLoop, NULL);
   (void) g_source_attach(source, NULL);

   return VGAUTH_E_OK;
}
#endif


/*
 ******************************************************************************
 * ServiceIOMainLoop --                                            */ /**
 *
 * Runs the glib main loop.
 *
 * @return NEVER
 *
 ******************************************************************************
 */

VGAuthError
ServiceIOMainLoop(void)
{
   g_main_loop_run(mainLoop);

   Log("%s: main loop has exited", __FUNCTION__);

   return VGAUTH_E_OK;
}
