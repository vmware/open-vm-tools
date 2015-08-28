/*********************************************************
 * Copyright (C) 2007-2014 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/


/*
 * driverLog.c --
 *
 *      Common logging functions for Linux kernel modules.
 */

#include "driver-config.h"
#include "compat_kernel.h"
#include "compat_sched.h"
#include <asm/current.h>

#include "driverLog.h"

#define LINUXLOG_BUFFER_SIZE 1024

static const char *driverLogPrefix = "";

/*
 * vsnprintf was born in 2.4.10. Fall back on vsprintf if we're
 * an older kernel.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 10)
# define vsnprintf(str, size, fmt, args) vsprintf(str, fmt, args)
#endif


/*
 *----------------------------------------------------------------------------
 *
 * DriverLog_Init --
 *
 *      Initializes the Linux logging.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
DriverLog_Init(const char *prefix) // IN
{
   driverLogPrefix = prefix ? prefix : "";
}


/*
 *----------------------------------------------------------------------
 *
 * DriverLogPrint --
 *
 *      Log error message from a Linux module.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
DriverLogPrint(const char *level,     // IN: KERN_* constant
               const char *fmt,       // IN: error format string
               va_list args)          // IN: arguments for format string
{
   static char staticBuf[LINUXLOG_BUFFER_SIZE];
   char stackBuf[128];
   va_list args2;
   const char *buf;

   /*
    * By default, use a small buffer on the stack (thread safe). If it is too
    * small, fall back to a larger static buffer (not thread safe).
    */
   va_copy(args2, args);
   if (vsnprintf(stackBuf, sizeof stackBuf, fmt, args2) < sizeof stackBuf) {
      buf = stackBuf;
   } else {
      vsnprintf(staticBuf, sizeof staticBuf, fmt, args);
      buf = staticBuf;
   }
   va_end(args2);

   printk("%s%s[%d]: %s", level, driverLogPrefix, current->pid, buf);
}


/*
 *----------------------------------------------------------------------
 *
 * Warning --
 *
 *      Warning messages from kernel module: logged into kernel log
 *      as warnings.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Warning(const char *fmt, ...)  // IN: warning format string
{
   va_list args;

   va_start(args, fmt);
   DriverLogPrint(KERN_WARNING, fmt, args);
   va_end(args);
}


/*
 *----------------------------------------------------------------------
 *
 * Log --
 *
 *      Log messages from kernel module: logged into kernel log
 *      as debug information.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Log(const char *fmt, ...)  // IN: log format string
{
   va_list args;

   /*
    * Use the kernel log with at least a KERN_DEBUG level
    * so it doesn't garbage the screen at (re)boot time on RedHat 6.0.
    */

   va_start(args, fmt);
   DriverLogPrint(KERN_DEBUG, fmt, args);
   va_end(args);
}


/*
 *----------------------------------------------------------------------
 *
 * Panic --
 *
 *      ASSERTION failures and Panics from kernel module get here.
 *      Message is logged to the kernel log and on console.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Never returns
 *
 *----------------------------------------------------------------------
 */

void
Panic(const char *fmt, ...)  // IN: panic format string
{
   va_list args;

   va_start(args, fmt);
   DriverLogPrint(KERN_EMERG, fmt, args);
   va_end(args);

#ifdef BUG
   BUG();
#else
   /* Should die with %cs unwritable, or at least with page fault. */
   asm volatile("movb $0, %cs:(0)");
#endif

   while (1);
}
