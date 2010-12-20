/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 *
 * vm_api.h --
 *
 *    Import/export macro definitions.
 */


#ifndef VM_API_H
#define VM_API_H

/*
 * DLL/DSO import/export macros.
 *
 * These macros can be used by libraries to automatically export/import symbols.
 * The general approach is:
 *
 * 1) libfoo defines a macro, say FOO_COMPILING_DYNAMIC,
 *    in its Makefile/Scons file.
 *
 * 2) libfoo has the following code in a header file, e.g. foo/config.h, that is
 *    included by all headers that export/import symbols:
 *     #include <vm_api.h>
 *     #ifdef FOO_COMPILING_DYNAMIC
 *     #  define FOO_API VMW_EXPORT
 *     #else
 *     #  define FOO_API VMW_IMPORT
 *     #endif // FOO_COMPILING_DYNAMIC
 *
 *    For example:
 *     // In a file named FooObject.h
 *     #ifndef FOO_OBJECT_H
 *     #define FOO_OBJECT_H
 *
 *     #include <foo/config.h>
 *
 *     class FOO_API FooObject { };
 *     FOO_API FooObject *GetFooObject();

 *     #endif // FOO_OBJECT_H
 *
 * 3) libfoo can now use FOO_API for all symbols it would like to export,
 *    which resolves to VMW_EXPORT, while compiling libfoo as a dynamic shared
 *    library.
 *
 * 4) Whenever a client of libfoo includes its headers, these symbols will be
 *    marked with VMW_IMPORT, since FOO_COMPILING_DYNAMIC is not defined for
 *    the client.
 *
 * NOTE: By default, symbols are hidden when compiling with MSC and exported
 * when compiling with GCC.  Thus, it's best to compile with GCC's
 * -fvisibility=hidden and -fvisibility-inlines-hidden flags, so that only
 * symbols explicitly marked with VMW_EXPORT are exported.  Also note that
 * these flags, as well as the attributes, are available in GCC 4 and later.
 */
#ifdef _MSC_VER
#  define VMW_IMPORT    __declspec(dllimport)
#  define VMW_EXPORT    __declspec(dllexport)
#elif defined __GNUC__ && __GNUC__ >= 4 /* !_MSC_VER */
#  define VMW_IMPORT
#  define VMW_EXPORT    __attribute__ ((visibility ("default")))
#else
#  define VMW_IMPORT
#  define VMW_EXPORT
#endif /* _MSC_VER */


#endif /* VM_API_H */
