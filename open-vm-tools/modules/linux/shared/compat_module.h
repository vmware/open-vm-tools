/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * compat_module.h --
 */

#ifndef __COMPAT_MODULE_H__
#   define __COMPAT_MODULE_H__


#include <linux/module.h>


/*
 * Modules wishing to use the GPL license are required to include a
 * MODULE_LICENSE definition in their module source as of 2.4.10.
 */
#ifndef MODULE_LICENSE
#define MODULE_LICENSE(license)
#endif

/*
 * To make use of our own home-brewed MODULE_INFO, we need macros to
 * concatenate two expressions to "__mod_", and and to convert an
 * expression into a string. I'm sure we've got these in our codebase,
 * but I'd rather not introduce such a dependency in a compat header.
 */
#ifndef __module_cat
#define __module_cat_1(a, b) __mod_ ## a ## b
#define __module_cat(a, b) __module_cat_1(a, b)
#endif

#ifndef __stringify
#define __stringify_1(x) #x
#define __stringify(x) __stringify_1(x)
#endif

/*
 * MODULE_INFO was born in 2.5.69.
 */
#ifndef MODULE_INFO
#define MODULE_INFO(tag, info)                                                \
static const char __module_cat(tag, __LINE__)[]                               \
  __attribute__((section(".modinfo"), unused)) = __stringify(tag) "=" info
#endif

/*
 * MODULE_VERSION was born in 2.6.4. The earlier form appends a long "\0xxx"
 * string to the module's version, but that was removed in 2.6.10, so we'll
 * ignore it in our wrapper.
 */
#ifndef MODULE_VERSION
#define MODULE_VERSION(_version) MODULE_INFO(version, _version)
#endif

/*
 * Linux kernel < 2.6.31 takes 'int' for 'bool' module parameters.
 * Linux kernel >= 3.3.0 takes 'bool' for 'bool' module parameters.
 * Kernels between the two take either.  So flip switch at 3.0.0.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
   typedef bool compat_mod_param_bool;
#else
   typedef int compat_mod_param_bool;
#endif

#endif /* __COMPAT_MODULE_H__ */
