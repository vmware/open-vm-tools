/*********************************************************
 * Copyright (C) 2011-2016, 2021 VMware, Inc. All rights reserved.
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
 * vmblockmounter.c --
 *
 *      Helper app for mounting vmblock filesystem on FreeBSD and Solaris.
 *      Linux does not need it as it knows how to mount pseudo-filesystems
 *      without a helper program.
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#if defined(__FreeBSD__)
#   include <sys/uio.h>
#   include <sys/param.h>
#endif
#include <sys/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "vmblock.h"
#include "vmblockmounter_version.h"

#include "vm_version.h"
#include "embed_version.h"
VM_EMBED_VERSION(VMBLOCKMOUNTER_VERSION_STRING);

#define LOG(format, ...) (beVerbose ? printf(format, ##__VA_ARGS__) : 0)

static char *thisProgram;
static char *thisProgramBase;
static Bool beVerbose = FALSE;

/*
 *-----------------------------------------------------------------------------
 *
 * PrintVersion --
 *
 *    Displays version.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
PrintVersion(void)
{
   printf("%s version: %s\n", thisProgramBase, VMBLOCKMOUNTER_VERSION_STRING);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PrintUsage --
 *
 *    Displays usage for the vmblock mounting utility.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
PrintUsage(FILE *fd)   // IN: File stream to use for output
{
   fprintf(fd, "Usage: %s <source> <dir> [-o <options>]\n", thisProgramBase);
   fprintf(fd, "Mount the vmblock filesystem at given mount point.\n");
   fprintf(fd, "\n");
   fprintf(fd, "This command is intended to be run from within /bin/mount by\n");
   fprintf(fd, "passing the option '-t %s'. For example:\n", VMBLOCK_FS_NAME);
   fprintf(fd, "  mount -t %s /tmp/VMwareDnD /var/run/vmblock\n",
           VMBLOCK_FS_NAME);
}


/*-----------------------------------------------------------------------------
 *
 * main --
 *
 *    Main entry point. Parses the mount options received, makes a call to
 *    mount(2), and handles the results.
 *
 * Results:
 *    Zero on success, non-zero on failure.
 *
 * Side effects:
 *    May mount a vmblock filesystem.
 *
 *-----------------------------------------------------------------------------
 */

int
main(int argc,          // IN
     char *argv[])      // IN
{
   int c;
   int i;
   int result = EXIT_FAILURE;
   int mntRes = -1;
   struct stat statBuf;
   char *sourceDir;
   char *mountPoint;

   thisProgram = argv[0];

   /* Compute the base name of the program, too. */
   thisProgramBase = strrchr(thisProgram, '/');
   if (thisProgramBase != NULL) {
      thisProgramBase++;
   } else {
      thisProgramBase = thisProgram;
   }

   while ((c = getopt(argc, argv, "hvV")) != -1) {
      switch (c) {
      case 'h':
         PrintUsage(stdout);
         result = EXIT_SUCCESS;
         goto out;

      case 'v':
         beVerbose = TRUE;
         break;

      case 'V':
         PrintVersion();
         result = EXIT_SUCCESS;
         goto out;

      case '?':
      default:
         PrintUsage(stderr);
         goto out;
      }
   }

   LOG("Original command line: \"%s", thisProgram);
   for (i = 1; i < argc; i++) {
      LOG(" %s", argv[i]);
   }
   LOG("\"\n");

   /* After getopt_long(3), optind is the first non-option argument. */
   if (argc != optind + 2) {
      fprintf(stderr, "Error: invalid number of arguments\n");
      PrintUsage(stderr);
      goto out;
   }

   sourceDir = argv[optind];
   mountPoint = argv[optind + 1];

   /* Do some sanity checks on our desired mount point. */
   if (stat(mountPoint, &statBuf)) {
      perror("Error: cannot stat mount point");
      goto out;
   }

   if (S_ISDIR(statBuf.st_mode) == 0) {
      fprintf(stderr,
              "Error: mount point \"%s\" is not a directory\n", mountPoint);
      goto out;
   }

   if (access(mountPoint, X_OK) < 0) {
      fprintf(stderr,
              "Error: no access rights to mount point \"%s\"\n", mountPoint);
      goto out;
   }

   /* Do the same checks on the source directory. */
   if (stat(sourceDir, &statBuf)) {
      perror("Error: cannot stat source directory");
      goto out;
   }

   if (S_ISDIR(statBuf.st_mode) == 0) {
      fprintf(stderr, "Error: source \"%s\" is not a directory\n", sourceDir);
      goto out;
   }

   if (access(sourceDir, X_OK) < 0) {
      fprintf(stderr, "Error: no access rights to source \"%s\"\n", sourceDir);
      goto out;
   }

   /* Go! */
#if defined(sun)
   mntRes = mount(sourceDir, mountPoint, MS_DATA, VMBLOCK_FS_NAME);
#elif defined(__FreeBSD__)
   {
      struct iovec iov[] = {
         { .iov_base = "fstype", .iov_len = sizeof "fstype" },
         { .iov_base = VMBLOCK_FS_NAME, .iov_len = sizeof VMBLOCK_FS_NAME },
         { .iov_base = "fspath", .iov_len = sizeof "fspath" },
         { .iov_base = mountPoint, .iov_len = strlen(mountPoint) + 1 },
         { .iov_base = "target", .iov_len = sizeof "target" },
         { .iov_base = sourceDir, .iov_len = strlen(sourceDir) + 1 }
      };
      mntRes = nmount(iov, ARRAYSIZE(iov), MNT_NOSUID);
   }
#else
#error "Unsupported OS"
#endif

   if (mntRes) {
      perror("Error: cannot mount filesystem");
      goto out;
   }

   result = EXIT_SUCCESS;

out:
   return result;
}

