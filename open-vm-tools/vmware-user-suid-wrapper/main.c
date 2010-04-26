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
 * main.c --
 *
 *      This program is run as root to prepare the system for vmware-user.  It
 *      unmounts the vmblock file system, unloads the vmblock module, then
 *      reloads the module, mounts the file system, and opens a file descriptor
 *      that vmware-user can use to add and remove blocks.  This must all
 *      happen as root since we cannot allow any random process to add and
 *      remove blocks in the blocking file system.
 */

#if !defined(sun) && !defined(__FreeBSD__) && !defined(linux)
# error This program is not supported on your platform.
#endif

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(sun)
# include <sys/systeminfo.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#include "vmware.h"
#include "vmblock.h"
#include "vmsignal.h"
#include "wrapper.h"

#include "wrapper_version.h"
#include "embed_version.h"
VM_EMBED_VERSION(WRAPPER_VERSION_STRING);


/*
 * Local functions (prototypes)
 */

#ifdef TOGGLE_VMBLOCK
static void ToggleVMBlock(void);
static Bool StartVMBlock(void);
static Bool StopVMBlock(void);
static Bool MakeDirectory(const char *path, mode_t mode, uid_t uid, gid_t gid);
static Bool ChmodChownDirectory(const char *path,
                                mode_t mode, uid_t uid, gid_t gid);
#endif
static void MaskSignals(void);
static Bool StartVMwareUser(char *const envp[]);


/*
 *----------------------------------------------------------------------------
 *
 * main --
 *
 *    On platforms where this wrapper manages the vmblock module:
 *       Unmounts vmblock and unloads the module, then reloads the module,
 *       and remounts the file system, then starts vmware-user as described
 *       below.
 *
 *       This program is the only point at which vmblock is stopped or
 *       started.  This means we must always unload the module to ensure that
 *       we are using the newest installed version (since an upgrade could
 *       have occurred since the last time this program ran).
 *
 *    On all platforms:
 *       Acquires the vmblock control file descriptor, drops privileges, then
 *       starts vmware-user.
 *
 * Results:
 *    EXIT_SUCCESS on success and EXIT_FAILURE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
main(int argc,
     char *argv[],
     char *envp[])
{
   MaskSignals();

#ifdef TOGGLE_VMBLOCK
   ToggleVMBlock();
#endif

   if (!StartVMwareUser(envp)) {
      Error("failed to start vmware-user\n");
      exit(EXIT_FAILURE);
   }

   exit(EXIT_SUCCESS);
}


/*
 * Local functions (definitions)
 */


#ifdef TOGGLE_VMBLOCK
/*
 *----------------------------------------------------------------------------
 *
 * ToggleVMBlock --
 *
 *    Unmounts vmblock and unloads the module, then reloads the module and
 *    remounts the file system.
 *
 * Results:
 *    Vmblock file system "service" is reloaded.
 *
 * Side effects:
 *    May exit with EXIT_FAILURE if the vmblock service cannot be stopped.
 *
 *----------------------------------------------------------------------------
 */

static void
ToggleVMBlock(void)
{
   if (!StopVMBlock()) {
      Error("failed to stop vmblock\n");
      exit(EXIT_FAILURE);
   }

   if (!StartVMBlock()) {
      /*
       * There is more to vmware-user than VMBlock, so in case of error,
       * only make a little noise.  Continue to launch vmware-user.
       */
      Error("failed to start vmblock\n");
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * StopVMBlock --
 *
 *    Unmounts the vmblock file system and unload the vmblock module.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
StopVMBlock(void)
{
   Bool ret;
   int id;

   /*
    * Default to success whether or not module loaded.  Can fail only if
    * unload fails.
    */
   ret = TRUE;

   /*
    * The file system may not be mounted and that's okay.  If it is mounted and
    * this fails, the unloading of the module will fail later.
    */
   UnmountVMBlock(VMBLOCK_MOUNT_POINT);

   id = GetModuleId(MODULE_NAME);
   if (id >= 0) {
      /* The module is loaded. */
      if (!UnloadModule(id)) {
         ret = FALSE;
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * StartVMBlock --
 *
 *    Loads the vmblock module and mounts its file system.
 *
 * Results:
 *    TRUE on success and FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
StartVMBlock(void)
{
   uid_t euid;
   gid_t egid;

   euid = geteuid();
   egid = getegid();

   if (!MakeDirectory(TMP_DIR, TMP_DIR_MODE, euid, egid)) {
      Error("failed to create %s\n", TMP_DIR);
      return FALSE;
   }

   if (!MakeDirectory(VMBLOCK_MOUNT_POINT, MOUNT_POINT_MODE, euid, egid)) {
      Error("failed to create %s\n", VMBLOCK_MOUNT_POINT);
      return FALSE;
   }

   if (!LoadVMBlock()) {
      return FALSE;
   }

   if (!MountVMBlock()) {
      /* This will unload the module and ignore the unmount failure. */
      StopVMBlock();
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * MakeDirectory --
 *
 *    Creates a directory with the provided mode, uid, and gid.  If the
 *    provided path already exists, this will ensure that it has the correct
 *    mode, uid, and gid, or else it will fail.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
MakeDirectory(const char *path, // IN: path to create
              mode_t mode,      // IN: mode of new directory
              uid_t uid,        // IN: owner of new directory
              gid_t gid)        // IN: group of new directory
{
   if (mkdir(path, mode) == 0) {
      /*
       * We still need to chmod(2) the directory since mkdir(2) takes the umask
       * into account.
       */
      if (!ChmodChownDirectory(path, mode, uid, gid)) {
         return FALSE;
      }
      return TRUE;
   }

   /*
    * If we couldn't create the directory because the path already exists, we
    * need to make sure it's a directory and that it has the correct
    * permissions and owner.  For any other failure we fail.
    */
   if (errno != EEXIST || !ChmodChownDirectory(path, mode, uid, gid)) {
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * ChmodChownDirectory --
 *
 *    Atomically ensures the provided path is a directory and changes its mode,
 *    uid, and gid to the provided values.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
ChmodChownDirectory(const char *path,  // IN
                    mode_t mode,       // IN
                    uid_t uid,         // IN
                    gid_t gid)         // IN
{
   int fd;
   struct stat stat;
   int ret = FALSE;

   fd = open(path, O_RDONLY);
   if (fd < 0) {
      return FALSE;
   }

   if (fstat(fd, &stat) != 0) {
      goto out;
   }

   if (!S_ISDIR(stat.st_mode)) {
      goto out;
   }

   if ((stat.st_uid != uid || stat.st_gid != gid) &&
       fchown(fd, uid, gid) != 0) {
      goto out;
   }

   if (stat.st_mode != mode && fchmod(fd, mode) != 0) {
      goto out;
   }

   ret = TRUE;

out:
   close(fd);
   return ret;
}
#endif  // ifdef TOGGLE_VMBLOCK


/*
 *-----------------------------------------------------------------------------
 *
 * MaskSignals --
 *
 *      Sets SIG_IGN as the handler for SIGUSR1 and SIGUSR2 which may arrive
 *      prematurely from our services script.  See bug 542135.
 *
 * Results:
 *      Returns if applicable signals are blocked.
 *      Exits with EXIT_FAILURE otherwise.
 *
 * Side effects:
 *      SIG_IGN disposition persists across execve().  These signals will
 *      remain masked until vmware-user defines its own handlers.
 *
 *-----------------------------------------------------------------------------
 */

static void
MaskSignals(void)
{
   int const signals[] = {
      SIGUSR1,
      SIGUSR2
   };
   struct sigaction olds[ARRAYSIZE(signals)];

   if (Signal_SetGroupHandler(signals, olds, ARRAYSIZE(signals),
                              SIG_IGN) == 0) {
      /* Signal_SetGroupHandler will write error message to stderr. */
      exit(EXIT_FAILURE);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * StartVMwareUser --
 *
 *    Obtains the library directory from the Tools locations database, then
 *    opens a file descriptor (while still root) to add and remove blocks,
 *    drops privilege to the real uid of this process, and finally starts
 *    vmware-user.
 *
 * Results:
 *    Parent: TRUE on success, FALSE on failure.
 *    Child: FALSE on failure, no return on success.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
StartVMwareUser(char *const envp[])
{
   pid_t pid;
   uid_t uid;
   gid_t gid;
   int fd = -1;
   int ret;
   char path[MAXPATHLEN];
   char *argv[4];

   if (!BuildExecPath(path, sizeof path)) {
      return FALSE;
   }

   /*
    * Now create a child process, obtain a file descriptor as root, downgrade
    * privilege, and run vmware-user.
    */
   pid = fork();
   if (pid == -1) {
      Error("fork failed: %s\n", strerror(errno));
      return FALSE;
   } else if (pid != 0) {
      /* Parent */
      return TRUE;
   }

   /* Child */

   /*
    * We know the file system is mounted and want to keep this suid
    * root wrapper as small as possible, so here we directly open(2) the
    * "device" instead of calling DnD_InitializeBlocking() and bringing along
    * a whole host of libs.
    */
   fd = open(VMBLOCK_FUSE_DEVICE, VMBLOCK_FUSE_DEVICE_MODE);
   if (fd < 0) {
      fd = open(VMBLOCK_DEVICE, VMBLOCK_DEVICE_MODE);
   }

   uid = getuid();
   gid = getgid();

   if ((setreuid(uid, uid) != 0) ||
       (setregid(gid, gid) != 0)) {
      Error("could not drop privileges: %s\n", strerror(errno));
      if (fd != -1) {
         close(fd);
      }
      return FALSE;
   }

   /*
    * Since vmware-user provides features that don't depend on vmblock, we
    * invoke vmware-user even if we couldn't obtain a file descriptor or we
    * can't parse the descriptor to pass as an argument.  We set up the
    * argument vector accordingly.
    */
   argv[0] = path;

   if (fd < 0) {
      Error("could not open %s\n", VMBLOCK_DEVICE);
      argv[1] = NULL;
   } else {
      char fdStr[8];

      ret = snprintf(fdStr, sizeof fdStr, "%d", fd);
      if (ret == 0 || ret >= sizeof fdStr) {
         Error("could not parse file descriptor (%d)\n", fd);
         argv[1] = NULL;
      } else {
         argv[1] = "--blockFd";
         argv[2] = fdStr;
         argv[3] = NULL;
      }
   }

   CompatExec(path, argv, envp);

   /*
    * CompatExec, if successful, doesn't return.  I.e., we're here only
    * if CompatExec fails.
    */
   Error("could not execute %s: %s\n", path, strerror(errno));
   exit(EXIT_FAILURE);
}


#ifndef USES_LOCATIONS_DB
/*
 *-----------------------------------------------------------------------------
 *
 * BuildExecPath --
 *
 *	Writes the path to vmware-user to execPath.  This version, as opposed
 *	to the versions in $platform/wrapper.c, is only used when the locations
 *	database isn't used.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
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
   if (execPathSize < sizeof VMWARE_USER_PATH) {
      return FALSE;
   }
   strcpy(execPath, VMWARE_USER_PATH);
   return TRUE;
}
#endif // ifndef USES_LOCATIONS_DB
