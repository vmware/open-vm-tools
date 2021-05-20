/*********************************************************
 * Copyright (C) 2004-2019,2021 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * pollGtk.c -- a simple poll implementation built on top of GLib
 * For historical reasons, it is named pollGtk but does not
 * depend on gtk.
 * This is the actual Poll_* functions, and so it is different
 * than the Gtk IVmdbPoll implementation.
 *
 * This has to be at least slightly thread-safe. Specifically,
 * it has to allow any thread to schedule callbacks on the Poll
 * thread. For example, the asyncSocket library may schedule a
 * callback in a signal handler when a socket is suddenly
 * disconnected. As a result, we need to wrap a lock around the
 * queue of events.
 */


#ifdef _WIN32
#include <winsock2.h>
#pragma pack(push, 8)
#endif
#include <glib.h>
#ifdef _WIN32
#pragma pack(pop)
#endif

#include "pollImpl.h"
#include "mutexRankLib.h"
#include "err.h"

#define LOGLEVEL_MODULE poll
#include "loglevel_user.h"


/*
 * This describes a single callback waiting for an event
 * or a timeout.
 */
typedef struct {
   int            flags;
   PollerFunction cb;
   void          *clientData;
   PollClassSet   classSet;
   MXUserRecLock *cbLock;
   uint32         timesNotFired;
} PollEntryInfo;

typedef struct PollGtkEntry {
   PollEntryInfo  read;
   PollEntryInfo  write;

   PollEventType  type;
   PollDevHandle  event;       /* POLL_DEVICE file descriptor or POLL_REALTIME
                                  delay. */
   guint          gtkInputId;  /* Handle of the registered GTK callback  */
   /*
    * In practice, "channel" is only used when invoking the callbacks of clients
    * who registered with POLL_FLAG_FD.  When you create a channel from a file
    * descriptor with g_io_channel_win32_new_fd(), you MUST read from the
    * channel with g_io_channel_read() instead of reading directly from the fd
    * with read(fd).
    *
    * See http://library.gnome.org/devel/glib/unstable/glib-IO-Channels.html#g-io-channel-win32-new-fd for details.
    */
   GIOChannel *channel;
} PollGtkEntry;


/*
 * This describes a data necessary to find matching entry.
 */
typedef struct {
   int            flags;
   PollerFunction cb;
   void          *clientData;
   PollClassSet   classSet;
   PollEventType  type;
   Bool           matchAnyClientData;
} PollGtkFindEntryData;


/*
 * The global Poll state.
 */
typedef struct Poll {
   MXUserExclLock *lock;

   GHashTable     *deviceTable;
   GHashTable     *timerTable;
#ifdef _WIN32
   GHashTable     *signaledTable;
   GSList         *newSignaled;
   Bool            signaledInUse;
   guint           retrySource;
#endif
} Poll;

static Poll *pollState;
static gsize inited = 0;

static VMwareStatus
PollGtkCallback(PollClassSet classSet,   // IN
                int flags,               // IN
                PollerFunction f,        // IN
                void *clientData,        // IN
                PollEventType type,      // IN
                PollDevHandle info,      // IN
                MXUserRecLock *lock);    // IN

static gboolean PollGtkBasicCallback(gpointer data);

static gboolean PollGtkEventCallback(GIOChannel *source,
                                     GIOCondition condition,
                                     gpointer data);
static gboolean PollGtkEventCallbackWork(GIOChannel *source,
                                         GIOCondition condition,
                                         gpointer data,
                                         Bool hasPollLock,
                                         Bool *firedAll);
#ifdef _WIN32
static gboolean PollGtkFireSignaled(gpointer key,
                                    gpointer value,
                                    gpointer user_data);
static gboolean PollGtkFireSignaledList(gpointer data);
#endif

static void PollGtkRemoveOneCallback(gpointer data);

#define ASSERT_POLL_LOCKED()                                    \
   ASSERT(!pollState || !pollState->lock ||                     \
          MXUser_IsCurThreadHoldingExclLock(pollState->lock))

#define LOG_ENTRY(_l, _str, _e, _isWrite)                                     \
   do {                                                                       \
      if (_isWrite) {                                                         \
         LOG(_l, "POLL: entry %p (wcb %p, data %p, flags %x, type %x)" _str,  \
             (_e), (_e)->write.cb, (_e)->write.clientData,                    \
             (_e)->write.flags, (_e)->type);                                  \
      } else {                                                                \
         LOG(_l, "POLL: entry %p (rcb %p, data %p, flags %x, type %x)" _str,  \
             (_e), (_e)->read.cb, (_e)->read.clientData,                      \
             (_e)->read.flags, (_e)->type);                                   \
      }                                                                       \
   } while (0)

typedef void (*PollerFunctionGtk)(void *, GIOChannel *);


/*
 *----------------------------------------------------------------------------
 *
 * PollGtkLock --
 * PollGtkUnlock --
 *
 *      Locking of the internal poll state.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
PollGtkLock(void)
{
   MXUser_AcquireExclLock(pollState->lock);
}


static INLINE void
PollGtkUnlock(void)
{
   MXUser_ReleaseExclLock(pollState->lock);
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkInit --
 *
 *      Module initialization.
 *
 * Results:
 *       None
 *
 * Side effects:
 *       Initializes the module-wide state and sets pollState.
 *
 *----------------------------------------------------------------------
 */

static void
PollGtkInit(void)
{
   ASSERT(pollState == NULL);
   pollState = g_new0(Poll, 1);

   pollState->lock = MXUser_CreateExclLock("pollGtkLock",
                                           RANK_pollDefaultLock);

   pollState->deviceTable = g_hash_table_new_full(g_direct_hash,
                                                  g_direct_equal,
                                                  NULL,
                                                  PollGtkRemoveOneCallback);
   ASSERT(pollState->deviceTable);

   pollState->timerTable = g_hash_table_new_full(g_direct_hash,
                                                 g_direct_equal,
                                                 NULL,
                                                 PollGtkRemoveOneCallback);
   ASSERT(pollState->timerTable);

#ifdef _WIN32
   pollState->signaledTable = g_hash_table_new(g_direct_hash,
                                               g_direct_equal);
   ASSERT(pollState->signaledTable);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkExit --
 *
 *      Module exit.
 *
 * Results:
 *       None
 *
 * Side effects:
 *       Discards the module-wide state and clears pollState.
 *
 *----------------------------------------------------------------------
 */

static void
PollGtkExit(void)
{
   Poll *poll = pollState;

   ASSERT(poll != NULL);

   PollGtkLock();
   g_hash_table_destroy(poll->deviceTable);
   g_hash_table_destroy(poll->timerTable);
   poll->deviceTable = NULL;
   poll->timerTable = NULL;
#ifdef _WIN32
   g_hash_table_destroy(poll->signaledTable);
   poll->signaledTable = NULL;
   g_slist_free(poll->newSignaled);
   poll->newSignaled = NULL;
   if (poll->retrySource > 0) {
      g_source_remove(poll->retrySource);
      poll->retrySource = 0;
   }
#endif
   PollGtkUnlock();

   MXUser_DestroyExclLock(poll->lock);

   g_free(poll);
   pollState = NULL;
   inited = 0;
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkLoopTimeout --
 *
 *       The poll loop.
 *       This is defined here to allow libraries like Foundry to link.
 *       When run with the Gtk Poll implementation, however, this routine
 *       should never be called. The Gtk framework will pump events.
 *
 * Result:
 *       Void.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static void
PollGtkLoopTimeout(Bool loop,          // IN: loop forever if TRUE, else do one pass.
                   Bool *exit,         // IN: NULL or set to TRUE to end loop.
                   PollClass class,    // IN: class of events (POLL_CLASS_*)
                   int timeout)        // IN: maximum time to sleep
{
   NOT_IMPLEMENTED();
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkEntryInfoMatches --
 *
 *      Test whether provided PollEntryInfo satisfies FindEntryData
 *      requirements.
 *
 * Results:
 *      TRUE if the value matches our search criteria, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE gboolean
PollGtkEntryInfoMatches(const PollEntryInfo        *entry,  // IN
                        const PollGtkFindEntryData *search) // IN
{
   return PollClassSet_Equals(entry->classSet, search->classSet) &&
          entry->cb == search->cb && entry->flags == search->flags &&
          (search->matchAnyClientData || entry->clientData == search->clientData);
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkFindReadPredicate --
 *
 *      Predicate usable by GHashTable iteration functions to find
 *      specific elements, looking for read entry.
 *
 * Results:
 *      TRUE if the value matches our search criteria, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
PollGtkFindReadPredicate(gpointer key,   // IN
                         gpointer value, // IN
                         gpointer data)  // IN
{
   const PollGtkEntry *current = value;
   const PollGtkFindEntryData *search = data;

   ASSERT_POLL_LOCKED();
   return current->type == search->type &&
          PollGtkEntryInfoMatches(&current->read, search);
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkFindWritePredicate --
 *
 *      Predicate usable by GHashTable iteration functions to find
 *      specific elements, looking for write entry.
 *
 * Results:
 *      TRUE if the value matches our search criteria, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
PollGtkFindWritePredicate(gpointer key,   // IN
                          gpointer value, // IN
                          gpointer data)  // IN
{
   const PollGtkEntry *current = value;
   const PollGtkFindEntryData *search = data;

   ASSERT_POLL_LOCKED();
   return current->type == search->type &&
          PollGtkEntryInfoMatches(&current->write, search);
}


#ifdef _WIN32
/*
 *----------------------------------------------------------------------------
 *
 * PollGtkFireSignaled --
 *
 *      Fire the callback(s) of a previously signaled event, if the callback(s)
 *      is still registered.
 *
 * Results:
 *      TRUE if we fire all callbacks that are ready to fire.
 *
 * Side effects:
 *      If returning TRUE, the associated key is removed from signaledTable.
 *
 *----------------------------------------------------------------------------
 */

static gboolean
PollGtkFireSignaled(gpointer key,        // IN: PollDevHandle
                    gpointer value,      // IN: PollGtkEntry *
                    gpointer user_data)  // IN: unused
{
   PollGtkEntry *entry;
   Bool firedAll = TRUE;
   GIOCondition condition = 0;

   entry = g_hash_table_lookup(pollState->deviceTable, key);

   if (entry == NULL) {
      goto exit;
   }
   if (entry->read.cb && entry->read.timesNotFired > 0) {
      condition |= G_IO_IN;
   }
   if (entry->write.cb && entry->write.timesNotFired > 0) {
      condition |= G_IO_OUT;
   }
   if (condition == 0) {
      goto exit;
   }

   PollGtkEventCallbackWork(NULL, condition, entry, TRUE, &firedAll);

exit:
   LOG(4, "POLL: entry %p %s\n", entry, entry && condition ?
       (firedAll ? "fired" : "not all fired") : "not ready to fire");
   return firedAll;
}


/*
 *----------------------------------------------------------------------------
 *
 * PollGtkFireSignaledList --
 *
 *      This is a timer based callback scheduled when the signaled list is not
 *      empty.  For each entry, we will try to fire any callback with non-zero
 *      timesNotFired.  The entry is removed from the signaled list only if
 *      all the callbacks that we try to fire do fire.
 *
 * Results:
 *      TRUE if this callback should fire again, FALSE otherwise.
 *
 * Side effects:
 *      Timer source is removed if FALSE is returned.
 *
 *----------------------------------------------------------------------------
 */

static gboolean
PollGtkFireSignaledList(gpointer data) // IN: unused
{
   Poll *poll = pollState;
   gboolean ret;
   GSList *cur;

   ASSERT(poll);
   PollGtkLock();

   /*
    * Do not allow other changes to signaledTable while iterating through the
    * hash table.  (The poll lock is dropped when callback fires.)
    */
   poll->signaledInUse = TRUE;

   g_hash_table_foreach_remove(pollState->signaledTable,
                               PollGtkFireSignaled, NULL);

   /* Now we can add any new signaled entry into the hash table. */
   for (cur = poll->newSignaled; cur; cur = g_slist_next(cur)) {
      PollGtkEntry *entry;
      gpointer key = cur->data;

      entry = g_hash_table_lookup(poll->deviceTable, key);
      if (entry) {
         g_hash_table_replace(poll->signaledTable, key, entry);
      }
   }
   g_slist_free(poll->newSignaled);
   poll->newSignaled = NULL;

   /* Return TRUE to keep this function firing, FALSE to unregister. */
   if (g_hash_table_size(poll->signaledTable) > 0) {
      LOG(5, "POLL: not removing retry source\n");
      ret = TRUE;
   } else {
      LOG(5, "POLL: no retry remains; removing timer source\n");
      poll->retrySource = 0;
      ret = FALSE;
   }

   poll->signaledInUse = FALSE;
   PollGtkUnlock();

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * PollGtkAddToSignaledList --
 *
 *      Remember a signaled event so that we can retry later.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
PollGtkAddToSignaledList(PollGtkEntry *entry)  // IN
{
   Poll *poll = pollState;
   gpointer key;

   ASSERT_POLL_LOCKED();
   ASSERT(entry);

   key = (gpointer)(intptr_t)entry->event;

   /*
    * Add it to a separate linked list if the poll thread is iterating over
    * signaledTable.
    */
   if (poll->signaledInUse) {
      if (!g_slist_find(poll->newSignaled, key)) {
         poll->newSignaled = g_slist_prepend(poll->newSignaled, key);
         LOG(4, "POLL: added entry %p event 0x%x to signaled list\n",
             entry, entry->event);
      }
   } else {
      g_hash_table_replace(poll->signaledTable, key, entry);
      if (poll->retrySource == 0) {
         poll->retrySource = g_timeout_add(0, PollGtkFireSignaledList, NULL);
      }
      LOG(4, "POLL: added entry %p event 0x%x to signaled hash table\n",
          entry, entry->event);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * PollGtkReadableSocketCheck --
 *
 *      Windows does not signal a read event for a socket if there has not
 *      been a recv() since the last time the socket is signaled (even
 *      after a new event is associated with the socket).  We check the socket
 *      and add it to the signaled list if there is data to read.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
PollGtkReadableSocketCheck(PollGtkEntry *entry)  // IN
{
   char buf[1];
   int ret;

   ASSERT_POLL_LOCKED();
   ASSERT(entry->read.cb && entry->read.flags & POLL_FLAG_SOCKET);

   ret = recv(entry->event, buf, 1, MSG_PEEK);
   if (ret == 1) {
      entry->read.timesNotFired = 1;
      PollGtkAddToSignaledList(entry);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * PollGtkWritableSocketCheck --
 *
 *      Windows does not signal a write event for a socket if a previous
 *      write had not resulted in WSAEWOULDBLOCK.  Do a zero-byte send()
 *      to find out if it would block and if not, add the event to the
 *      signaled list so that it will fire.
 *
 * Results:
 *      TRUE if socket is writable.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
PollGtkWritableSocketCheck(PollGtkEntry *entry)  // IN
{
   int ret;
   char c;

   ASSERT_POLL_LOCKED();
   ASSERT(entry->write.cb && entry->write.flags & POLL_FLAG_SOCKET);

   ret = send(entry->event, &c, 0, 0);
   if (ret == SOCKET_ERROR) {
      if (GetLastError() != WSAEWOULDBLOCK) {
         LOG(1, "POLL error while doing zero-byte send: %u %s\n",
             GetLastError(), Err_ErrString());
      }
      return FALSE;
   } else {
      entry->write.timesNotFired = 1;
      PollGtkAddToSignaledList(entry);
      return TRUE;
   }
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * PollGtkDeviceCallback --
 *
 *      Insert specified entry as device callback.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
PollGtkDeviceCallback(PollGtkEntry *entry)  // IN
{
   Poll *poll = pollState;
   int conditionFlags;

   ASSERT_POLL_LOCKED();
   conditionFlags = G_IO_ERR | G_IO_HUP | G_IO_NVAL;
   if (POLL_FLAG_READ & entry->read.flags) {
      conditionFlags |= G_IO_IN | G_IO_PRI;
   }
   if (POLL_FLAG_WRITE & entry->write.flags) {
      conditionFlags |= G_IO_OUT;
   }

   /*
    * XXX Looking at the GTK/GLIB source code, it seems that a returned value
    *     of 0 indicates failure (and I should check for it), but that is not
    *     clear
    */
#ifdef _WIN32
   if ((entry->read.flags | entry->write.flags) & POLL_FLAG_SOCKET) {
      entry->channel = g_io_channel_win32_new_socket(entry->event);

      if (entry->read.flags & POLL_FLAG_READ) {
         PollGtkReadableSocketCheck(entry);
      }
      if (entry->write.flags & POLL_FLAG_WRITE) {
         PollGtkWritableSocketCheck(entry);
      }
   } else if ((entry->read.flags | entry->write.flags) & POLL_FLAG_FD) {
      entry->channel = g_io_channel_win32_new_fd(entry->event);
   } else {
      entry->channel = g_io_channel_win32_new_messages(entry->event);
   }
#else
   entry->channel = g_io_channel_unix_new(entry->event);
#endif
   entry->gtkInputId = g_io_add_watch(entry->channel,
                                      conditionFlags,
                                      PollGtkEventCallback,
                                      entry);

   g_hash_table_insert(poll->deviceTable, (gpointer)(intptr_t)entry->event,
                       entry);
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkCallbackRemoveEntry --
 *
 *      Remove specified poll entry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
PollGtkCallbackRemoveEntry(PollGtkEntry *foundEntry,  // IN
                           Bool removeWrite)          // IN
{
   Poll *poll = pollState;

   ASSERT_POLL_LOCKED();
   if (foundEntry->type == POLL_DEVICE) {
      PollGtkEntry *newEntry = NULL;
      intptr_t key;

      if (removeWrite) {
         if (foundEntry->read.flags) {
            newEntry = g_new0(PollGtkEntry, 1);
            newEntry->read = foundEntry->read;
            LOG_ENTRY(2, " to be removed, read cb remains\n", foundEntry, TRUE);
         } else {
            LOG_ENTRY(2, " to be removed\n", foundEntry, TRUE);
         }
      } else {
         if (foundEntry->write.flags) {
            newEntry = g_new0(PollGtkEntry, 1);
            newEntry->write = foundEntry->write;
            LOG_ENTRY(2, " to be removed, write cb remains\n", foundEntry,
                      FALSE);
         } else {
            LOG_ENTRY(2, " to be removed\n", foundEntry, FALSE);
         }
      }

      key = foundEntry->event;
      g_hash_table_remove(poll->deviceTable, (gpointer)key);
#ifdef _WIN32
      if (!poll->signaledInUse) {
         g_hash_table_remove(poll->signaledTable, (gpointer)key);
      }
#endif
      if (newEntry) {
         newEntry->event = key;
         newEntry->type = POLL_DEVICE;
         PollGtkDeviceCallback(newEntry);
      }
   } else {
      ASSERT(!removeWrite);
      ASSERT(foundEntry->write.cb == NULL);
      LOG_ENTRY(2, " to be removed\n", foundEntry, FALSE);
      if (!g_hash_table_remove(poll->timerTable,
                               (gpointer)(intptr_t)foundEntry->gtkInputId)) {
         LOG(2, "POLL: entry %p not found\n", foundEntry);
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkCallbackRemoveInt --
 *
 *      Remove a callback.
 *
 * Results:
 *      TRUE if entry found and removed, FALSE otherwise
 *
 * Side effects:
 *      A callback may be modified instead of completely removed.
 *
 *----------------------------------------------------------------------
 */

static Bool
PollGtkCallbackRemoveInt(PollClassSet classSet,           // IN
                         int flags,                       // IN
                         PollerFunction f,                // IN
                         void *clientData,                // IN
                         Bool matchAnyClientData,         // IN
                         PollEventType type,              // IN
                         void **foundClientData)          // OUT
{
   Poll *poll = pollState;
   GHashTable *searchTable;
   PollGtkFindEntryData searchEntry;
   PollGtkEntry *foundEntry;

   ASSERT(poll);
   ASSERT(!clientData || !matchAnyClientData);
   ASSERT(type >= 0 && type < POLL_NUM_QUEUES);
   ASSERT(foundClientData);

   searchEntry.classSet = classSet;
   searchEntry.flags = flags;
   searchEntry.cb = f;
   searchEntry.clientData = clientData;
   searchEntry.type = type;
   searchEntry.matchAnyClientData = matchAnyClientData;

   switch (type) {
   case POLL_REALTIME:
   case POLL_MAIN_LOOP:
      searchTable = poll->timerTable;
      break;
   case POLL_DEVICE:
      searchTable = poll->deviceTable;
      break;
   case POLL_VIRTUALREALTIME:
   case POLL_VTIME:
   default:
      NOT_IMPLEMENTED();
   }

   PollGtkLock();

   if (flags & POLL_FLAG_WRITE) {
      foundEntry = g_hash_table_find(searchTable,
                                     PollGtkFindWritePredicate,
                                     &searchEntry);
   } else {
      foundEntry = g_hash_table_find(searchTable,
                                     PollGtkFindReadPredicate,
                                     &searchEntry);
   }
   if (foundEntry) {
      if (flags & POLL_FLAG_WRITE) {
         *foundClientData = foundEntry->write.clientData;
         PollGtkCallbackRemoveEntry(foundEntry, TRUE);
      } else {
         *foundClientData = foundEntry->read.clientData;
         PollGtkCallbackRemoveEntry(foundEntry, FALSE);
      }
   } else {
      LOG(1, "POLL: no matching entry for cb %p, data %p, flags %x, type %x\n",
          f, clientData, flags, type);
   }

   PollGtkUnlock();
   return foundEntry != NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkCallbackRemove --
 *
 *      Remove a callback.
 *
 * Results:
 *      TRUE if entry found and removed, FALSE otherwise
 *
 * Side effects:
 *      A callback may be modified instead of completely removed.
 *
 *----------------------------------------------------------------------
 */

static Bool
PollGtkCallbackRemove(PollClassSet classSet,   // IN
                      int flags,               // IN
                      PollerFunction f,        // IN
                      void *clientData,        // IN
                      PollEventType type)      // IN
{
   void *foundClientData;

   return PollGtkCallbackRemoveInt(classSet, flags, f, clientData, FALSE, type,
                                   &foundClientData);
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkCallbackRemoveOneByCB --
 *
 *      Remove a callback.
 *
 * Results:
 *      TRUE if entry found and removed (*clientData updated), FALSE otherwise
 *
 * Side effects:
 *      A callback may be modified instead of completely removed.
 *
 *----------------------------------------------------------------------
 */

static Bool
PollGtkCallbackRemoveOneByCB(PollClassSet classSet,   // IN
                             int flags,               // IN
                             PollerFunction f,        // IN
                             PollEventType type,      // IN
                             void **clientData)       // OUT
{
   return PollGtkCallbackRemoveInt(classSet, flags, f, NULL, TRUE, type, clientData);
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkRemoveOneCallback --
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If entry was active, it is removed from main loop.
 *
 *----------------------------------------------------------------------
 */

static void
PollGtkRemoveOneCallback(gpointer data) // IN
{
   PollGtkEntry *eventEntry = data;

   switch(eventEntry->type) {
   case POLL_REALTIME:
   case POLL_MAIN_LOOP:
      g_source_remove(eventEntry->gtkInputId);
      break;
   case POLL_DEVICE:
      g_source_remove(eventEntry->gtkInputId);
      g_io_channel_unref(eventEntry->channel);
      eventEntry->channel = NULL;
      break;
   case POLL_VIRTUALREALTIME:
   case POLL_VTIME:
   default:
      NOT_IMPLEMENTED();
   }

   g_free(eventEntry);
}


/*
 *----------------------------------------------------------------------
 *
 * PollGtkCallback --
 *
 *      For the POLL_REALTIME or POLL_DEVICE queues, entries can be
 *      inserted for good, to fire on a periodic basis (by setting the
 *      POLL_FLAG_PERIODIC flag).
 *
 *      Otherwise, the callback fires only once.
 *
 *      For periodic POLL_REALTIME callbacks, "info" is the time in
 *      microseconds between execution of the callback.  For
 *      POLL_DEVICE callbacks, info is a file descriptor.
 *
 *----------------------------------------------------------------------
 */

static VMwareStatus
PollGtkCallback(PollClassSet classSet,   // IN
                int flags,               // IN
                PollerFunction f,        // IN
                void *clientData,        // IN
                PollEventType type,      // IN
                PollDevHandle info,      // IN
                MXUserRecLock *lock)     // IN
{
   VMwareStatus result;
   Poll *poll = pollState;
   PollGtkEntry *newEntry;

   ASSERT(f);

   newEntry = g_new0(PollGtkEntry, 1);
   newEntry->type = type;
   if (flags & POLL_FLAG_WRITE) {
      newEntry->write.flags = flags;
      newEntry->write.cb = f;
      newEntry->write.clientData = clientData;
      newEntry->write.cbLock = lock;
      newEntry->write.classSet = classSet;
      LOG_ENTRY(2, " is being added\n", newEntry, TRUE);
   } else {
      newEntry->read.flags = flags;
      newEntry->read.cb = f;
      newEntry->read.clientData = clientData;
      newEntry->read.cbLock = lock;
      newEntry->read.classSet = classSet;
      LOG_ENTRY(2, " is being added\n", newEntry, FALSE);
   }

   PollGtkLock();

   if (type == POLL_DEVICE) {
      PollGtkEntry *foundEntry;

      foundEntry = g_hash_table_lookup(poll->deviceTable,
                                       (gpointer)(intptr_t)info);
      if (foundEntry) {
         /*
          * We are going to merge old entry with new.  Verify that we really
          * found entry we were looking for.
          */
         ASSERT(foundEntry->type == type);
         ASSERT(foundEntry->event == info);

         /*
          * Now verify that found entry does not wait for direction we are
          * registering.
          */
         if (flags & POLL_FLAG_WRITE) {
            ASSERT(foundEntry->write.flags == 0);
            ASSERT(foundEntry->write.cb == NULL);
            ASSERT(foundEntry->read.cb != NULL);
            newEntry->read = foundEntry->read;
            LOG_ENTRY(2, " will merge with new entry\n", foundEntry, FALSE);
         } else {
            ASSERT(foundEntry->read.flags == 0);
            ASSERT(foundEntry->read.cb == NULL);
            ASSERT(foundEntry->write.cb != NULL);
            newEntry->write = foundEntry->write;
            LOG_ENTRY(2, " will merge with new entry\n", foundEntry, TRUE);
         }

         /*
          * Either both callbacks must be for socket, or for non-socket.
          * Mixing them is not supported at this moment.
          */
         ASSERT(((newEntry->read.flags ^ newEntry->write.flags) & POLL_FLAG_SOCKET)
                == 0);
         g_hash_table_remove(poll->deviceTable, (gpointer)(intptr_t)info);
      } else if (vmx86_debug) {
         /*
          * We did not find entry by fd.  Try looking it up by flags/f/cs/cd.
          * If we can find it, then user tried to insert same flags/f/cs/cd for
          * two file descriptors.  Which is not allowed.
          */
         PollGtkFindEntryData searchEntry;

         searchEntry.flags = flags;
         searchEntry.classSet = classSet;
         searchEntry.cb = f;
         searchEntry.clientData = clientData;
         searchEntry.type = POLL_DEVICE;
         searchEntry.matchAnyClientData = FALSE;

         if (flags & POLL_FLAG_WRITE) {
            foundEntry = g_hash_table_find(poll->deviceTable,
                                           PollGtkFindWritePredicate,
                                           &searchEntry);
         } else {
            foundEntry = g_hash_table_find(poll->deviceTable,
                                           PollGtkFindReadPredicate,
                                           &searchEntry);
         }
         ASSERT(!foundEntry);
      }
   }

   ASSERT(poll != NULL);

   /*
    * Every callback must be in POLL_CLASS_MAIN (plus possibly others)
    */
   ASSERT(PollClassSet_IsMember(classSet, POLL_CLASS_MAIN) != 0);

   ASSERT(type >= 0 && type < POLL_NUM_QUEUES);
   switch(type) {
   case POLL_MAIN_LOOP:
      ASSERT(info == 0);
      /* Fall-through */
   case POLL_REALTIME:
      info = info / 1000;
      ASSERT(info == (uint32)info);
      ASSERT(info >= 0);

      newEntry->event = info;

      /*
       * info is the delay in microseconds, but we need to pass in
       * a delay in milliseconds.
       */
      newEntry->gtkInputId = g_timeout_add(info,
                                           PollGtkBasicCallback,
                                           newEntry);
      g_hash_table_insert(poll->timerTable, (gpointer)(intptr_t)newEntry->gtkInputId,
                          newEntry);
      break;

   case POLL_DEVICE:
      /*
       * info is a file descriptor/socket/handle
       */
      newEntry->event = info;

      PollGtkDeviceCallback(newEntry);
      break;

   case POLL_VIRTUALREALTIME:
   case POLL_VTIME:
   default:
      NOT_IMPLEMENTED();
   }

   result = VMWARE_STATUS_SUCCESS;

   PollGtkUnlock();

   return result;
} // Poll_Callback


/*
 *-----------------------------------------------------------------------------
 *
 * PollGtkEventCallback --
 * PolLGtkEventCallbackWork --
 *
 *       This is the basic callback marshaller. It is invoked directly by gtk
 *       in the case of event callbacks and indirectly through a wrapper for
 *       timer callbacks. It calls the real callback and either cleans up
 *       the event or (if it's PERIODIC) leaves it registered to fire again.
 *
 * Results:
 *       TRUE if the event source should remain registered. FALSE otherwise.
 *
 * Side effects:
 *       Depends on the invoked callback
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
PollGtkEventCallback(GIOChannel *source,     // IN: GIOChannel associated with
                                             //     the file descriptor
                     GIOCondition condition, // IN: State(s) of the file
                                             //     descriptor that triggered
                                             //     the GTK callback
                     gpointer data)          // IN: PollGtkEntry *
{
   Bool fired;

   return PollGtkEventCallbackWork(source, condition, data, FALSE, &fired);
}


static gboolean
PollGtkEventCallbackWork(GIOChannel *source,     // IN:
                         GIOCondition condition, // IN:
                         gpointer data,          // IN:
                         Bool hasPollLock,       // IN:
                         Bool *firedAll)         // OUT:
{
   PollGtkEntry *eventEntry;
   PollerFunction cbFunc;
   void *clientData;
   MXUserRecLock *cbLock;
   Bool needReadAndWrite = FALSE;
   Bool fireWriteCallback;
   PollDevHandle fd = -1;
   gboolean ret;
   GIOChannel *channel;

   *firedAll = FALSE;

   if (!hasPollLock) {
      PollGtkLock();
   }

   if (g_source_is_destroyed(g_main_current_source())) {
      ret = FALSE;
      goto exitHasLock;
   }

   eventEntry = data;
   ASSERT(eventEntry);

   /*
    * Cache the bits we need to fire the callback in case the
    * callback is discarded for being non-periodic.
    */
   channel = eventEntry->channel;

   if (eventEntry->read.cb &&
       (condition & (G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL))) {
      cbFunc = eventEntry->read.cb;
      clientData = eventEntry->read.clientData;
      cbLock = eventEntry->read.cbLock;
      ret = (eventEntry->read.flags & POLL_FLAG_PERIODIC) != 0;
      fireWriteCallback = FALSE;
      fd = eventEntry->event;
      if (eventEntry->write.cb && (condition & G_IO_OUT)) {
         needReadAndWrite = TRUE;
      }
   } else if (eventEntry->write.cb &&
              (condition & (G_IO_OUT | G_IO_ERR | G_IO_HUP | G_IO_NVAL))) {
      cbFunc = eventEntry->write.cb;
      clientData = eventEntry->write.clientData;
      cbLock = eventEntry->write.cbLock;
      ret = (eventEntry->write.flags & POLL_FLAG_PERIODIC) != 0;
      fireWriteCallback = TRUE;
      fd = eventEntry->event;
   } else {
      ASSERT(FALSE);
      ret = TRUE;
      goto exitHasLock;
   }

   if (cbLock && !MXUser_TryAcquireRecLock(cbLock)) {
      /*
       * We cannot fire at this time.  For device callbacks, on Posix platforms
       * we should get called again at the next dispatch so we do nothing; on
       * Windows platforms we cannot rely on that so we have to remember the
       * signaled event and retry in the next loop iteration.  For non-zero
       * delay real-time callbacks, we schedule a 0-delay timer callback to
       * attempt firing again.  Main-loop callbacks are already 0-delay
       * callbacks, so we only need to return TRUE such that the source is not
       * removed.
       * TODO: Detect busy looping and apply backoff.
       */

      LOG_ENTRY(3, " did not fire\n", eventEntry, fireWriteCallback);
      if (fireWriteCallback) {
         eventEntry->write.timesNotFired++;
      } else {
         eventEntry->read.timesNotFired++;
      }

      if (eventEntry->type == POLL_DEVICE) {
#ifdef _WIN32
         PollGtkAddToSignaledList(eventEntry);
#endif
      } else {
         if (eventEntry->type == POLL_REALTIME && eventEntry->event != 0 &&
             eventEntry->read.timesNotFired == 1) {
            /* Re-purpose the event for the retry (as a 0-delay timer). */
            g_source_remove(eventEntry->gtkInputId);
            if (!g_hash_table_steal(pollState->timerTable,
                                    (gpointer)(intptr_t)eventEntry->gtkInputId)) {
               LOG_ENTRY(0, " not found\n", eventEntry, FALSE);
               ASSERT(FALSE);
            }
            eventEntry->gtkInputId = g_timeout_add(0, PollGtkBasicCallback,
                                                   eventEntry);
            g_hash_table_insert(pollState->timerTable,
                                (gpointer)(intptr_t)eventEntry->gtkInputId,
                                eventEntry);
            LOG_ENTRY(1, " rescheduled for retry\n", eventEntry, FALSE);
            ret = FALSE;
         } else {
            /* The event is already a 0-delay timer. */
            LOG_ENTRY(2, " will retry firing\n", eventEntry, FALSE);
            ret = TRUE;
         }
         ASSERT(!needReadAndWrite);
         goto exitHasLock;
      }
   } else {
      /*
       * Fire the callback.
       *
       * The callback must fire after unregistering non-periodic callbacks
       * in case the callback function re-registers itself.  If the callback
       * explicitly calls Poll_CallbackRemove, it is harmless when the callback
       * is already gone.
       */

      LOG_ENTRY(3, " about to fire\n", eventEntry, fireWriteCallback);
      *firedAll = TRUE;

      if (!ret) {
         PollGtkCallbackRemoveEntry(eventEntry, fireWriteCallback);
      } else if (fireWriteCallback) {
         eventEntry->write.timesNotFired = 0;
      } else if (!fireWriteCallback && eventEntry->read.timesNotFired > 0) {
         eventEntry->read.timesNotFired = 0;
         if (eventEntry->type == POLL_REALTIME && eventEntry->event != 0) {
            /* We need to reschedule the callback with the original delay. */
            g_source_remove(eventEntry->gtkInputId);
            if (!g_hash_table_steal(pollState->timerTable,
                                    (gpointer)(intptr_t)eventEntry->gtkInputId)) {
               LOG_ENTRY(0, " not found\n", eventEntry, FALSE);
               ASSERT(FALSE);
            }
            eventEntry->gtkInputId = g_timeout_add((guint)eventEntry->event,
                                                   PollGtkBasicCallback,
                                                   eventEntry);
            g_hash_table_insert(pollState->timerTable,
                                (gpointer)(intptr_t)eventEntry->gtkInputId,
                                eventEntry);
            LOG_ENTRY(1, " rescheduled with original delay\n",
                      eventEntry, FALSE);
         }
      }

      PollGtkUnlock();
      ((PollerFunctionGtk)cbFunc)(clientData, channel);
      if (cbLock) {
         MXUser_ReleaseRecLock(cbLock);
      }
#ifdef _WIN32
      PollGtkLock();
      eventEntry = g_hash_table_lookup(pollState->deviceTable,
                                       (gpointer)(intptr_t)fd);
      if (fireWriteCallback && eventEntry && eventEntry->write.cb &&
          (eventEntry->write.flags & POLL_FLAG_SOCKET)) {
         PollGtkWritableSocketCheck(eventEntry);
      } else if (!fireWriteCallback && eventEntry && eventEntry->read.cb &&
                 (eventEntry->read.flags & POLL_FLAG_SOCKET)) {
         PollGtkReadableSocketCheck(eventEntry);
      }
      if (fireWriteCallback || !needReadAndWrite) {
         goto exitHasLock;
      }
#else  /* ifdef _WIN32 */
      if (needReadAndWrite) {
         PollGtkLock();
      } else {
         goto exit;
      }
#endif
   }

   /*
    * We must fire both read & write callbacks.  Read callback already fired,
    * and could remove write callback.  So lookup entry from file descriptor.
    */
   if (needReadAndWrite) {
      PollGtkEntry *foundEntry;

      cbFunc = NULL;
      cbLock = NULL;
      ASSERT(fd != -1);
      foundEntry = g_hash_table_lookup(pollState->deviceTable,
                                       (gpointer)(intptr_t)fd);
      if (foundEntry && (cbFunc = foundEntry->write.cb) != NULL) {
         cbLock = foundEntry->write.cbLock;
         clientData = foundEntry->write.clientData;
         if (!cbLock || MXUser_TryAcquireRecLock(cbLock)) {
            LOG_ENTRY(3, " about to fire\n", foundEntry, TRUE);
            if (!(foundEntry->write.flags & POLL_FLAG_PERIODIC)) {
               PollGtkCallbackRemoveEntry(foundEntry, TRUE);
               ret = FALSE;
            } else {
               foundEntry->write.timesNotFired = 0;
            }
            PollGtkUnlock();
            cbFunc(clientData);
            if (cbLock) {
               MXUser_ReleaseRecLock(cbLock);
            }
#ifdef _WIN32
            PollGtkLock();
            foundEntry = g_hash_table_lookup(pollState->deviceTable,
                                             (gpointer)(intptr_t)fd);
            if (foundEntry && foundEntry->write.cb &&
                (foundEntry->write.flags & POLL_FLAG_SOCKET)) {
               PollGtkWritableSocketCheck(foundEntry);
            }
            goto exitHasLock;
#else
            goto exit;
#endif
         } else {
            LOG_ENTRY(3, " did not fire\n", foundEntry, TRUE);
            foundEntry->write.timesNotFired++;
            *firedAll = FALSE;
#ifdef _WIN32
            PollGtkAddToSignaledList(foundEntry);
#endif
         }
      }
   }

exitHasLock:
   if (!hasPollLock) {
      PollGtkUnlock();
   }
   return ret;

#ifndef _WIN32
exit:
   if (hasPollLock) {
      PollGtkLock();
   }
   return ret;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollGtkBasicCallback --
 *
 *       This is called by Gtk when a timeout expires. It is simply
 *       an adapter that immediately calls PollGtkEventCallback.
 *
 * Results:
 *       TRUE if the event source should remain registered. FALSE otherwise.
 *
 * Side effects:
 *       Depends on the invoked callback
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
PollGtkBasicCallback(gpointer data) // IN: The eventEntry
{
   return PollGtkEventCallback(NULL, G_IO_IN, data);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Poll_InitGtk --
 *
 *      Public init function for this Poll implementation. Poll loop will be
 *      up and running after this is called.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
Poll_InitGtk(void)
{
   static const PollImpl gtkImpl =
   {
      PollGtkInit,
      PollGtkExit,
      PollGtkLoopTimeout,
      PollGtkCallback,
      PollGtkCallbackRemove,
      PollGtkCallbackRemoveOneByCB,
      PollLockingAlwaysEnabled,
   };

   if (g_once_init_enter(&inited)) {
      gsize didInit = 1;
      Poll_InitWithImpl(&gtkImpl);
      g_once_init_leave(&inited, didInit);
   }
}
