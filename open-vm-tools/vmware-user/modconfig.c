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
 * modconfig.c --
 *
 *      Handles interaction with the modconfig gui for vmware-user.
 */

#include "vmwareuserInt.h"

#ifdef USE_NOTIFY_DLOPEN

#include "installerdb.h"
#include "file.h"
#include "installerdb.h"
#include "modconf.h"
#include "str.h"


/*
 * Local functions
 */
static GtkWidget *GetMenu(void);
static void LaunchModconfig(void);
static gboolean ActivateCallback(GtkWidget *widget, Notifier *n);
static void MenuItemCallback(GObject *self, void *data);


/*
 *----------------------------------------------------------------------------
 *
 * LaunchModconfig --
 *
 *      Asynchronously spawn the modconfig process to rebuild kernel modules.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The modoconfig program is launched, and modules are recompiled.
 *
 *----------------------------------------------------------------------------
 */

static void
LaunchModconfig(void)
{
   char *exePath;
   char *command;
   gchar *quotedExePath;

   exePath = Str_Asprintf(NULL, "%s/sbin/vmware-modconfig-wrapper", vmLibDir);
   quotedExePath = g_shell_quote(exePath);
   command = Str_Asprintf(NULL, "%s --icon=\"vmware-modconfig\" "
                          "--appname=\"VMware Tools\"", quotedExePath);

   g_spawn_command_line_async(command, NULL);

   free(command);
   g_free(quotedExePath);
   free(exePath);
}


/*
 *----------------------------------------------------------------------------
 *
 * ActivateCallback --
 *
 *      The callback invoked when the status icon is left-clicked.
 *
 * Results:
 *      TRUE if the signal is handled, FALSE otherwise.
 *
 * Side effects:
 *      Launches modconfig.
 *
 *----------------------------------------------------------------------------
 */

static gboolean
ActivateCallback(GtkWidget *widget,     // IN
                 Notifier *n)           // IN
{
   LaunchModconfig();
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * MenuItemCallback --
 *
 *      The callback invoked when any item on the popup context menu is clicked.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Launches modconfig.
 *
 *----------------------------------------------------------------------------
 */

static void
MenuItemCallback(GObject *self,              // IN
                 void *data)                 // IN
{
   LaunchModconfig();
}


/*
 *----------------------------------------------------------------------------
 *
 * GetMenu --
 *
 *      Create the context menu for the status icon.
 *
 * Results:
 *      Returns a pointer to the created menu.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static GtkWidget *
GetMenu(void)
{
   GtkWidget *menu = gtk_menu_new();
   GtkWidget *menuItem = gtk_menu_item_new_with_label("Update Modules");
   g_signal_connect(G_OBJECT(menuItem), "activate", G_CALLBACK(MenuItemCallback),
                    NULL);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);

   return menu;
}


/*
 *----------------------------------------------------------------------------
 *
 * Modules_Init --
 *
 *      Check for kernel modules and display a notification if any are
 *      found missing.
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side effects:
 *      A notification is displayed, informing the user of the missing
 *      modules.
 *
 *----------------------------------------------------------------------------
 */

Bool
Modules_Init(void)
{
   const char *libdir;
   char *moduleListPath;
   GList *modules, *modulesNotInstalled;

   if (!InstallerDB_Init("/etc/vmware-tools", TRUE)) {
      return FALSE;
   }

   /* 
    * Only do module out-of-dateness checking if we weren't installed as
    * a DSP.
    */
   if (InstallerDB_IsDSPInstall()) {
      InstallerDB_DeInit();
      return FALSE;
   }

   if (!ModConf_Init()) {
      return FALSE;
   }

   libdir = InstallerDB_GetLibDir();
   moduleListPath = g_build_filename(libdir, "modules/modules.xml", NULL);
   modules = ModConf_GetModulesList(moduleListPath);
   modulesNotInstalled = ModConf_GetModulesNotInstalled(modules);

   if (modulesNotInstalled != NULL) {
      Notify_Notify(30, "Kernel modules out-of-date",
                    "It appears your kernel modules are not longer "
                    "compatible with the running kernel.  Please "
                    "click on the icon to recompile them.",
                    GetMenu(), ActivateCallback);
   }

   g_list_free(modulesNotInstalled);
   ModConf_FreeModulesList(modules);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * Modules_Cleanup --
 *
 *      Cleanup the modconf subsystem.
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side effects:
 *      The modconf subsystem is closed.
 *
 *----------------------------------------------------------------------------
 */

void
Modules_Cleanup(void)
{
   ModConf_DeInit();
   InstallerDB_DeInit();
}

#endif /* USE_NOTIFY_DLOPEN */
