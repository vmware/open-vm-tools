/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * eventfd.c --
 *
 *    Implements eventfd syscall interface.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "eventfd.h"

#ifdef VMWARE_EVENTFD_REAL
#include <unistd.h>
#include <sys/syscall.h>


#ifdef SYS_eventfd2
/*
 * If this error fires, either start using <sys/eventfd.h> instead of
 * "eventfd.h", or rename our eventfd/eventfd_read/eventfd_write to some
 * private names and modify all users to use them. As you can guess,
 * switching to glibc's eventfd.h is preferred choice.
 */

#   error "You have real SYS_eventfd2.  You should stop using this one."
#endif
#ifdef VM_X86_64
#   define SYS_eventfd  284
#   define SYS_eventfd2 290
#else
#   define SYS_eventfd  323
#   define SYS_eventfd2 328
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * eventfd --
 *
 *      Create eventfd descriptor.
 *
 * Results:
 *      -1 on failure, errno set
 *      >= 0 on success, file descriptor to use
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
eventfd(int count,               // IN: Initial count
        int flags)               // IN: Initial flags
{
   static enum {
      EVENTFD_UNKNOWN = 0,
      EVENTFD_UNKNOWN_NOT2,
      EVENTFD_EVENTFD2,
      EVENTFD_EVENTFD,
      EVENTFD_NONE,
   } eventfdStyle;
   int ret;

   switch (eventfdStyle) {
   case EVENTFD_EVENTFD2: /* ESX and linux 2.6.27+ */
      return syscall(SYS_eventfd2, count, flags);
   case EVENTFD_EVENTFD:  /* linux 2.6.22 - 2.6.27 */
      if (flags) {
         errno = EINVAL;
         return -1;
      }
      return syscall(SYS_eventfd, count);
   case EVENTFD_UNKNOWN:  /* here we start */
      ret = syscall(SYS_eventfd2, count, flags);
      if (ret != -1 || errno != ENOSYS) {
         /*
          * We can get back SYS_eventfd2 instead of -1/ENOSYS on some
          * broken systems - see bug 460859.  This tries to figure out
          * whether returned value is legitimate fd 328, or just
          * masqueraded ENOSYS.
          */
         if (ret != SYS_eventfd2) {
            eventfdStyle = EVENTFD_EVENTFD2;
            return ret;
         }

         /*
          * Allocate another eventfd.  If that one is 328 too, it is
          * clear that kernel is broken, as we already got such fd
          * above.
          *
          * Otherwise if it is not -1, or if errno is not ENOSYS,
          * then eventfd2 works.  Close newly allocated fd, and
          * return 328 (we return 328 and not new one so code path
          * is same for both success & failure).
          *
          * If new call failed with ENOSYS, then it means that someone
          * attached strace between first and second eventfd2 syscalls -
          * it means that 328 we got was bogus, and system does not
          * support eventfd2.
          */
         ret = syscall(SYS_eventfd2, count, flags);
         if (ret != SYS_eventfd2) {
            if (ret != -1 || errno != ENOSYS) {
               eventfdStyle = EVENTFD_EVENTFD2;
               if (ret >= 0) {
                  close(ret);
               }
               return SYS_eventfd2;
            }
         }
      }
      eventfdStyle = EVENTFD_UNKNOWN_NOT2;
      /* FALLTHRU */
   case EVENTFD_UNKNOWN_NOT2: /* not eventfd2, but caller needed flags */
      if (flags) {
         errno = EINVAL;
         return -1;
      }
      ret = syscall(SYS_eventfd, count);
      if (ret != -1 || errno != ENOSYS) {
         eventfdStyle = EVENTFD_EVENTFD;
         return ret;
      }
      eventfdStyle = EVENTFD_NONE;
      /* FALLTHRU */
   default:             /* none - 2.6.21 and older */
      break;
   }
   errno = ENOSYS;
   return -1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * eventfd_read --
 *
 *      Read count from the descriptor.
 *
 * Results:
 *      -1 on failure, errno set
 *      0 on success, number of pending events retrieved
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
eventfd_read(int fd,             // IN: event fd
             eventfd_t *value)   // OUT: value
{
   ssize_t ret;

   ret = read(fd, value, sizeof *value);
   if (ret == sizeof *value) {
      return 0;
   }
   if (ret != -1) {
      errno = EINVAL;
   }
   return -1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * eventfd_write --
 *
 *      Write (increase) count on the descriptor.
 *
 * Results:
 *      -1 on failure, errno set
 *      0 on success, number of pending events increased
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
eventfd_write(int fd,            // IN: event fd
              eventfd_t value)   // OUT: value
{
   ssize_t ret;

   ret = write(fd, &value, sizeof value);
   if (ret == sizeof value) {
      return 0;
   }
   if (ret != -1) {
      errno = EINVAL;
   }
   return -1;
}

#endif /* VMWARE_EVENTFD_REAL */
