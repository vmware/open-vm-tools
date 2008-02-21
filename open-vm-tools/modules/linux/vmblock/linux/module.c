/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * module.c --
 *
 *   Module loading/unloading functions.
 *
 */

#include "driver-config.h"
#include "compat_init.h"
#include "compat_kernel.h"
#include "compat_module.h"
#include <linux/limits.h>
#include <linux/errno.h>
#include "compat_string.h"

#include "vmblockInt.h"
#include "vmblock_version.h"

/* Module parameters */
#ifdef VMX86_DEVEL /* { */
int LOGLEVEL_THRESHOLD = 4;
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
   module_param(LOGLEVEL_THRESHOLD, int, 0600);
#  else
   MODULE_PARM(LOGLEVEL_THRESHOLD, "i");
#  endif
MODULE_PARM_DESC(LOGLEVEL_THRESHOLD, "Logging level (0 means no log, "
                 "10 means very verbose, 4 is default)");
#endif /* } */

static char *root = "/tmp/VMwareDnD";
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
module_param(root, charp, 0600);
#else
MODULE_PARM(root, "s");
#endif
MODULE_PARM_DESC(root, "The directory the file system redirects to.");

/* Module information */
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Blocking File System");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VMBLOCK_DRIVER_VERSION_STRING);

/* Functions */
static int VMBlockInit(void);
static void VMBlockExit(void);

/* Define init/exit routines */
module_init(VMBlockInit);
module_exit(VMBlockExit);


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockInit --
 *
 *    Module entry point and initialization.
 *
 * Results:
 *    Zero on success, negative value on failure.
 *
 * Side effects:
 *    /proc entries are available and file system is registered with kernel and
 *    ready to be mounted.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockInit(void)
{
   int ret;

   ret = VMBlockInitControlOps();
   if (ret < 0) {
      goto error;
   }

   ret = VMBlockInitFileSystem(root);
   if (ret < 0) {
      VMBlockCleanupControlOps();
      goto error;
   }

   LOG(4, "module loaded\n");
   return 0;

error:
   Warning("VMBlock: could not initialize module\n");
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockExit --
 *
 *    Unloads module from kernel and removes associated state.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Opposite of VMBlockInit(): /proc entries go away and file system is
 *    unregistered.
 *
 *----------------------------------------------------------------------------
 */

static void
VMBlockExit(void)
{
   VMBlockCleanupControlOps();
   VMBlockCleanupFileSystem();

   LOG(4, "module unloaded\n");
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 70)
/*
 *----------------------------------------------------------------------------
 *
 * strlcpy --
 *
 *    2.4 doesn't have strlcpy().
 *
 *    Copies at most count - 1 bytes from src to dest, and ensures dest is NUL
 *    terminated.
 *
 * Results:
 *    Length of src.  If src >= count, src was truncated in copy.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

size_t
strlcpy(char *dest,         // OUT: destination to copy string to
        const char *src,    // IN : source to copy string from
        size_t count)       // IN : size of destination buffer
{
   size_t ret;
   size_t len;

   ret = strlen(src);
   len = ret >= count ? count - 1 : ret;
   memcpy(dest, src, len);
   dest[len] = '\0';
   return ret;
}
#endif
