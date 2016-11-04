/*********************************************************
 * Copyright (C) 2005-2016 VMware, Inc. All rights reserved.
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
 *----------------------------------------------------------------------
 *
 * miscSolaris --
 *
 *      Implementation of new Linux functions for Solaris.
 *
 *----------------------------------------------------------------------
 */

#ifdef sun
#include "vmware.h"
#include <unistd.h>
#include <iso/stdlib_iso.h>
#include <fcntl.h>

/*
 *----------------------------------------------------------------------
 *
 * daemon --
 *
 *   Implementation of function daemon() for Solaris.
 *
 * Results:
 *   0 if successful, -1 if failed.
 *
 * Side effects:
 *   None
 *
 *----------------------------------------------------------------------
 */

int
daemon(int nochdir,  // IN:
       int noclose)  // IN:
{
   int fd;

   switch (fork()) {
   case -1:
      return (-1);
   case 0:
      break;
   default:
      /* It is parent, so exit here. */
      exit(0);
   }

   if (setsid() == -1) {
      return (-1);
   }

   if (!nochdir) {
      chdir("/");
   }

   if (!noclose && (fd = open("/dev/null", O_RDWR, 0)) != -1) {
      dup2(fd, 0);
      dup2(fd, 1);
      dup2(fd, 2);
      if (fd > 2) {
         close (fd);
      }
   }

   return (0);
}
#endif
