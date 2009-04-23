/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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
 * conf.c --
 *
 *    Manage the tools configuration file.
 *
 */

#ifndef VMX86_DEVEL

#endif


#include <stdlib.h>

#include "vm_assert.h"
#include "guestApp.h"
#include "util.h"
#include "str.h"
#include "conf.h"
#include "eventManager.h"
#include "debug.h"


/*
 *----------------------------------------------------------------------
 *
 * Conf_Load --
 *
 *      Set the conf dict's default values then attempt to load the
 *      conf file into memory.
 *
 * Results:
 *      
 *      The conf dict.
 *
 * Side effects:
 *
 *      None.
 *
 *----------------------------------------------------------------------
 */

GuestApp_Dict *
Conf_Load(void)
{
   GuestApp_Dict *confDict;
   char *path;
   char *confPath = GuestApp_GetConfPath();
   char *installPath = GuestApp_GetInstallPath();

   /* We really can't proceed without these paths. */
   ASSERT(confPath);
   ASSERT(installPath);
   if (confPath == NULL) {
      Panic("Could not get path to Tools configuration file.\n");
   }

   if (installPath == NULL) {
      Panic("Could not get path to Tools installation.\n");
   }

   path = Str_Asprintf(NULL, "%s%c%s", confPath, DIRSEPC, CONF_FILE);
   ASSERT_NOT_IMPLEMENTED(path);
   confDict = GuestApp_ConstructDict(path);
   // don't free path; it's used by the dict
   
   /* Set default conf values */
   path = Str_Asprintf(NULL, "%s%c%s", installPath, DIRSEPC,
                       CONFVAL_POWERONSCRIPT_DEFAULT);
   ASSERT_NOT_IMPLEMENTED(path);
   GuestApp_SetDictEntryDefault(confDict, CONFNAME_POWERONSCRIPT, path);
   free(path);

   path = Str_Asprintf(NULL, "%s%c%s", installPath, DIRSEPC, 
                       CONFVAL_POWEROFFSCRIPT_DEFAULT);
   ASSERT_NOT_IMPLEMENTED(path);
   GuestApp_SetDictEntryDefault(confDict, CONFNAME_POWEROFFSCRIPT, path);
   free(path);

   path = Str_Asprintf(NULL, "%s%c%s", installPath, DIRSEPC, 
                       CONFVAL_RESUMESCRIPT_DEFAULT);
   ASSERT_NOT_IMPLEMENTED(path);
   GuestApp_SetDictEntryDefault(confDict, CONFNAME_RESUMESCRIPT, path);
   free(path);

   path = Str_Asprintf(NULL, "%s%c%s", installPath, DIRSEPC, 
                       CONFVAL_SUSPENDSCRIPT_DEFAULT);
   ASSERT_NOT_IMPLEMENTED(path);
   GuestApp_SetDictEntryDefault(confDict, CONFNAME_SUSPENDSCRIPT, path);
   free(path);

   GuestApp_SetDictEntryDefault(confDict, CONFNAME_MAX_WIPERSIZE,
                                CONFVAL_MAX_WIPERSIZE_DEFAULT);

   /* Load the user-configured values from the conf file if it's there */
   GuestApp_LoadDict(confDict);

   free(installPath);
   free(confPath);

   return confDict;
}


/*
 *----------------------------------------------------------------------
 *
 * Conf_ReloadFile --
 *
 *      Reload the conf dict if the conf file has changed.
 *      Callers are expected to add this function to the event loop to 
 *      periodically read in configuration values.
 *
 * Results:
 *      
 *      TRUE is file was reloaded, FALSE otherwise.
 *
 * Side effects:
 *
 *	     None.
 *
 *----------------------------------------------------------------------
 */

Bool
Conf_ReloadFile(GuestApp_Dict **pConfDict) // IN/OUT
{ 
   ASSERT(pConfDict);
   ASSERT(*pConfDict);
   
   if (GuestApp_WasDictFileChanged(*pConfDict)) {
      Debug("Conf file out of date; reloading...\n");

      GuestApp_FreeDict(*pConfDict);
      *pConfDict = Conf_Load();

      return TRUE;
   }
   return FALSE;
}
