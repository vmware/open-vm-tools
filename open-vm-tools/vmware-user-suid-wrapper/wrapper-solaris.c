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
 *      Platform dependent code for the VMware User Agent setuid wrapper.
 */

#include <sys/types.h>
#include <sys/systeminfo.h>

#include <errno.h>
#include <strings.h>
#include <unistd.h>

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
 *      Mount the VMBlock file system.
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
BuildExecPath(char *execPath,           // OUT: Path to executable for isaexec()
              size_t execPathSize)      // IN : size of execPath buffer
{
   /*
    * The locations database is the only path that's fixed, and it contains the
    * paths to all the other paths selected during Tools configuration.  The
    * locations database file is only writable by root, so we can trust it.
    */
   if (!QueryLocationsDB(LOCATIONS_PATH, QUERY_LIBDIR, execPath, execPathSize)) {
      Error("could not obtain LIBDIR\n");
      return FALSE;
   }

   /*
    * The wrapper script now emulates the work done by the isaexec command hence
    * we will simply call execve(2) below and allow the wrapper to do the rest.
    */
   if (strlcat(execPath,
               "/bin/vmware-user-wrapper", execPathSize) >= execPathSize) {
      Error("could not construct program filename\n");
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
 *      Simple platform-dependent isaexec() wrapper.
 *
 * Results:
 *      False.
 *
 * Side effects:
 *      Ideally, this function should not return.
 *
 *----------------------------------------------------------------------------
 */

Bool
CompatExec(const char *path, char * const argv[], char * const envp[])
{
   execve(path, argv, envp);
   return FALSE;
}
