/*********************************************************
 * Copyright (C) 2008-2016,2019 VMware, Inc. All rights reserved.
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
 * manual-blocker.c --
 *
 *      A small test program for manually manipulating vmblock.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>

#include "vmblock_user.h"


/*
 *-----------------------------------------------------------------------------
 *
 * main --
 *
 *      Takes the target file to block as a command line arg. Sits in a loop
 *      waiting for commands.
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
main(int argc,
     char *argv[])
{
   int status;

   if (argc < 2 ||
       strcmp(argv[1], "-h") == 0 ||
       strcmp(argv[1], "--help") == 0) {
      printf("Usage: %s <path>\n", argv[0]);
      puts("a to Add a block, d to Delete a block, or l to List blocks (to"
           " vmblock's log).\n");
      return 1;
   }

   int fd = open(VMBLOCK_DEVICE, VMBLOCK_DEVICE_MODE);
   if (fd <= 0) {
      perror("open");
      return 2;
   }
   printf("Opened " VMBLOCK_DEVICE " as fd %d.\n", fd);

   while (1) {
      char op = getchar();

      if (op == 'a') {
         status = VMBLOCK_CONTROL(fd, VMBLOCK_ADD_FILEBLOCK, argv[1]);
         if (status != 0) {
            perror(NULL);
         } else {
            printf("%s blocked.\n", argv[1]);
         }
      } else if (op == 'd') {
         status = VMBLOCK_CONTROL(fd, VMBLOCK_DEL_FILEBLOCK, argv[1]);
         if (status != 0) {
            perror(NULL);
         } else {
            printf("%s unblocked.\n", argv[1]);
         }
      } else if (op == 'l') {
         status = VMBLOCK_CONTROL(fd, VMBLOCK_LIST_FILEBLOCKS, argv[1]);
         if (status != 0) {
            perror(NULL);
         } else {
            printf("Check vmblock's log for list of blocks.\n");
         }
      }
   }

   return 0;
}
