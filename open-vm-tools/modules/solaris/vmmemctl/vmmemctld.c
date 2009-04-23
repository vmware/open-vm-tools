/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * vmmemctld.c --
 *
 *	Simple daemon that provides a worker thread for the vmmemctl
 *	driver.  Note that opening the device node causes the driver
 *	to load, and the driver can't be unloaded as long as we're
 *	running inside it.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "vm_basic_types.h"
#include "vmmemctl.h"

static const char *pname;
static const char dpath[] = "/devices/pseudo/vmmemctl@0:0";

static void myperror(const char *str)
{
   fprintf(stderr, "%s: ", pname);
   perror(str);
}

int main(int argc, char **argv)
{
   int err;
   pid_t pid;
   int fd;

   pname = argv[0];

   /*
    * Basic sanity checks
    */
   if (chdir("/") < 0) {
      myperror("chdir");
      return 1;
   }

   /*
    * Run in background
    */
   pid = fork();
   if (pid < 0) {
      myperror("fork");
      return 1;
   }
   if (pid != 0) {
      /*
       * Parent writes pidfile if specified.
       */
      if (argc >= 3 && strcmp(argv[1], "--background") == 0) {
         int fd;
         int len;
         char buf[64];

         (void) unlink(argv[2]);

         fd = open(argv[2], O_CREAT | O_EXCL | O_WRONLY);
         if (fd < 0) {
            myperror("open");
            return 1;
         }

         len = snprintf(buf, sizeof buf, "%"FMTPID"\n", pid);
         if (len >= sizeof buf) {
            /* String was truncated */
            myperror("snprintf");
            close(fd);
            return 1;
         }

         if (write(fd, buf, len) != len) {
            myperror("write");
            close(fd);
            return 1;
         }

         if (close(fd) != 0) {
            myperror("close");
            return 1;
         }
      }

      return 0;
   }

   /*
    * Clean up file descriptors and detach from controlling tty.
    */
   closefrom(0);
   (void) open("/dev/null", O_RDONLY);
   (void) open("/dev/null", O_WRONLY);
   (void) dup(1);
   (void) setsid();

   /*
    * Call into the driver to do work.
    */
   if ((fd = open(dpath, O_RDWR)) == -1) {
      myperror("open");
      return 1;
   }

   /*
    * If ioctl returns EINTR, we were interrupted by a non-fatal signal.
    * Call back into the driver to continue working.
    */
   while ((err = ioctl(fd, VMMIOCWORK, 0)) == EINTR)
      ;

   if (err != 0) {
      myperror("ioctl");
      return 1;
   }

   /*
    * We've been told to exit cleanly.
    */
   (void) close(fd);
   return 0;
}
