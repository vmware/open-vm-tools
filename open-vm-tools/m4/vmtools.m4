################################################################################
### Copyright (C) 2009-2019 VMware, Inc.  All rights reserved.
###
### VMware-specific macros for use with autoconf.
###
###
### This program is free software; you can redistribute it and/or modify
### it under the terms of version 2 of the GNU General Public License as
### published by the Free Software Foundation.
###
### This program is distributed in the hope that it will be useful,
### but WITHOUT ANY WARRANTY; without even the implied warranty of
### MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
### GNU General Public License for more details.
###
### You should have received a copy of the GNU General Public License
### along with this program; if not, write to the Free Software
### Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
################################################################################

#
# AC_VMW_CHECK_LIB(library, lvar, pkgname, lconfig, version, header, function,
#                  [action-if-found],
#                  [action-if-not-found])
#
# Checks for the existence of a library using three different methods, in the
# following order:
#
#     - user defined CUSTOM_(LIB)_CPPFLAGS and CUSTOM_(LIB)_LIBS variables.
#     - pkg-config
#     - the library's custom "config"
#
# If a library is successfully detected, the (LIB)_CPPFLAGS and (LIB)_LIBS
# variables are set to contain the appropriate flags, and these variables are
# exported using AC_SUBST. The parameters to the macro are:
#
#  library ($1): the library name; this is used for testing whether we can link
#                to the library (with AC_CHECK_LIB), and also to modify the user
#                provided CUSTOM_(LIB)_LIBS variable to make sure the library is
#                available in the linker flags.
#  lvar ($2): root name of the variables holding the libraries CPPFLAGS / LIBS;
#             e.g., FOO means that FOO_CPPFLAGS will be set, and CUSTOM_FOO_CPPFLAGS
#             used with AC_CHECK_HEADER.
#  pkgname ($3): pkg-config name of the library.
#  lconfig ($4): library's custom "config" program for figuring out compiler and
#                linker flags (optional).
#  version ($5): minimum version of the library when using pkg-config (optional).
#  header ($6): header file (when not using pkg-config; see AC_CHECK_HEADER; optional).
#  function ($7): function (when not using pkg-config; see AC_CHECK_LIB; optional).
#  action-if-found ($8): what to execute when successfully found the library.
#  action-if-not-found ($9): what to execute when the library was not found.
#
AC_DEFUN([AC_VMW_CHECK_LIB],[
   AC_REQUIRE([AC_CHECK_LIB]) dnl
   AC_REQUIRE([AC_CHECK_HEADER]) dnl

   if test -z "$1"; then
      AC_MSG_ERROR(['library' parameter is required.'])
   fi
   if test -z "$2"; then
      AC_MSG_ERROR(['lvar' parameter is required.'])
   fi

   ac_vmw_have_lib=0
   ac_vmw_have_lib_func=0
   ac_vmw_have_lib_header=0
   ac_vmw_custom_libs=

   #
   # First, try any user-defined CUSTOM_* flags.
   #
   if test -n "${CUSTOM_$2_CPPFLAGS}" || test -n "${CUSTOM_$2_LIBS}"; then
      ac_vmw_custom_libs="${CUSTOM_$2_LIBS} -l$1"
      if test -n "$6"; then
         ORIGINAL_CPPFLAGS="$CPPFLAGS"
         CPPFLAGS="${CUSTOM_$2_CPPFLAGS} $CPPFLAGS"

         AC_CHECK_HEADER([$6],
                         [ac_vmw_have_lib_header=1])

         CPPFLAGS="$ORIGINAL_CPPFLAGS"
      else
         ac_vmw_have_lib_header=1
      fi

      # Check a specific function in the library if requested.
      # If it hasn't, just pick a random function from libc, just to make
      # sure the linker can find the library being tested.
      if test $ac_vmw_have_lib_header -eq 1; then
         if test -n "$7"; then
            ac_vmw_function=$7
         else
            ac_vmw_function=strlen
         fi
         AC_CHECK_LIB(
            [$1],
            [$ac_vmw_function],
            [ac_vmw_have_lib_func=1],
            [],
            [$ac_vmw_custom_libs])
      fi

      if test $ac_vmw_have_lib_func -eq 1 && test $ac_vmw_have_lib_header -eq 1; then
         $2_CPPFLAGS="${CUSTOM_$2_CPPFLAGS}"
         $2_LIBS="$ac_vmw_custom_libs"
         ac_vmw_have_lib=1
      fi
   fi

   # If that didn't work, try with pkg-config.
   if test $ac_vmw_have_lib -eq 0 && test "$PKG_CONFIG" != "not_found" && test -n "$3"; then
      if test -n "$5"; then
         AC_MSG_CHECKING([for $3 >= $5 (via pkg-config)])
         if $PKG_CONFIG --exists '$3 >= $5'; then
            ac_vmw_have_lib=1
         fi
      else
         AC_MSG_CHECKING([for $3 (via pkg-config)])
         if $PKG_CONFIG --exists '$3'; then
            ac_vmw_have_lib=1
         fi
      fi

      if test $ac_vmw_have_lib -eq 1; then
         # Sometimes pkg-config might fail; for example, "pkg-config gtk+-2.0 --cflags"
         # fails on OpenSolaris B71. So be pessimistic.
         ac_vmw_cppflags="`$PKG_CONFIG --cflags $3`"
         ac_vmw_ret1=$?
         ac_vmw_libs="`$PKG_CONFIG --libs $3`"
         ac_vmw_ret2=$?
         if test $ac_vmw_ret1 -eq 0 && test $ac_vmw_ret2 -eq 0; then
            AC_MSG_RESULT([yes])
            $2_CPPFLAGS="$ac_vmw_cppflags"
            $2_LIBS="$ac_vmw_libs"
         else
            AC_MSG_RESULT([no])
         fi
      else
         AC_MSG_RESULT([no])
      fi
   fi

   # If we still haven't found the lib, try with the library's custom "config" script.
   if test $ac_vmw_have_lib -eq 0 && test -n "$4"; then
      unset ac_vmw_lib_cfg_$2
      AC_PATH_TOOL([ac_vmw_lib_cfg_$2], [$4], [not_found])
      if test "${ac_vmw_lib_cfg_$2}" != "not_found"; then
         # XXX: icu-config does not follow the "--cflags" and "--libs" convention,
         # so single it out here to avoid having to replicate all the rest of the
         # logic elsewhere.
         if test "$4" = "icu-config"; then
            $2_CPPFLAGS="`${ac_vmw_lib_cfg_$2} --cppflags`"
            $2_LIBS="`${ac_vmw_lib_cfg_$2} --ldflags`"
         else
            $2_CPPFLAGS="`${ac_vmw_lib_cfg_$2} --cflags`"
            $2_LIBS="`${ac_vmw_lib_cfg_$2} --libs`"
         fi
         ac_vmw_have_lib=1
      fi
   fi

   # Finish by executing the user provided action. The call to "true" is needed
   # because the actions are optional, and we need something inside the block.
   if test $ac_vmw_have_lib -eq 1; then
      AC_SUBST([$2_CPPFLAGS])
      AC_SUBST([$2_LIBS])
      true
      $8
   else
      true
      $9
   fi
])


#
# AC_VMW_CHECK_LIBXX(library, lvar, pkgname, lconfig, version, header, function,
#                    [action-if-found],
#                    [action-if-not-found])
#
# Similar to AC_VMW_CHECK_LIB, but for C++ libraries.
#
# XXX: Getting automake to choose between the C linker and the C++ linker
# depending on whether we're linking any C++ library was a royal pain in the
# ass. The classic way to do this is to define an optional source file for a
# program with an extension of .cxx, using nodist_EXTRA_fooprogram_SOURCES. This
# causes automake's linker detection algorithm to see a C++ source file and
# automatically set up the C++ linker and link line for us. Unfortunately, said
# linker detection doesn't obey conditionals, which means that it'd always pick
# the C++ linker, regardless of whether it's linking to a C++ library or not.
# Instead, we are forced to manually set the correct linker in fooprogram_LINK.
# However, since none of our programs actually contain C++ code, automake
# doesn't make the CXXLINK variable (which contains the linker as well as all
# link flags) available to us, so we must hard-code the entire link line into
# fooprogram_LINK. Not exactly a futureproof solution...
#
# Additional references on this problem:
# http://sources.redhat.com/ml/automake/1999-10/msg00101.html
# http://lists.gnu.org/archive/html/bug-automake/2008-04/msg00010.html
# http://www.gnu.org/software/automake/manual/automake.html#Libtool-Convenience-Libraries
# http://www.gnu.org/software/automake/manual/automake.html#C_002b_002b-Support
#
AC_DEFUN([AC_VMW_CHECK_LIBXX],[
   AC_REQUIRE([AC_VMW_CHECK_LIB])
   AC_LANG_PUSH([C++])
   AC_VMW_CHECK_LIB([$1], [$2], [$3], [$4], [$5], [$6], [$7], [$8], [$9])
   AC_LANG_POP([C++])
])


#
# AC_VMW_CHECK_X11_LIB(library, header, function, action-if-not-found)
#
# Special handling for X11 library checking. This macro checks that both the
# library provides the given function, and that the header exists, making use
# of COMMON_XLIBS when linking. On success, it modifies COMMON_XLIBS to include
# the library.
#
# library  ($1):   library name (value passed to ld with -l)
# header   ($2):   header file to look for, may be empty.
# function ($3):   function to look for in the library.
# action-if-not-found ($4): code to execute if failed to find the library.
#
#
AC_DEFUN([AC_VMW_CHECK_X11_LIB],[
   have_header=1
   if test -n "$2"; then
      AC_CHECK_HEADER(
         [X11/extensions/scrnsaver.h],
         [],
         [
          have_header=0;
          $4
         ],
         [])
   fi

   if test $have_header = 1; then
      AC_CHECK_LIB(
         [$1],
         [$3],
         [COMMON_XLIBS="-l$1 $COMMON_XLIBS"],
         [$4],
         [$COMMON_XLIBS])
   fi
])


#
# AC_VMW_LIB_ERROR(library, disable)
#
# Wrapper around AC_MSG_ERROR to print a standard message about missing libraries.
#
#     library ($1): name of missing library.
#     disable ($2): configure argument to disable usage of the library.
#     feature ($3): optional name of feature to be disabled; defaults to 'library'.
#
AC_DEFUN([AC_VMW_LIB_ERROR],[
   feature="$3"
   if test -z "$feature"; then
      feature="$1"
   fi
   AC_MSG_ERROR([Cannot find $1 library. Please configure without $feature (using --without-$2), or install the $1 libraries and devel package(s).])
])


#
# AC_VMW_DEFAULT_FLAGS(library)
#
# For use with libraries that don't have config scripts or pkg-config data.
# This makes sure that CUSTOM_${LIB}_CPPFLAGS is set to a reasonable default
# so that AC_VMW_CHECK_LIB can find the library.
#
#     library ($1): library name (as in CUSTOM_${library}_CPPFLAGS)
#     subdir  ($2): optional subdirectory to append to the default path
#
AC_DEFUN([AC_VMW_DEFAULT_FLAGS],[
   if test -z "$CUSTOM_$1_CPPFLAGS"; then
      if test "$os" = freebsd; then
         CUSTOM_$1_CPPFLAGS="-I/usr/local/include"
      else
         CUSTOM_$1_CPPFLAGS="-I/usr/include"
      fi
      if test -n "$2"; then
         CUSTOM_$1_CPPFLAGS="${CUSTOM_$1_CPPFLAGS}/$2"
      fi
   fi
])

