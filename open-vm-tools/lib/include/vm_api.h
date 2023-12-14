/*********************************************************
 * Copyright (c) 2010-2016,2021 VMware, Inc. All rights reserved.
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
 * API macros
 *
 * These macros can be used by libraries to export/import/hide symbols.
 *
 * The general approach is:
 *
 * 1) libfoo defines a macro, say EXPORT_FOO_API or STATIC_FOO_API,
 *    in its Makefile/SCons file depending on the type of the library.
 *
 * 2) libfoo has the following code in a header file, e.g. foo/platform.h,
 *    that is included by all headers that export/import/hide symbols:
 *
 *     // In a file named platform.h
 *     #include <vm_api.h>
 *
 *     #ifdef STATIC_FOO_API
 *     #  define FOO_API VMW_LIB_STATIC
 *     #  define FOO_INLINE_API VMW_LIB_STATIC
 *     #elif defined EXPORT_FOO_API
 *     #  define FOO_API VMW_LIB_DYNAMIC
 *     #  define FOO_INLINE_API VMW_LIB_DYNAMIC_INLINE
 *     #else
 *     #  define FOO_API VMW_LIB_CLIENT
 *     #  define FOO_INLINE_API VMW_LIB_CLIENT_INLINE
 *     #endif
 *
 *    For example:
 *     // In a file named FooTypes.h
 *     #ifndef FOO_TYPES_H
 *     #define FOO_TYPES_H
 *
 *     #include <foo/platform.h>
 *
 *     class FOO_API FooObject { };
 *     FOO_API FooObject *GetFooObject();
 *
 *     class FOO_INLINE_API FooException { };
 *     FOO_INLINE_API int GetInt() { return 5; }
 *
 *     #endif // FOO_TYPES_H
 *
 * DLL/DSO import/export macros.
 *
 * 3) Compiling a shared library - define EXPORT_FOO_API.
 *    libfoo can now use FOO_API for all symbols it would like to export,
 *    which resolves to VMW_LIB_DYNAMIC, while compiling libfoo as a dynamic
 *    shared library.
 *    On posix systems types that need to export RTTI information, including
 *    exceptions, must have default visibility. However these types may not need
 *    __declspec(dllexport) on Windows. Such classes should be marked with a
 *    FOO_INLINE_API macro.
 *
 * 4) Linking against a shared library - no defines needed.
 *    Whenever a client of libfoo includes its headers, these symbols will be
 *    marked with VMW_LIB_CLIENT or VMW_LIB_CLIENT_INLINE, since EXPORT_FOO_API
 *    and STATIC_FOO_API are not defined for the client.
 *
 * Static library macros.
 *
 * 3) Compiling a static library - define STATIC_FOO_API.
 *    libfoo should hide all of its symbols so that they don't leak through to
 *    another library, say libbar, which links libfoo statically. FOO_API and
 *    FOO_INLINE_API would both resolve to VMW_LIB_STATIC while compiling libfoo
 *    as a static library.
 *
 * 4) Linking against a static library - define STATIC_FOO_API.
 *    Whenever a client of libfoo includes its headers, the libfoo symbols
 *    should be hidden on posix systems, marked with VMW_LIB_STATIC. On Windows
 *    defining STATIC_FOO_API is a must when linking against the libfoo static
 *    library. If it's not defined then the libfoo symbols would get an
 *    __declspec(dllimport) declaration. As a result the linker will try to
 *    import them from the list of DLLs present in the link line instead of
 *    linking them directly from the libfoo static library.
 *
 * NOTE: By default, symbols are hidden when compiling with MSC and exported
 * when compiling with GCC.  Thus, it's best to compile with GCC's
 * -fvisibility=hidden and -fvisibility-inlines-hidden flags, so that only
 * symbols explicitly marked with VMW_LIB_DYNAMIC are exported.  Also note that
 * these flags, as well as the attributes, are available in GCC 4 and later.
 *
 * @see http://gcc.gnu.org/wiki/Visibility
 */

#ifdef _MSC_VER
#  define VMW_LIB_STATIC
#  define VMW_LIB_CLIENT   __declspec(dllimport)
#  define VMW_LIB_CLIENT_INLINE
#  define VMW_LIB_DYNAMIC  __declspec(dllexport)
#  define VMW_LIB_DYNAMIC_INLINE
#elif defined __GNUC__
#  define VMW_LIB_STATIC  __attribute__ ((visibility ("hidden")))
#  define VMW_LIB_CLIENT  __attribute__ ((visibility ("default")))
#  define VMW_LIB_CLIENT_INLINE  __attribute__ ((visibility ("default")))
#  define VMW_LIB_DYNAMIC  __attribute__ ((visibility ("default")))
#  define VMW_LIB_DYNAMIC_INLINE  __attribute__ ((visibility ("default")))
#else
#  define VMW_LIB_STATIC
#  define VMW_LIB_CLIENT
#  define VMW_LIB_CLIENT_INLINE
#  define VMW_LIB_DYNAMIC
#  define VMW_LIB_DYNAMIC_INLINE
#endif /* _MSC_VER */

#endif /* VM_API_H */
