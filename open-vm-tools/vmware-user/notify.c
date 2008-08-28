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
 *      Handles the system tray out-of-dateness notifications for vmware-user.
 */


#ifdef USE_NOTIFICATION
#include "file.h"
#include "installerdb.h"
#include "modconf.h"
#endif

#include "vmware.h"
#include "vmwareuserInt.h"


/*
 *----------------------------------------------------------------------------
 *
 * Notify_Init --
 *
 *    Initializes the notification system, including checking for out-of-date
 *    modules.
 *
 * Results:
 *    TRUE on success, FALSE otherwise
 *
 * Side effects:
 *    Notification system is initialized.
 *
 *----------------------------------------------------------------------------
 */

Bool
Notify_Init(void)
{
#ifdef USE_NOTIFICATION
   if (!InstallerDB_Init("/etc/vmware-tools", TRUE)) {
      return FALSE;
   }

   /* 
    * Only do module out-of-dateness checking if we weren't installed as
    * a DSP.
    */
   if (InstallerDB_IsDSPInstall()) {
      return FALSE;
   }

   const char *libdir = InstallerDB_GetLibDir();
   char *moduleListPath = g_build_filename(libdir, "modules/modules.xml", NULL);
   GList *modules = ModConf_GetModulesList(moduleListPath);

   ModConf_FreeModulesList(modules);

   return TRUE;

#else
   return FALSE;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * Notify_Cleanup --
 *
 *    Clean up the notification system.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Notification system is deinitialized.
 *
 *----------------------------------------------------------------------------
 */

void
Notify_Cleanup(void)
{
#ifdef USE_NOTIFICATION
   InstallerDB_DeInit();
#endif
}
