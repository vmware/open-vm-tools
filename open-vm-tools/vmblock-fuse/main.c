/*********************************************************
 * Copyright (C) 2008-2016,2021 VMware, Inc. All rights reserved.
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
 *      Entry point for vmblock-fuse file system.
 */

#include <string.h>
#include <stdio.h>

#include "fsops.h"

int LOGLEVEL_THRESHOLD = 0;


/*
 *-----------------------------------------------------------------------------
 *
 * main --
 *
 *      Entry point for the vmblock-fuse file system. Calls fuse_main(). See
 *      http://fuse.sourceforge.net/doxygen/fuse_8h.html#3bf31250361d44c2436d76f47f2400ed
 *      for more information.
 *
 *      There are many command line options that fuse filesystems can take. Run
 *      with --help for a listing or consult the fuse documentation.
 *      Options which are likely to be usefor for vmblock-fuse are
 *      -o default_permissions and -o allow_other.
 *
 *      If the -d option is specified, enables our logging in addition to
 *      what fuse does.
 *
 * Results:
 *      Returns 0 on success and nonzero on failure.
 *
 * Side effects:
 *      None/all.
 *
 *-----------------------------------------------------------------------------
 */

int
main(int argc,           // IN
     char *argv[])       // IN
{
   int i;
   for (i = 1; i < argc && strcmp(argv[i], "--") != 0; ++i) {
      if (strcmp(argv[i], "-d") == 0) {
         LOGLEVEL_THRESHOLD = 4;
      }
   }
#if FUSE_USE_VERSION < 26
   return fuse_main(argc, argv, &vmblockOperations);
#else
   return fuse_main(argc, argv, &vmblockOperations, NULL);
#endif
}
