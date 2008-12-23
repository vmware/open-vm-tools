/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * notify.c --
 *
 *      Handles the system tray notifications for vmware-user.
 */

#include "vmware.h"
#include "vmwareuserInt.h"

#ifdef USE_NOTIFY

#include "conf.h"
#include "debug.h"
#include "str.h"

#if defined(USE_NOTIFY_SO)
#include <libnotify/notify.h>
#include <libnotify/notification.h>
#include <libnotify/NotifyNotification.h>
#elif defined(USE_NOTIFY_DLOPEN)
#include <dlfcn.h>
#include <stdlib.h>
#endif


/*
 * Local symbols
 */
const char *vmLibDir = NULL;

#ifdef USE_NOTIFY_DLOPEN
static gboolean (*notify_init)(const char *app_name) = NULL;
static void (*notify_uninit)(void) = NULL;
static NotifyNotification *(*notify_notification_new_with_status_icon)
          (const gchar *summary, const gchar *body,
           const gchar *icon, GtkStatusIcon *status_icon) = NULL;
static gboolean (*notify_notification_show)(NotifyNotification *notification,
                                            GError **error) = NULL;
void (*notify_notification_set_timeout)(NotifyNotification *notification,
                                        gint timeout) = NULL;

static Bool LoadLibNotify(void);
static Bool UnloadLibNotify(void);

static void *libNotifyHandle = NULL;
static Bool initialized = FALSE;


/*
 *----------------------------------------------------------------------------
 *
 * LoadLibNotify --
 *
 *      Dynamically load required symbols from libnotify.  We only do this
 *      when building inside the VMware tree; when shipping open-vm-tools,
 *      we can just let the linker do the work, since we have the libraries
 *      available at build-time.
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side effects:
 *      libnotify is loaded, and symbols populated.
 *
 *----------------------------------------------------------------------------
 */

static Bool
LoadLibNotify(void)
{
   int i;

   libNotifyHandle = dlopen("libnotify.so.1", RTLD_LAZY);

   if (!libNotifyHandle) {
      return FALSE;
   }

   /* 
    * The list of symbols we want to dynamically load from libnotify.  It
    * must be NULL-terminated.  To load additional symbols, be sure to
    * define them above, with file-level scope, and then add them to this
    * list.
    */
   {
      struct FuncEntry
      {
         void **funcPtr;
         const char *symName;
      } vtable[] = {
         { (void **) &notify_init, "notify_init" },
         { (void **) &notify_uninit, "notify_uninit" },
         { (void **) &notify_notification_show, "notify_notification_show" },
         { (void **) &notify_notification_new_with_status_icon,
           "notify_notification_new_with_status_icon" },
         { (void **) &notify_notification_set_timeout,
           "notify_notification_set_timeout" },
         { NULL, NULL }
      };

      /*
       * Load each of the above symbols from libnotify, checking to make sure
       * that they exist.
       */
      for (i = 0; vtable[i].funcPtr != NULL; i++) {
         *(vtable[i].funcPtr) = dlsym(libNotifyHandle, vtable[i].symName);
         if ( *(vtable[i].funcPtr) == NULL) {
            Debug("Could not find %s in libnotify\n", vtable[i].symName);
            UnloadLibNotify();
            return FALSE;
         }
      }
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnloadLibNotify --
 *
 *      Decrement the reference count for libnotify.  We only do this when
 *      building inside the VMware tree; when shipping open-vm-tools, we can
 *      just let the linker do the work, since we have the libraries
 *      available at build-time.
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side effects:
 *      libnotify is unloaded
 *
 *----------------------------------------------------------------------------
 */

static Bool
UnloadLibNotify(void)
{
   return dlclose(libNotifyHandle) == 0;
}
#endif  /* USE_NOTIFY_DLOPEN */


/*
 *----------------------------------------------------------------------------
 *
 * Notify_Init --
 *
 *      Initializes the notification system.
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side effects:
 *      Notification system is initialized.
 *
 *----------------------------------------------------------------------------
 */

Bool
Notify_Init(GuestApp_Dict *confDict)  // IN: Configuration dictionary
{
#ifdef USE_NOTIFY_DLOPEN
   if (LoadLibNotify() == FALSE) {
      return FALSE;
   }
#endif

   vmLibDir = GuestApp_GetDictEntry(confDict, CONFNAME_LIBDIR);
   initialized = notify_init("vmware-user");

   return initialized;
}


/*
 *----------------------------------------------------------------------------
 *
 * Notify_Cleanup --
 *
 *      Clean up the notification system.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Notification system is deinitialized.
 *
 *----------------------------------------------------------------------------
 */

void
Notify_Cleanup(void)
{
   initialized = FALSE;
   notify_uninit();

#ifdef USE_NOTIFY_DLOPEN
   UnloadLibNotify();
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * PopupCallback --
 *
 *      The callback invoked when the status icon is right-clicked.
 *
 * Results:
 *      TRUE if the signal is handled, FALSE otherwise.
 *
 * Side effects:
 *      Displays the popup menu for the icon.
 *
 *----------------------------------------------------------------------------
 */

static gboolean
PopupCallback(GtkStatusIcon *statusIcon,        // IN
              guint button,                     // IN
              guint activateTime,               // IN
              Notifier *n)                      // IN
{
   gtk_menu_set_screen(GTK_MENU(n->menu),
                       gtk_status_icon_get_screen(n->statusIcon));
   gtk_menu_popup(GTK_MENU(n->menu), NULL, NULL,
                  gtk_status_icon_position_menu, n->statusIcon, button,
                  activateTime);
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * Notify_Notify --
 *
 *      Create and display the notification icon with the given message.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      Notification is displayed.
 *
 *----------------------------------------------------------------------------
 */

Bool
Notify_Notify(int secs,                  // IN: Number of seconds to display
              const char *shortMsg,      // IN: Short summary message
              const char *longMsg,       // IN: Longer detailed message
              GtkWidget *menu,           // IN: Context menu
              gboolean (*activateCallback)(GtkWidget *, Notifier *))
                                         // IN: The left-click callback
{
   char *iconPath;
   Notifier *n;

   if (!initialized) {
      return FALSE;
   }

   n = g_new0(Notifier, 1);
   iconPath = Str_Asprintf(NULL, "%s/share/icons/vmware.png", vmLibDir);
   n->statusIcon = gtk_status_icon_new_from_file(iconPath);
   gtk_status_icon_set_tooltip(n->statusIcon, shortMsg);
   gtk_status_icon_set_visible(n->statusIcon, TRUE);
   free(iconPath);

   /*
    * Display the notification for secs seconds.
    */
   n->notification = notify_notification_new_with_status_icon(shortMsg, longMsg,
                                                              NULL, n->statusIcon);
   notify_notification_set_timeout(n->notification, secs * 1000);
   notify_notification_show(n->notification, NULL);

   /*
    * Connect the click and right-click signals.
    */
   g_signal_connect(G_OBJECT(n->statusIcon), "activate",
                    G_CALLBACK(activateCallback), n);
   g_signal_connect(G_OBJECT(n->statusIcon), "popup-menu",
                    G_CALLBACK(PopupCallback), n);

   n->menu = menu;
   gtk_widget_show_all(n->menu);

   return TRUE;
}

#endif
