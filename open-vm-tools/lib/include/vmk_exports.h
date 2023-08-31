/*********************************************************
 * Copyright (C) 2009-2016,2023 VMware, Inc. All rights reserved.
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

/*********************************************************
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
 * vmk_exports.h --
 *
 *      Macros for exporting symbols from vmkernel.
 */

#ifndef _VMK_EXPORTS_H
#define _VMK_EXPORTS_H

#ifdef VMKERNEL
#   include "vm_basic_defs.h"

#   define _VMK_KERNEL_EXPORT(string) asm(                                    \
   ".pushsection .vmkexports, \"aS\", @progbits"                       "\n\t" \
   ".string \"" string "\""                                            "\n\t" \
   ".popsection"                                                              \
)
#else
#   define _VMK_KERNEL_EXPORT(string)
#endif

/*
 * The following macros allow you to export symbols from vmkernel,
 * thus making them available for modules to access.
 *
 * The versions with no explicit name-space and version imply the
 * default vmkernel name-space/version.  They should be considered
 * deprecated as they impede your ability to create very stable APIs.
 *
 * Sample usage:
 *
 * VMK_KERNEL_EXPORT(foo)          : export foo
 *
 * VMK_KERNEL_ALIAS(foo, fooAlias) : export foo as fooAlias
 *
 * VMK_KERNEL_EXPORT_TO_NAMESPACE(foo, "ns", "ver") :
 *                                   export foo @ns:ver
 *
 * VMK_KERNEL_ALIAS_TO_NAMESPACE(foo, fooAlias, "ns", "ver") :
 *                                   export foo as fooAlias @ns:ver
 *
 * Further details on the name-space/version implementation can be
 * found in the VMKAPI module headers and documentation.  The 2 second
 * version is that if you export foo@myns:1, modules wanting access to
 * that symbol will require an explicit
 * VMK_MODULE_NAMESPACE_REQUIRED("myns", "1") tag to see that symbol.
 */

#define VMK_KERNEL_EXPORT(symbol)                                             \
   _VMK_KERNEL_EXPORT(XSTR(symbol))

#define VMK_KERNEL_ALIAS(symbol, alias)                                       \
   _VMK_KERNEL_EXPORT(XSTR(symbol) "!" XSTR(alias))

#define VMK_KERNEL_EXPORT_TO_NAMESPACE(symbol, namespace, version)            \
   _VMK_KERNEL_EXPORT(XSTR(symbol) "@" namespace ":" version)

#define VMK_KERNEL_ALIAS_TO_NAMESPACE(symbol, alias, namespace, version)      \
   _VMK_KERNEL_EXPORT(XSTR(symbol) "!" XSTR(alias) "@" namespace ":" version)

#endif /* _VMK_EXPORTS_H */
