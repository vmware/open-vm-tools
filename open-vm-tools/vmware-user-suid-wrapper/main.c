/*********************************************************
 * Copyright (C) 2007-2018 VMware, Inc. All rights reserved.
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
 *      This program is run as root to prepare the system for vmware-user.
 *      - It unmounts the vmblock file system, unloads the vmblock module, then
 *        reloads the module, mounts the file system, and opens a file descriptor
 *        that vmware-user can use to add and remove blocks.
 *        This must all happen as root since we cannot allow any random process
 *        to add and remove blocks in the blocking file system.
 *      - It opens the uinput device file and passes the file descriptor to
 *        vmware-user.
 */

#if !defined(sun) && !defined(__FreeBSD__) && !defined(__linux__)
# error This program is not supported on your platform.
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "vmware.h"
#include "vmblock.h"
#include "vmsignal.h"
#include "wrapper.h"

#include "wrapper_version.h"
#include "vm_version.h"
#include "embed_version.h"
VM_EMBED_VERSION(WRAPPER_VERSION_STRING);


/*
 * Local functions (prototypes)
 */

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

   if (!StartVMwareUser(envp)) {
      Error("failed to start vmware-user\n");
      exit(EXIT_FAILURE);
   }

   exit(EXIT_SUCCESS);
}


/*
 * Local functions (definitions)
 */


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
   int blockFd = -1;
   char blockFdStr[8];
   int uinputFd = -1;
   char uinputFdStr[8];
   int ret;
   char path[MAXPATHLEN];
   char *argv[8];
   size_t idx = 0;
   char *xdgSessionType;
   Bool useWayland = FALSE;

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

   xdgSessionType = getenv("XDG_SESSION_TYPE");
   if (   (xdgSessionType != NULL)
       && (strstr(xdgSessionType, "wayland") != NULL)) {
      useWayland = TRUE;
   }

   /*
    * We know the file system is mounted and want to keep this suid
    * root wrapper as small as possible, so here we directly open(2) the
    * "device" instead of calling DnD_InitializeBlocking() and bringing along
    * a whole host of libs.
    */
   blockFd = open(VMBLOCK_FUSE_DEVICE, VMBLOCK_FUSE_DEVICE_MODE);
   if (blockFd < 0) {
      blockFd = open(VMBLOCK_DEVICE, VMBLOCK_DEVICE_MODE);
   }

   if (useWayland) {
      uinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
      if (uinputFd < 0) {
         uinputFd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK);
      }
   }

   uid = getuid();
   gid = getgid();

   if ((setreuid(uid, uid) != 0) ||
       (setregid(gid, gid) != 0)) {
      Error("could not drop privileges: %s\n", strerror(errno));
      if (blockFd != -1) {
         close(blockFd);
      }
      if (useWayland) {
         if (uinputFd != -1) {
            close(uinputFd);
         }
      }
      return FALSE;
   }

   /*
    * Since vmware-user provides features that don't depend on vmblock, we
    * invoke vmware-user even if we couldn't obtain a file descriptor or we
    * can't parse the descriptor to pass as an argument.  We set up the
    * argument vector accordingly.
    */
   argv[idx++] = path;
   argv[idx++] = "-n";
   argv[idx++] = "vmusr";

   if (blockFd < 0) {
      Error("could not open %s\n", VMBLOCK_DEVICE);
   } else {
      ret = snprintf(blockFdStr, sizeof blockFdStr, "%d", blockFd);
      if (ret == 0 || ret >= sizeof blockFdStr) {
         Error("could not parse file descriptor (%d)\n", blockFd);
      } else {
         argv[idx++] = "--blockFd";
         argv[idx++] = blockFdStr;
      }
   }

   if (useWayland) {
      if (uinputFd < 0) {
         Error("could not open uinput device\n");
      } else {
         ret = snprintf(uinputFdStr, sizeof uinputFdStr, "%d", uinputFd);
         if (ret == 0 || ret >= sizeof uinputFdStr) {
            Error("could not parse uinput file descriptor (%d)\n", uinputFd);
         } else {
            argv[idx++] = "--uinputFd";
            argv[idx++] = uinputFdStr;
         }
      }
   }

   argv[idx++] = NULL;
   CompatExec(path, argv, envp);

   /*
    * CompatExec, if successful, doesn't return.  I.e., we're here only
    * if CompatExec fails.
    */
   Error("could not execute %s: %s\n", path, strerror(errno));
   _exit(EXIT_FAILURE);
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
   if (execPathSize < sizeof VMTOOLSD_PATH) {
      return FALSE;
   }
   strcpy(execPath, VMTOOLSD_PATH);
   return TRUE;
}
#endif // ifndef USES_LOCATIONS_DB
