/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * @file event.cc
 *
 * Provides code relating the Glib main loop and Xlib/X11 event sources.
 */


extern "C" {
   #include <stdlib.h>
   #include <stdio.h>
   #include "vmware.h"
}

#include "x11Platform.h"


/*
 * File scope prototypes.
 */


static void ConnectionWatch(Display *display, XPointer client_data, int fd,
                            int opening, XPointer *watch_data);
static gboolean TeardownHashRemove(gpointer key, gpointer value,
                                   gpointer user_data);
static gboolean USourcePrepare(GSource *source, gint *timeout);
static gboolean USourceCheck(GSource *source);
static gboolean USourceDispatch(GSource *source, GSourceFunc callback,
                                gpointer cbData);


/*
 * Library scope functions.
 */


/*
 ******************************************************************************
 * UnityX11EstablishSource --                                            */ /**
 *
 * @brief Creates Glib event source for X11 events.  Attaches to the Glib event
 * loop.
 *
 * @param[in] up        Our Unity platform context.
 *
 ******************************************************************************
 */

void
UnityX11EventEstablishSource(UnityPlatform *up) // IN
{
   static GSourceFuncs unitySourceFuncs = {0};
   unitySourceFuncs.prepare = USourcePrepare;
   unitySourceFuncs.check = USourceCheck;
   unitySourceFuncs.dispatch = USourceDispatch;

   UnityGSource *uSource;

   ASSERT(up);
   ASSERT(up->display);

   uSource = (UnityGSource *)g_source_new(&unitySourceFuncs, sizeof *uSource);
   uSource->up = up;
   uSource->fdTable = g_hash_table_new(g_direct_hash, g_direct_equal);

   up->glibSource = uSource;

   /* Hook our main X11 connection into our event source. */
   ConnectionWatch(up->display, (XPointer)up, ConnectionNumber(up->display), TRUE,
                   NULL);

   /* If Xlib opens an internal connection, bind it to the source, too. */
   XAddConnectionWatch(up->display, &ConnectionWatch, (XPointer)up);

   /* Attach the source to the event loop. */
   g_source_set_callback((GSource*)uSource, UnityX11HandleEvents, up, NULL);
   g_source_attach((GSource*)uSource, NULL);

   /* Transfer ownership to the event loop. */
   g_source_unref((GSource*)uSource);
}


/*
 ******************************************************************************
 * UnityX11TeardownSource --                                             */ /**
 *
 * @brief Detach Unity from our Glib event loop.
 *
 * @param[in] up        Our Unity platform context.
 *
 ******************************************************************************
 */

void
UnityX11EventTeardownSource(UnityPlatform *up) // IN
{
   UnityGSource *uSource = up->glibSource;

   ASSERT(up);
   ASSERT(uSource);

   /* Detach Xlib internal connection notification from the Glib event loop. */
   XRemoveConnectionWatch(up->display, &ConnectionWatch, (XPointer)up);

   /* Detach all Xlib file descriptors from our Glib event source. */
   g_hash_table_foreach_remove(uSource->fdTable, TeardownHashRemove, uSource);
   g_hash_table_unref(uSource->fdTable);
   uSource->fdTable = NULL;

   /* Destroy the Glib event source. */
   g_source_destroy((GSource*)uSource);
   up->glibSource = NULL;
}


/*
 * File scope functions.
 */


/*
 ******************************************************************************
 * ConnectionWatch --                                                    */ /**
 *
 * @brief Binds Xlib internal connections to Glib event sources.
 *
 * When Xlib or its extensions create new X11 connections, they're bound to
 * a @a Display as <em>"internal connections"</em>.  When Xlib processes the
 * incoming event queue, it pulls requests from @em all of these connections,
 * not just the main event connection.  As such, we should monitor all of
 * them.
 *
 * @param[in] display     X11 display context.
 * @param[in] client_data Our UnityPlatform context.
 * @param[in] fd          Relevant file descriptor.
 * @param[in] opening     TRUE when @a fd is opened, FALSE when @a fd is closed.
 * @param[in] watch_data  Unused.
 *
 * @sa XAddConnectionWatch
 *
 ******************************************************************************
 */

static void
ConnectionWatch(Display *display,       // IN
                XPointer client_data,   // IN
                int fd,                 // IN
                int opening,            // IN
                XPointer *watch_data)   // UNUSED
{
   UnityPlatform *up;
   UnityGSource *uSource;

   up = (UnityPlatform *)client_data;

   ASSERT(up);             // Make sure we're correctly registered.
   ASSERT(up->isRunning);  // This cb should be stripped before we exit Unity.
   ASSERT(up->glibSource); // This function is useless w/o an established source.
   ASSERT(display == up->display);

   uSource = up->glibSource;

   if (opening) {
      /*
       * Add new a new file descriptor to the poll array.
       */
      GPollFD *newFd = g_new0(GPollFD, 1);

      ASSERT(g_hash_table_lookup(uSource->fdTable, GINT_TO_POINTER(fd)) == NULL);

      newFd->fd = fd;
      newFd->events = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL;

      g_hash_table_insert(uSource->fdTable, GINT_TO_POINTER(fd), newFd);
      g_source_add_poll((GSource*)uSource, newFd);
   } else {
      /*
       * Remove a file descriptor from the poll array.
       */
      GPollFD *oldFd = (GPollFD*)g_hash_table_lookup(uSource->fdTable, GINT_TO_POINTER(fd));

      if (oldFd) {
         g_source_remove_poll((GSource *)uSource, oldFd);
         g_hash_table_remove(uSource->fdTable, GINT_TO_POINTER(fd));
         g_free(oldFd);
      }
   }
}


/*
 ******************************************************************************
 * TeardownHashRemove --                                                 */ /**
 *
 * @brief Assists TeardownSource with destroying its GPollFD hash table.
 *
 * @param[in] key       GINT_TO_POINTER-converted file descriptor.
 * @param[in] value     Corresponding GPollFD *.
 * @param[in] user_data Our UnityGSource.
 *
 * @see g_hash_table_foreach_remove
 *
 ******************************************************************************
 */

static gboolean
TeardownHashRemove(gpointer key,        // IN
                   gpointer value,      // IN
                   gpointer user_data)  // IN
{
   GPollFD *oldFd = (GPollFD*)value;
   UnityGSource *uSource = (UnityGSource*)user_data;

   ASSERT(value);
   ASSERT(user_data);

   g_source_remove_poll((GSource *)uSource, oldFd);
   g_free(oldFd);

   return TRUE;
}


/*
 ******************************************************************************
 * USourcePrepare --                                                     */ /**
 *
 * @brief See GSourceFuncs::prepare.
 *
 * @param[in]  source   Points to our UnityGSource.
 * @param[out] timeout  May specify a maximum timeout value for poll(2).
 *
 * @retval TRUE  Source is ready.
 * @retval FALSE Source not yet ready.
 *
 ******************************************************************************
 */

static gboolean
USourcePrepare(GSource *source, // IN
               gint *timeout)   // OUT
{
   UnityGSource *uSource = (UnityGSource *)source;

   /*
    * "It sets the returned timeout to -1 to indicate that it doesn't mind how
    * long the poll() call blocks."
    *  - http://library.gnome.org/devel/glib/unstable/glib-The-Main-Event-Loop.html#GSourceFuncs
    */
   *timeout = -1;

   return XQLength(uSource->up->display) > 0;
}


/*
 ******************************************************************************
 * USourceCheck --                                                       */ /**
 *
 * @brief See GSourceFuncs::check.
 *
 * @param[in]  source   Points to our UnityGSource.
 *
 * @retval TRUE  Source is ready.
 * @retval FALSE Source not yet ready.
 *
 * @todo Should exit Unity upon file descriptor error.
 *
 ******************************************************************************
 */

static gboolean
USourceCheck(GSource *source)   // IN
{
   UnityGSource *uSource = (UnityGSource *)source;
   gboolean haveData = FALSE;

   /*
    * XXX Could/should test for FD errors here.
    */
   if (XQLength(uSource->up->display)) {
      haveData = TRUE;
   } else {
      GList *pollFds;
      GList *listIter;

      pollFds = g_hash_table_get_values(uSource->fdTable);

      for (listIter = pollFds; listIter; listIter = listIter->next) {
         GPollFD *pollFd = (GPollFD*)listIter->data;

         if (pollFd->revents & G_IO_IN) {
            haveData = TRUE;
            break;
         }
      }

      g_list_free(pollFds);
   }

   return haveData;
}


/*
 ******************************************************************************
 * USourceDispatch --                                                    */ /**
 *
 * @brief See GSourceFuncs::dispatch.
 *
 * @param[in] source    Points to our UnityGSource.
 * @param[in] callback  Points to the user's callback.
 * @param[in] cbData    Points to user's callback data.
 *
 * @retval TRUE  Glib should continue monitoring the source.
 * @retval FALSE Glib should stop monitoring the source.
 *
 ******************************************************************************
 */

static gboolean
USourceDispatch(GSource *source,      // IN
                GSourceFunc callback, // IN
                gpointer cbData)      // IN
{
   /*
    * Remind callers to attach a callback.
    */
   ASSERT(callback);
   ASSERT(cbData);

   return callback(cbData);
}
