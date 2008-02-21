/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_UACCESS_H__
#   define __COMPAT_UACCESS_H__


/* User space access functions moved in 2.1.7 to asm/uaccess.h --hpreg */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 1, 7)
#   include <asm/uaccess.h>
#else
#   include <asm/segment.h>
#endif


/* get_user() API modified in 2.1.4 to take 2 arguments --hpreg */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 1, 4)
#   define compat_get_user get_user
#else
/*
 * We assign 0 to the variable in case of failure to prevent "`_var' might be
 * used uninitialized in this function" compiler warnings. I think it is OK,
 * because the hardware-based version in newer kernels probably has the same
 * semantics and does not guarantee that the value of _var will not be
 * modified, should the access fail --hpreg
 */
#   define compat_get_user(_var, _uvAddr) ({                        \
   int _status;                                                     \
                                                                    \
   _status = verify_area(VERIFY_READ, _uvAddr, sizeof(*(_uvAddr))); \
   if (_status == 0) {                                              \
      (_var) = get_user(_uvAddr);                                   \
   } else {                                                         \
      (_var) = 0;                                                   \
   }                                                                \
   _status;                                                         \
})
#endif


/*
 * The copy_from_user() API appeared in 2.1.4
 *
 * The emulation is not perfect here, but it is conservative: on failure, we
 * always return the total size, instead of the potentially smaller faulty
 * size --hpreg
 *
 * Since 2.5.55 copy_from_user() is no longer macro.
 */
#if !defined(copy_from_user) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 0)
#   define copy_from_user(_to, _from, _size) ( \
   verify_area(VERIFY_READ, _from, _size)      \
       ? (_size)                               \
       : (memcpy_fromfs(_to, _from, _size), 0) \
)
#   define copy_to_user(_to, _from, _size) ( \
   verify_area(VERIFY_WRITE, _to, _size)     \
       ? (_size)                             \
       : (memcpy_tofs(_to, _from, _size), 0) \
)
#endif


#endif /* __COMPAT_UACCESS_H__ */
