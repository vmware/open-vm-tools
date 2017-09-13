/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * mspackConfig.h --
 *
 *      Definitions of mspack constants.
 */

/*
 * This file is a version of the auto generated code. This has a bunch of
 * #definitions which are required to make the mspack library compatible with
 * the client scenario. An alternative is to use define HAVE_CONFIG_H and use
 * the auto generated file.
 *
 * The ownership has been taken into our codebase in order to be able to compile
 * for different flavors of linux/unix using -target in gcc.
 */

// Define to 1 if you have the `alarm' function.
#define HAVE_ALARM 1

// Define to 1 if you have `alloca', as a function or macro.
#define HAVE_ALLOCA 1

// Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
#define HAVE_ALLOCA_H 1

// Define to 1 if you have the <ctype.h> header file.
#define HAVE_CTYPE_H 1

// Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
#define HAVE_DIRENT_H 1

// Define to 1 if you have the <errno.h> header file.
#define HAVE_ERRNO_H 1

// Define to 1 if your system has a working POSIX `fnmatch' function.
#define HAVE_FNMATCH 1

// Define to 1 if you have the <fnmatch.h> header file.
#define HAVE_FNMATCH_H 1

// Define to 1 if fseeko (and presumably ftello) exists and is declared.
#define HAVE_FSEEKO 1

// Define to 1 if you have the <getopt.h> header file.
#define HAVE_GETOPT_H 1

// Define to 1 if you have the `getopt_long' function.
#define HAVE_GETOPT_LONG 1

// Define to 1 if you have the <inttypes.h> header file.
#define HAVE_INTTYPES_H 1

// Define to 1 if you have the <libintl.h> header file.
#define HAVE_LIBINTL_H 1

// Define to 1 if you have the <limits.h> header file.
#define HAVE_LIMITS_H 1

// Define to 1 if <wchar.h> declares mbstate_t.
#define HAVE_MBSTATE_T 1

// Define to 1 if you have the `memcpy' function.
#define HAVE_MEMCPY 1

// Define to 1 if you have the <memory.h> header file.
#define HAVE_MEMORY_H 1

// Define to 1 if you have the `mempcpy' function.
#define HAVE_MEMPCPY 1

// Define to 1 if you have the <stdarg.h> header file.
#define HAVE_STDARG_H 1

// Define to 1 if you have the <stdint.h> header file.
#define HAVE_STDINT_H 1

// Define to 1 if you have the <stdlib.h> header file.
#define HAVE_STDLIB_H 1

// Define to 1 if you have the `strcasecmp' function.
#define HAVE_STRCASECMP 1

// Define to 1 if you have the `strchr' function.
#define HAVE_STRCHR 1

// Define to 1 if you have the <strings.h> header file.
#define HAVE_STRINGS_H 1

// Define to 1 if you have the <string.h> header file.
#define HAVE_STRING_H 1

// Define to 1 if you have the <sys/stat.h> header file.
#define HAVE_SYS_STAT_H 1

// Define to 1 if you have the <sys/time.h> header file.
#define HAVE_SYS_TIME_H 1

// Define to 1 if you have the <sys/types.h> header file.
#define HAVE_SYS_TYPES_H 1

// Define to 1 if you have the <unistd.h> header file.
#define HAVE_UNISTD_H 1

// Define to 1 if you have the `utime' function.
#define HAVE_UTIME 1

// Define to 1 if you have the `utimes' function.
#define HAVE_UTIMES 1

// Define to 1 if you have the <utime.h> header file.
#define HAVE_UTIME_H 1

// Define to 1 if you have the <wchar.h> header file.
#define HAVE_WCHAR_H 1

// Define to 1 if you have the <wctype.h> header file.
#define HAVE_WCTYPE_H 1

/*
 * PACKAGE, PACKAGE_BUGREPORT, PACKAGE_NAME, PACKAGE_STRING, PACKAGE_TARNAME,
 * PACKAGE_VERSION and VERSION should not be defined here, since they will be in
 * conflict with open-vm-tools definitions.
 */

// Define to 1 if you have the ANSI C header files.
#define STDC_HEADERS 1

// Define to 1 if you can safely include both <sys/time.h> and <time.h>.
#define TIME_WITH_SYS_TIME 1

// Number of bits in a file offset, on hosts where this is settable.
#define _FILE_OFFSET_BITS 64

// Define to 1 to make fseeko visible on some hosts (e.g. glibc 2.2).
#define _LARGEFILE_SOURCE 1

