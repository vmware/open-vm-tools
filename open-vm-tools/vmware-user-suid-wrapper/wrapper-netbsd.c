/*********************************************************
 * Copyright (C) 2007-2016 VMware, Inc. All rights reserved.
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
 * wrapper.c --
 *
 *      Platform specific code for the VMware User Agent setuid wrapper.
 */


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vmware.h"
#include "wrapper.h"


/*
 * Global functions
 */


#ifdef USES_LOCATIONS_DB
/*
 *-----------------------------------------------------------------------------
 *
 * BuildExecPath --
 *
 *      Determine & return path of vmware-user for use by execve(2).
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
BuildExecPath(char *execPath,           // OUT: Buffer to store executable's path
              size_t execPathSize)      // IN : size of execPath buffer
{
   char tmpPath[MAXPATHLEN];
   int execLen;

   /*
    * The locations database is the only path that's fixed, and it contains the
    * paths to all the other paths selected during Tools configuration.  The
    * locations database file is only writable by root, so we can trust it.
    */
   if (!QueryLocationsDB(LOCATIONS_PATH, QUERY_BINDIR, tmpPath, sizeof tmpPath)) {
      Error("could not obtain BINDIR\n");
      return FALSE;
   }

   if (strlcat(tmpPath,
               "/vmware-user-wrapper", sizeof tmpPath) >= sizeof tmpPath) {
      Error("could not construct program filename\n");
      return FALSE;
   }

   /*
    * From readlink(2), "The readlink() system call does not append a NUL
    * character to buf."  (NB:  This breaks if user ever replaces the symlink
    * with the target.)
    */
   if ((execLen = readlink(tmpPath, execPath, execPathSize - 1)) == -1) {
      Error("could not resolve symlink: %s\n", strerror(errno));
      return FALSE;
   }

   execPath[execLen] = '\0';

   /*
    * Now make sure that the target is actually part of our "trusted"
    * directory.  (Check that execPath has LIBDIR as a prefix and does
    * not contain "..".)
    */
   if (!QueryLocationsDB(LOCATIONS_PATH, QUERY_LIBDIR, tmpPath,
                         sizeof tmpPath)) {
      Error("could not obtain LIBDIR\n");
      return FALSE;
   }

   if ((strncmp(execPath, tmpPath, strlen(tmpPath)) != 0) ||
       (strstr(execPath, "..") != NULL)) {
      Error("vmware-user path untrusted\n");
      return FALSE;
   }

   return TRUE;
}
#endif // ifdef USES_LOCATIONS_DB


/*
 *----------------------------------------------------------------------------
 *
 * CompatExec --
 *
 *      Simple platform-dependent execve() wrapper.
 *
 * Results:
 *      False.
 *
 * Side effects:
 *      This function may not return.
 *
 *----------------------------------------------------------------------------
 */

Bool
CompatExec(const char *path, char * const argv[], char * const envp[])
{
   execve(path, argv, envp);
   return FALSE;
}
