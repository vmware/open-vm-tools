/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/modctl.h>
#include <sys/systeminfo.h>

#include <errno.h>
#include <strings.h>
#include <unistd.h>

#include "wrapper.h"


/*
 * Global functions
 */


/*
 *-----------------------------------------------------------------------------
 *
 * UnloadModule --
 *
 *      Lookup and, if loaded, unload the VMBlock kernel module.
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
UnloadModule(int id)    // IN: module id for modctl(2)
{
   if (modctl(MODUNLOAD, id) < 0) {
      Error("Error unloading VMBlock module: %s", strerror(errno));
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * LoadVMBlock --
 *
 *      Load the VMBlock kernel module.
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
LoadVMBlock(void)
{
   /*
    * modctl will load either the 32-bit or 64-bit module, as appropriate.
    */
   if (modctl(MODLOAD, 1, "drv/vmblock", NULL) < 0) {
      Error("failed to load vmblock\n");
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnmountVMBlock --
 *
 *      Unmount the VMBlock file system.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnmountVMBlock(const char *mountPoint)  // IN: VMBlock mount point
{
   if (umount(mountPoint) == -1) {
      return FALSE;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MountVMBlock --
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
MountVMBlock(void)
{
   if (mount(TMP_DIR, VMBLOCK_MOUNT_POINT, MS_DATA, VMBLOCK_FS_NAME) < 0) {
      Error("failed to mount vmblock file system: %s\n", strerror(errno));
      return FALSE;
   }

   return TRUE;
}


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
    * We will use isaexec(3C) below so we specify the base directory in our
    * tarball that contains the ISA directories with actual binaries.
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
 * GetModuleId --
 *
 *      Finds the id of the provided loaded module.
 *
 * Results:
 *      The id on success, a negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
GetModuleId(const char *name)   // IN: module name to search for
{
   struct modinfo modinfo;

   /*
    * Loop until either there are no more modules (modctl() fails) or we found
    * the module the caller wanted.
    */
   modinfo.mi_id = modinfo.mi_nextid = -1;
   modinfo.mi_info = MI_INFO_ALL;

   do {
      if (modctl(MODINFO, modinfo.mi_id, &modinfo) < 0) {
         return -1;
      }
   } while (strcmp(modinfo.mi_name, name) != 0);

   return modinfo.mi_id;
}


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
   isaexec(path, argv, envp);
   return FALSE;
}
