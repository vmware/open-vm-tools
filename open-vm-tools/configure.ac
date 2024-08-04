################################################################################
### Copyright (c) 2007-2024 Broadcom. All Rights Reserved.
### The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
###
### Configure script for building the VMware OSS Tools.
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

################################################################################
# In addition to the usual environment variables and command line
# arguments that a configure script usually takes (CFLAGS, LDFLAGS,
# etc.), this script also accepts variables of the form:
#
#  CUSTOM_LIB_CPPFLAGS: compile-time flags for LIB
#  CUSTOM_LIB_LIBS: link-time flags for LIB
#  RPCGENFLAGS: extra flags to pass to rpcgen
#
# The following libraries are currently tested: DNET, FUSE, GLIB2, GMODULE,
# GOBJECT, GTHREAD, GTK, GTKMM, ICU, LIBPNG, PAM, URIPARSER, ZLIB
################################################################################

###
### Initialization
###

TOOLS_VERSION="12.4.5"
AC_INIT(
   [open-vm-tools],
   [12.4.5],
   [open-vm-tools-devel@lists.sourceforge.net])

# In order to make this configure script auto-detect situations where
# people have a 32-bit userland running with a 64-bit kernel, we try to ask
# the compiler (assumedly gcc) for its default Target:.
# We have to set up $TEST_CC manually, since AC_PROG_CC hasn't yet been run (and can't be until AC_CANONICAL_HOST & AC_CANONICAL_BUILD are run)
# The purpose of all this is to set up $host_alias/$build_alias in a more
# intelligent way than config.guess currently does.
TEST_CC="$CC_FOR_BUILD"
test -z "$TEST_CC" && TEST_CC="$HOST_CC"
test -z "$TEST_CC" && TEST_CC="$CC"
if test -n "$TEST_CC" -a -z "$host_alias"; then
   host_alias="`$TEST_CC -dumpmachine`"
   if test -z "$build_alias" -a -n "$host_alias"; then
      build_alias="$host_alias"
   fi
fi
unset TEST_CC

# checkvm/checkvm.c has no special significance - we just need to pass in a file that
# helps autoconf verify that it really has found the source tree.
AC_CONFIG_SRCDIR([checkvm/checkvm.c])

# Keep the top-level directory tidy by putting auxiliary build tools and local
# macros in separate subdirectories.
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_HOST
AC_CANONICAL_BUILD

# Quote the regular expressions
case "$host_cpu" in
   [i[3456]86])
      arch="x32"
      ;;
   [amd64|x86_64])
      arch="x64"
      ;;
   [aarch64])
      arch="arm64"
      ;;
   [*])
      AC_MSG_ERROR([Unknown architecture.])
      ;;
esac

# Operational arguments.
AC_ARG_WITH([root-privileges],
	    [AS_HELP_STRING([--without-root-privileges],
	       [does not perform any operations that require root privileges])],
	    [],
	    [with_root_privileges=yes])

# Kernel arguments.
# The kernel args have to go here otherwise the KERNEL_RELEASE won't be visible
# to getOsVersion()
AC_ARG_WITH([kernel-release],
	    [AS_HELP_STRING([--with-kernel-release],
		[specifies the kernel release you want to build against])],
	    [KERNEL_RELEASE="$withval"],
	    [KERNEL_RELEASE=`uname -r`])

AC_ARG_WITH([linuxdir],
	    [AS_HELP_STRING([--with-linuxdir],
		[specifies the Linux directory you want to use])],
	    [LINUXDIR="$withval"],
	    [LINUXDIR=/lib/modules/$KERNEL_RELEASE])

# Turn the uname output into something we can run comparisons on.
getOsVersion() {
   major_version="`echo $KERNEL_RELEASE | cut -f1 -d. | cut -f1 -d-`"
   minor_version="`echo $KERNEL_RELEASE | cut -f2 -d. | cut -f1 -d-`"
   micro_version="`echo $KERNEL_RELEASE | cut -f3 -d. | cut -f1 -d-`"
   printf '%02d%02d%03d' $major_version $minor_version $micro_version
}

case "$host_os" in
   [linux*])
      os="linux"
      ;;
   [freebsd*])
      os="freebsd"
      ;;
   [kfreebsd*-gnu])
      os="kfreebsd-gnu"
      ;;
   [solaris*])
      os="solaris"
      ;;
   [*])
      AC_MSG_WARN([This is an untested and unsupported Operating System. Proceed at your own peril.])
      ;;
esac
osVersion="`getOsVersion`"

AC_ARG_ENABLE(
   glibc-check,
   AS_HELP_STRING(
      [--disable-glibc-check],
      [Skip checking the glibc version on the system. (off by default)]),
   [enable_glibc_check=$enableval],
   [enable_glibc_check="yes"])

if test "x$enable_glibc_check" != "xno" ; then
   if test "x$os" = "xlinux" ; then
      libc_ver=`ldd --version 2>/dev/null | head -1 | sed 's/.* //;q'`

      if test "x$libc_ver" = "x"; then
         AC_MSG_ERROR(["Failed to detect glibc version installed in the system."])
      fi

      libc_ver_mjr=`expr "$libc_ver" : "\([[0-9]]*\)"`
      libc_ver_mnr=`expr "$libc_ver" : "[[0-9]]*\.\([[0-9]]*\)"`
      libc_ver_num=`expr $libc_ver_mjr \* 1000 + $libc_ver_mnr`

      if test "$libc_ver_num" -lt 2011; then
         AC_MSG_ERROR(["glibc version $libc_ver detected.
This version of open-vm-tools requires glibc version 2.11 or later.
Please use an older version of open-vm-tools for this system."])
      fi
   fi
fi

if test "x$os" = "xfreebsd" -a "$osVersion" -lt 1000000; then
   AC_MSG_ERROR(["This version of open-vm-tools requires FreeBSD version 10 or later.
Please use an older version of open-vm-tools for this system."])
fi

AC_ARG_WITH([kernel-modules],
	    [AS_HELP_STRING([--with-kernel-modules],
		[compile and install the kernel modules])],
	    [],
	    [if test "x$os" = "xlinux" ; then
	        with_kernel_modules=no
	     else
	        with_kernel_modules=yes
	     fi
	    ])

if test "$with_kernel_modules" = "yes"; then
   case "$os" in
      linux)
         AC_MSG_ERROR([Building kernel modules for Linux is no longer supported.])
         ;;
      freebsd)
         freebsd_sysdir=/usr/src/sys
         if test -n "$SYSDIR"; then
            freebsd_sysdir="$SYSDIR"
         fi
         if test ! -f "$freebsd_sysdir/conf/kmod.mk"; then
            AC_MSG_ERROR([FreeBSD kernel tree not found. Please install the kernel sources (or provide the location using SYSDIR) or configure using --without-kernel-modules.])
         fi
         ;;
   esac
fi

# Locates X includes and lib, adds --without-x option,
# and sets no_x.
AC_PATH_XTRA

AC_CHECK_HEADER([valgrind/valgrind.h],
                [],
                [have_valgrind=no],
                [])

AC_ARG_ENABLE(
   valgrind,
   AS_HELP_STRING(
      [--enable-valgrind],
      [enables building with valgrind]),
   [enable_valgrind="$enableval"],
   [enable_valgrind="no"])

if test "$enable_valgrind" = "yes" ; then
   if test "$have_valgrind" = "no" ; then
      AC_MSG_ERROR([valgrind/valgrind.h not found. Make sure you have the valgrind headers installed.]),
   fi
   CPPFLAGS="$CPPFLAGS -DUSE_VALGRIND=1"
fi

# Arguments for disabling individual open-vm-tools features or libraries.
AC_ARG_ENABLE(
   multimon,
   AS_HELP_STRING(
      [--disable-multimon],
      [disables multimon, enabled by default]),
   [enable_multimon="$enableval"],
   [enable_multimon="yes"])

AC_ARG_WITH(
   gtk3,
   AS_HELP_STRING(
      [--without-gtk3],
      [compiles without Gtk 3.0]),
   [with_gtk3="$withval"],
   [with_gtk3="auto"])

AC_ARG_WITH(
   gtk2,
   AS_HELP_STRING(
      [--without-gtk2],
      [compiles without Gtk 2.0]),
   [with_gtk2="$withval"],
   [with_gtk2="auto"])


if test "$no_x" = "yes" ; then
   with_gtk2="no"
   with_gtk3="no"
else
   if test "$with_gtk2" = "auto" ; then
      if test "$with_gtk3" = "auto" ; then
         with_gtk2="no"
         with_gtk3="yes"
      elif test "$with_gtk3" = "no" ; then
         with_gtk2="yes"
      elif test "$with_gtk3" = "yes" ; then
         with_gtk2="no"
      fi
   elif test "$with_gtk2" = "no" ; then
      if test "$with_gtk3" = "auto" ; then
         with_gtk3="yes"
      elif test "$with_gtk3" = "no" ; then
         AC_MSG_ERROR('need either gtk2 or gtk3 or build with --without-x to disable building desktop plugins')
      fi
   elif test "$with_gtk2" = "yes"  ; then
      if test "$with_gtk3" = "auto" ; then
         with_gtk3="no"
      elif test "$with_gtk3" = "yes" ; then
         AC_MSG_ERROR('cannot set both --with-gtk2 and --with-gtk3')
      fi
   fi
fi

if test "$with_gtk2" = "no" ; then
   with_gtkmm="no"
fi

if test "$with_gtk3" = "no" ; then
   with_gtkmm3="no"
fi

if test "$with_gtk3" = "yes"; then
   AC_ARG_WITH(
      gtkmm3,
      AS_HELP_STRING(
         [--without-gtkmm3],
         [compiles without Gtkmm 3, sigc++, and related libs]),
      [with_gtkmm3="$withval"],
      [with_gtkmm3="yes"])
      with_gtkmm="no"
elif test "$with_gtk2" = "yes"; then
   AC_ARG_WITH(
      gtkmm,
      AS_HELP_STRING(
         [--without-gtkmm],
         [compiles without Gtkmm, sigc++, and related libs]),
      [with_gtkmm="$withval"],
      [with_gtkmm="yes"])
      with_gtkmm3="no"
fi

AC_ARG_ENABLE(
   docs,
   AS_HELP_STRING(
      [--disable-docs],
      [disables generation of API documentation; by default, docs are built if doxygen is available.]),
   [enable_docs="$enableval"],
   [enable_docs="yes"])

AC_ARG_ENABLE(
   tests,
   AS_HELP_STRING(
      [--disable-tests],
      [disable compilation of test code.]),
   [enable_tests="$enableval"],
   [enable_tests="auto"])

AC_ARG_ENABLE(
   resolutionkms,
   AS_HELP_STRING(
      [--enable-resolutionkms],
      [build the linux / unix resolutionkms module.]),
   [],
   [enable_resolutionkms="auto"])


AC_ARG_ENABLE(
   vmwgfxctrl,
   AS_HELP_STRING(
      [--enable-vmwgfxctrl],
      [build the linux / unix vmwgfxctrl tool.]),
   [],
   [enable_vmwgfxctrl="auto"])

AM_INIT_AUTOMAKE

###
### Constants
###
# These need to be declared after initialization.

# Some of our macro call-sites require changes to
# CPPFLAGS/CFLAGS/LDFLAGS. In such places, we save the original value
# of CPPFLAGS/CFLAGS/LDFLAGS before the macro call and restore it when
# the call is done. We must perform this save at each macro site,
# because CPPFLAGS/CFLAGS/LDFLAGS may change over the course of
# configuration.
#
# CPPFLAGS is intended for preprocessor options (-D and -I mainly)
# CFLAGS is intended for compiler options (-O, -f, -W, and so forth)

CPPFLAGS="$CPPFLAGS -DUSING_AUTOCONF=1 -DOPEN_VM_TOOLS"

###
### Programs
###
# C preprocessor and compiler.
AC_PROG_CPP
AC_PROG_CC

# C++ compiler. Note that unlike AC_PROG_CC, this call does not trigger an
# error if no C++ compiler was found; it'll just set the variable CXX to 'g++'.
AC_PROG_CXX

# This allows features like per-target compiler flags.  I.e., you can compile
# one copy of the same sources twice with different flags.  (See lib/guestApp
# for an example.)
AM_PROG_CC_C_O

# Needed for the various install and uninstall hooks.
AC_PROG_INSTALL
AC_PROG_SED
AC_PROG_LN_S
AC_PROG_MKDIR_P

# Needed for creating the archives in lib/ and the shared libraries.
AC_PROG_LIBTOOL
if test "$ac_cv_prog_AR" = false; then
   AC_MSG_ERROR([The 'ar' utility was not found. Please put ar on the path.])
fi

# We use pkg-config to set up the cflags and libs for gtk.
AC_CHECK_TOOL(
   [PKG_CONFIG],
   [pkg-config],
   [not_found])

if test "$GCC" != "yes"; then
   AC_MSG_ERROR([Only GCC is currently supported. Please put gcc in the path.])
fi

###
### Libraries
###

#
# Check for libintl.h. When configuring using "--without-x", /usr/local/include
# may not be added to the include path, so code that use glib's i18n functions
# would fail to compile because it can't find libintl.h.
#
AC_CHECK_HEADER([libintl.h],
                [],
                [have_libintl=no],
                [])
if test "$have_libintl" = "no"; then
   unset ac_cv_header_libintl_h
   CPPFLAGS="$CPPFLAGS -I/usr/local/include"
   AC_CHECK_HEADER([libintl.h],
                   [],
                   [AC_MSG_ERROR([libintl.h not found. Make sure you have the gettext headers installed.])],
                   [])
fi

AC_ARG_ENABLE([deploypkg],
   [AS_HELP_STRING([--disable-deploypkg],
     [do not build deploypkg plugin.])],
   [],
   [
    if test "$os" = "linux"; then
       enable_deploypkg=yes
    else
       enable_deploypkg=no
    fi
   ])

if test "$enable_deploypkg" = "yes"; then
  AC_VMW_CHECK_LIB([mspack],
                   [MSPACK],
                   [libmspack],
                   [],
                   [0.0.20040308alpha],
                   [mspack.h],
                   [mspack_version],
                   [],
                   [AC_MSG_ERROR([mspack >= 0.0.20040308alpha is required.])])
fi

AC_ARG_ENABLE([libappmonitor],
   [AS_HELP_STRING([--enable-libappmonitor],
     [Build the libappmonitor library.])],
   [],
   [
    enable_libappmonitor=no
   ])

AC_ARG_ENABLE([servicediscovery],
   [AS_HELP_STRING([--enable-servicediscovery],
     [Build the Service Discovery Plugin.])],
   [
    enable_servicediscovery=$enableval
   ],
   [
    enable_servicediscovery=no
   ])

#
# Check that servicediscovery is enabled only on linux systems
#

if test "$enable_servicediscovery" = "yes"; then
   if test "$os" != "linux"; then
      AC_MSG_ERROR([The Service Discovery plugin is only supported for Linux
      platforms. Try configure without --enable-servicediscovery option.])
   fi
fi

AC_ARG_ENABLE([salt-minion],
   [AS_HELP_STRING([--enable-salt-minion],
     [Install salt_minion related script files.])],
   [
    enable_saltminion=$enableval
   ],
   [
    enable_saltminion=no
   ])

#
# Check that salt_minion is enabled only on linux systems
#

if test "$enable_saltminion" = "yes"; then
   if test "$os" != "linux"; then
      AC_MSG_ERROR([The salt_minion package is only supported for Linux
      platforms. Try configure without --enable-salt-minion option.])
   fi
fi

#
# Check for glib 2.34.0 or greater.
#
AC_VMW_CHECK_LIB([glib-2.0],
                 [GLIB2],
                 [glib-2.0],
                 [],
                 [2.34.0],
                 [glib.h],
                 [g_key_file_new],
                 [],
                 [AC_MSG_ERROR([glib2 >= 2.34.0 is required.])])

AC_VMW_CHECK_LIB([gmodule-2.0],
                 [GMODULE],
                 [gmodule-2.0],
                 [],
                 [2.34.0],
                 [],
                 [g_module_open],
                 [],
                 [AC_MSG_ERROR([gmodule >= 2.34.0 is required.])])
AC_VMW_CHECK_LIB([gobject-2.0],
                 [GOBJECT],
                 [gobject-2.0],
                 [],
                 [2.34.0],
                 [glib-object.h],
                 [g_object_new],
                 [],
                 [AC_MSG_ERROR([gobject >= 2.34.0 is required.])])
AC_VMW_CHECK_LIB([gthread-2.0],
                 [GTHREAD],
                 [gthread-2.0],
                 [],
                 [2.34.0],
                 [],
                 [g_thread_new],
                 [],
                 [AC_MSG_ERROR([gthread >= 2.34.0 is required.])])
AC_CHECK_PROG([have_genmarshal],
              [glib-genmarshal],
              [yes],
              [no])

if test "$have_genmarshal" != "yes"; then
   AC_MSG_ERROR([glib-genmarshal is required; make sure it is available in your path.])
fi

#
# Check for fuse.
#

AC_ARG_WITH([fuse],
   [AS_HELP_STRING([--without-fuse],
     [compiles without FUSE support (disables vmblock-fuse/vmhgfs-fuse).])],
   [with_fuse="$withval"],
   [with_fuse=auto])

case "$with_fuse" in
   fuse|2)
      with_fuse="fuse"
      ;;
   fuse3|3)
      with_fuse="fuse3"
      ;;
   auto)
      ;;
   yes)
      ;;
   no)
      have_fuse3=no;
      have_fuse=no;
      ;;
   *)
      AC_MSG_FAILURE([--with-fuse was given with an unsupported paramter $with_fuse])
      ;;
esac


if test "$with_fuse" = "auto" ||
   test "$with_fuse" = "fuse3" ||
   test "$with_fuse" = "yes"; then
   #
   # Check for fuse3.
   #
   # FUSE_USE_VERSION sets the version of the FUSE API that will be exported.
   AC_VMW_CHECK_LIB([fuse3],
                    [FUSE3],
                    [fuse3],
                    [],
                    [3.10.0],
                    [fuse3/fuse.h],
                    [fuse_main],
                    [have_fuse3=yes;
                     AC_DEFINE([HAVE_FUSE3], 1, [Define to 1 if using FUSE3.])
                     AC_DEFINE([FUSE_USE_VERSION], 35, [FUSE API version to use.])],
                    [have_fuse3=no])

   if test "$have_fuse3" = "no"; then
      if test "$with_fuse" = "auto" || test "$with_fuse" = "yes"; then
         AC_MSG_NOTICE([Fuse3 is missing, trying to use older Fuse library.])
      else
         AC_MSG_FAILURE([Fuse3 was requested but unavailable on the system.])
      fi
   fi
fi

if test "$with_fuse" = "fuse" ||
   ( ( test "$with_fuse" = "auto" || test "$with_fuse" = "yes" ) &&
     test "$have_fuse3" = "no" ); then
   #
   # Check for fuse.
   #
   # FUSE_USE_VERSION sets the version of the FUSE API that will be exported.
   AC_VMW_CHECK_LIB([fuse],
                    [FUSE],
                    [fuse],
                    [],
                    [],
                    [fuse.h],
                    [fuse_main],
                    [have_fuse=yes;
                     AC_DEFINE([HAVE_FUSE], 1, [Define to 1 if using FUSE.])
                     AC_DEFINE([FUSE_USE_VERSION], 29, [FUSE API version to use.])],
                    [have_fuse=no])

   if test "$have_fuse" = "no"; then
      if test "$with_fuse" = "auto" || test "$with_fuse" = "yes"; then
	 AC_MSG_NOTICE([Fuse is missing, vmblock-fuse/vmhgfs-fuse will be disabled.])
      else
         AC_MSG_FAILURE([Fuse2 was requested but unavailable on the system.])
      fi
   fi
fi

if test "$with_fuse" = "no"; then
   AC_MSG_WARN([Fuse or Fuse3 is suppressed, vmblock-fuse/vmhgfs-fuse will be disabled.])
fi

#
# Check for PAM.
#
AC_ARG_WITH([pam],
   [AS_HELP_STRING([--without-pam],
     [compiles without PAM support (disables vgauth).])],
   [with_pam="$withval"],
   [with_pam=yes])

if test "$with_pam" = "yes"; then
   AC_VMW_DEFAULT_FLAGS([PAM])
   AC_VMW_CHECK_LIB([pam],
                    [PAM],
                    [],
                    [],
                    [],
                    [security/pam_appl.h],
                    [pam_start],
                    [PAM_CPPFLAGS="$PAM_CPPFLAGS -DUSE_PAM"],
                    [AC_VMW_LIB_ERROR([PAM], [pam])])
fi

AC_ARG_ENABLE([containerinfo],
   [AS_HELP_STRING([--disable-containerinfo],
     [do not build containerinfo plugin.])],
   [
     enable_containerinfo=$enableval
   ],
   [
       if test "$os" = "linux"; then
          enable_containerinfo=auto
       else
          enable_containerinfo=no
       fi
   ])

#
# Check that containerinfo plugin is enabled only on linux systems.
#
if test "$os" != "linux"; then
   if test "$enable_containerinfo" = "yes"; then
      AC_MSG_ERROR([The containerinfo plugin is only supported for Linux
      platforms. Try configure with --disable-containerinfo option.])
   fi
fi

if test "$enable_containerinfo" = "yes" ||
   test "$enable_containerinfo" = "auto"; then

can_build_containerinfo=yes

#
# AC_VMW_CONTAINERINFO_MSG(library)
#
# Wrapper around AC_MSG_WARN to print a standard message about missing libraries.
#
#     library ($1): name of missing library / package.
#
AC_DEFUN([AC_VMW_CONTAINERINFO_MSG],[
   can_build_containerinfo=no
   AC_MSG_WARN(["$1 is missing which is required for building containerinfo plugin."])
])

   AC_VMW_DEFAULT_FLAGS([CURL])
   AC_VMW_CHECK_LIB([curl],
                    [CURL],
                    [],
                    [],
                    [],
                    [curl/curl.h],
                    [curl_easy_init],
                    [CURL_CPPFLAGS="$CURL_CPPFLAGS"],
                    [AC_VMW_CONTAINERINFO_MSG([CURL])])

   AC_VMW_CHECK_LIB([protobuf],
                    [PROTOBUF],
                    [protobuf],
                    [],
                    [3.0.0],
                    [],
                    [],
                    [],
                    [AC_VMW_CONTAINERINFO_MSG(["protobuf >= 3.0.0"])])

   AC_VMW_DEFAULT_FLAGS([GRPC])
   AC_VMW_CHECK_LIBXX([grpc++],
                      [GRPC],
                      [grpc++],
                      [],
                      [1.3.2],
                      [grpc++/grpc++.h],
                      [],
                      [PKG_CHECK_MODULES([grpcxx], [grpc++ >= 1.3.2])],
                      [AC_VMW_CONTAINERINFO_MSG(["grpc++ >= 1.3.2"])])

#
# proto files needed by containerd grpc client.
#
# Installation location varies between Linux vendors.
#   Canonical, Ubuntu and Debian: in /usr/share/gocode/src
#   openSUSE/SUSE: in /usr/share/go/<version>/contrib/src
#
   for d in /usr/share/gocode/src /usr/share/go/1.*/contrib/src; do
       if test -d "$d"/github.com; then
           src_prefix="$d"
           break
       fi
   done
   shared_prefix=$src_prefix/github.com
   AC_SUBST(TYPES_DIR, github.com/containerd/containerd/api/types)
   AC_SUBST(TASKS_PROTOPATH, $shared_prefix/containerd/containerd/api/services/tasks/v1)
   AC_SUBST(DEP_PROTOPATH, $src_prefix)
   AC_SUBST(CONTAINERD_PROTOPATH, $shared_prefix/containerd/containerd/api/services/containers/v1)
   AC_SUBST(GOGO_PROTOPATH, $shared_prefix/gogo/protobuf)
   AC_CHECK_FILE([${CONTAINERD_PROTOPATH}/containers.proto],
                 [],
                 [AC_VMW_CONTAINERINFO_MSG(["containerd package"])])
   AC_CHECK_FILE([${TASKS_PROTOPATH}/tasks.proto],
                 [],
                 [AC_VMW_CONTAINERINFO_MSG(["containerd package"])])
   AC_CHECK_FILE([${DEP_PROTOPATH}/${TYPES_DIR}/mount.proto],
                 [],
                 [AC_VMW_CONTAINERINFO_MSG(["containerd package"])])
   AC_CHECK_FILE([${DEP_PROTOPATH}/${TYPES_DIR}/metrics.proto],
                 [],
                 [AC_VMW_CONTAINERINFO_MSG(["containerd package"])])
   AC_CHECK_FILE([${DEP_PROTOPATH}/${TYPES_DIR}/descriptor.proto],
                 [],
                 [AC_VMW_CONTAINERINFO_MSG(["containerd package"])])
   AC_CHECK_FILE([${DEP_PROTOPATH}/${TYPES_DIR}/task/task.proto],
                 [],
                 [AC_VMW_CONTAINERINFO_MSG(["containerd package"])])
   AC_CHECK_FILE([${GOGO_PROTOPATH}/gogoproto/gogo.proto],
                 [],
                 [AC_VMW_CONTAINERINFO_MSG(["gogoproto package"])])

#
# Binaries needed to build for containerd grpc client.
#
   AC_CHECK_PROG([GRPC_CPP], [grpc_cpp_plugin], [grpc_cpp_plugin], [not found])

   if test "$GRPC_CPP" != "grpc_cpp_plugin"  ; then
      AC_VMW_CONTAINERINFO_MSG(["grpc_cpp_plugin binary"])
   fi

   AC_CHECK_PROG([PROTOC], [protoc], [protoc], [not found])

   if test "$PROTOC" != "protoc"  ; then
      AC_VMW_CONTAINERINFO_MSG(["protoc binary"])
   fi

   if test "$can_build_containerinfo" = "no" ; then
      if test "$enable_containerinfo" = "auto" ; then
         enable_containerinfo=no
         AC_MSG_WARN(["Cannot enable containerinfo plugin since one ore more required packages are missing."])
      else
         AC_MSG_ERROR(["Cannot enable containerinfo plugin since one or more required packages are missing. Please configure without containerinfo (using --disable-containerinfo), or install the necessary libraries and devel package(s)."])
      fi
   else
      enable_containerinfo=yes
   fi
fi

AC_ARG_ENABLE([vgauth],
   [AS_HELP_STRING([--disable-vgauth],
     [do not build vgauth])],
   [
    if test "$with_pam" = "no" -a "$enableval" = "yes"; then
       AC_MSG_ERROR([Cannot enable vgauth without PAM. Please configure
                     without --without-pam or without --enable-vgauth.])
    fi
    enable_vgauth="$enableval"
   ],
   [
    if test "$with_pam" = "no"; then
       enable_vgauth="no"
       AC_MSG_WARN("Building without PAM; vgauth will be disabled.")
    elif test "$os" = "linux"; then
       enable_vgauth=yes
    else
       enable_vgauth=no
    fi
   ])


#
# Check for openssl, xmlsec1, xml2
#
AC_ARG_WITH([ssl],
   [AS_HELP_STRING([--without-ssl],
     [compiles without openssl support (disables vgauth)])],
   [if test "$withval" = "no"; then enable_vgauth = "no"; fi],
   [with_ssl="yes"])

AC_ARG_WITH([xmlsec1],
   [AS_HELP_STRING([--without-xmlsec1],
     [compiles without xmlsec1 support (disables vgauth)])],
   [if test "$withval" = "no"; then enable_vgauth = "no"; fi],
   [with_xmlsec1="yes"])

AC_ARG_WITH([xml2],
   [AS_HELP_STRING([--without-xml2],
     [compiles without xml2 support (disables vgauth)])],
   [if test "$withval" = "no"; then enable_vgauth = "no"; fi],
   [with_xml2="yes"])

AC_ARG_WITH([tirpc],
   [AS_HELP_STRING([--without-tirpc],
     [compiles with/without libtirpc])],
   [],
   [with_tirpc=auto])

# Make sure we are building with openssl 1.0.1 and above so that
# we use only TLSv1_2.

if test "$enable_vgauth" = "yes" ; then
   AC_VMW_DEFAULT_FLAGS([SSL])
   AC_VMW_CHECK_LIB([ssl],
                    [SSL],
                    [openssl],
                    [],
                    [1.0.1],
                    [],
                    [BIO_new_file],
                    [],
                    [AC_VMW_LIB_ERROR([SSL], [ssl])])

   CPPFLAGS="$CPPFLAGS -DUSE_VGAUTH"
   AC_VMW_DEFAULT_FLAGS([XML2])
   AC_VMW_CHECK_LIB([xml2],
                     [XML2],
                     [],
                     [],
                     [],
                     [],
                     [],
                     [],
                     [AC_VMW_LIB_ERROR([XML2], [xml2])])

# Multiple distros built xmlsec1 with -DXMLSEC_NO_SIZE_T but
# their xmlssec1-config --cflags doesn't properly report it.
# Force it on following the xmlSecSize changelog in below link:
# https://www.aleksey.com/xmlsec/news.html
# The xmlsec configure flag "enable_size_t" is "yes" by default
# since 1.3.0, and both the configure flag and CPP flag has been
# deprecated since 1.3.3, which means the size_t is used by default
# and no need to add CPP flag -DXMLSEC_NO_SIZE_T since 1.3.0.
   AC_VMW_DEFAULT_FLAGS([XMLSEC1])
   AC_VMW_CHECK_LIB([xmlsec1],
                     [XMLSEC1],
                     [xmlsec1],
                     [xmlsec1-config],
                     [],
                     [xmlsec/xmlsec.h],
                     [xmlSecCheckVersionExt],
                     [XMLSEC1_VER=`$PKG_CONFIG --modversion xmlsec1`
                      xmlsec1_major_version="`echo $XMLSEC1_VER | cut -f1 -d. | cut -f1 -d-`"
                      xmlsec1_minor_version="`echo $XMLSEC1_VER | cut -f2 -d. | cut -f1 -d-`"
                      xmlsec1_micro_version="`echo $XMLSEC1_VER | cut -f3 -d. | cut -f1 -d-`"
                      xmlsec1_version=`printf '%02d%02d%02d' $xmlsec1_major_version $xmlsec1_minor_version $xmlsec1_micro_version`
                      AC_CHECK_SIZEOF(size_t)
                      if test "$xmlsec1_version" -lt 010300 -a "$ac_cv_sizeof_size_t" -ne 4 ; then
                         XMLSEC1_CPPFLAGS="$XMLSEC1_CPPFLAGS -DXMLSEC_NO_SIZE_T"
                      fi
                     ],
                     [AC_VMW_LIB_ERROR([XMLSEC1], [xmlsec1])])
fi

#
# Check for CUnit and disable test code if not available.
#
if test "$enable_tests" = "auto" -o "$enable_tests" = "yes"; then
   AC_VMW_DEFAULT_FLAGS([CUNIT])
   AC_VMW_CHECK_LIB([cunit],
                    [CUNIT],
                    [],
                    [],
                    [],
                    [CUnit/CUnit.h],
                    [CU_initialize_registry],
                    [have_cunit=yes],
                    [have_cunit=no])
   if test "$have_cunit" = "no"; then
      if test "$enable_tests" = "yes"; then
         AC_VMW_LIB_ERROR([CUNIT], [cunit])
      else
         AC_MSG_WARN([CUnit not found, tests won't be compiled.])
      fi
   fi
fi

# If the user explicitly disables X11, then don't try to detect the X-related libraries
if test "$have_x" = "disabled"; then
   enable_multimon="no"
elif test "$have_x" != "yes"; then
   AC_MSG_ERROR(
      [The X11 libraries were not found. Please configure without X11 (using --without-x), or install the libX11 devel package(s).])
else
   CPPFLAGS="$CPPFLAGS $X_CFLAGS"
   COMMON_XLIBS="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"

   AC_CHECK_LIB(
      [Xext],
      [XeviQueryVersion],
      [COMMON_XLIBS="-lXext $COMMON_XLIBS"],
      [AC_MSG_ERROR(
         [libXext not found. Please configure without X11 (using --without-x), or install the libXext devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_HEADER(
      [X11/extensions/extutil.h],
      [],
      [AC_MSG_ERROR(
         [X11/extensions/extutil.h header not found - you are probably on Solaris 10 or older. Please copy that header file onto your system manually, or configure without X11 (using --without-x).])],
      [#include <X11/Xlib.h>
       #include <X11/Xproto.h>])

   if test "$enable_multimon" != "no"; then
      AC_CHECK_LIB(
	 [Xinerama],
	 [XineramaQueryVersion],
	 [COMMON_XLIBS="-lXinerama $COMMON_XLIBS"],
	 [AC_MSG_ERROR(
	    [libXinerama not found. Please configure without multimon (using --disable-multimon), configure without X11 (using --without-x), or install the libXinerama devel package(s).])],
	 [$COMMON_XLIBS])
   fi

   AC_CHECK_LIB(
      [Xi],
      [XOpenDevice],
      [COMMON_XLIBS="-lXi $COMMON_XLIBS"],
      [AC_MSG_ERROR(
         [libXi not found. Please configure without X11 (using --without-x), or install the libXi devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_LIB(
      [Xrender],
      [XRenderQueryVersion],
      [COMMON_XLIBS="-lXrender $COMMON_XLIBS"],
      [AC_MSG_ERROR(
         [libXrender not found. Please configure without X11 (using --without-x), or install the libXrender devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_LIB(
      [Xrandr],
      [XRRQueryVersion],
      [COMMON_XLIBS="-lXrandr $COMMON_XLIBS"],
      [AC_MSG_ERROR(
	 [libXrandr not found. Please configure without X11 (using --without-x) or install the libXrandr devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_LIB(
      [Xtst],
      [XTestQueryExtension],
      [COMMON_XLIBS="-lXtst $COMMON_XLIBS"],
      [AC_MSG_ERROR(
	 [libXtst not found. Please configure without X11 (using --without-x) or install the libXtst devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_LIB(
      [SM],
      [SmcOpenConnection],
      [XSM_LIBS="-lSM -lICE" && have_xsm_lib="yes"],
      []
      [-lICE])

   AC_CHECK_HEADERS([X11/SM/SMlib.h X11/ICE/ICElib.h],
                    [have_xsm_header="yes"],
                    [],
                    [])
   if test "$have_xsm_lib" = "yes" -a "$have_xsm_header" = "yes"; then
      have_xsm="yes"
   fi

   AC_CHECK_LIB(
      [Xcomposite],
      [XCompositeQueryExtension],
      [XCOMPOSITE_LIBS="-lXcomposite"],
      [have_xcomposite="no"]
      [])
   AC_CHECK_HEADERS([X11/extensions/Xcomposite.h],
                    [],
                    [have_xcomposite="no"],
                    [])
   if test "$have_xcomposite" != "no"; then
      have_xcomposite="yes"
   fi

   # Check whether we have gtk+ 3.0.
   if test "$with_gtk3" != "no"; then
      # gdk_display_get_default_group (added in gtk+ 2.4.0) is function currently
      # needed by vmware-user.
      AC_VMW_CHECK_LIB([gtk-3],
                       [GTK],
                       [gtk+-3.0],
                       [],
                       [3.0.0],
                       [gtk/gtk.h],
                       [gdk_display_get_default_group],
                       [GTK_CPPFLAGS="$GTK_CPPFLAGS -DGTK3"],
                       [AC_MSG_ERROR([Gtk+ 3.0 library not found or too old. Please configure without Gtk+ support (using --without-gtk3) or install the Gtk+ 3.0 devel package.])])

   # Check whether we have gtk+ 2.0.
   elif test "$with_gtk2" != "no"; then
      AC_VMW_CHECK_LIB([gtk-x11-2.0],
                       [GTK],
                       [gtk+-2.0],
                       [],
                       [2.4.0],
                       [gtk/gtk.h],
                       [gdk_display_get_default_group],
                       [GTK_CPPFLAGS="$GTK_CPPFLAGS -DGTK2"],
                       [AC_MSG_ERROR([Gtk+ 2.0 library not found or too old. Please configure without Gtk+ support (using --without-gtk2) or install the Gtk+ 2.0 devel package.])])
   fi

   #
   # Check for gtkmm 2.4.0 or greater.
   #


   if test "$with_gtkmm" != "no" -o "$with_gtkmm3" != "no"; then
      if test "$with_gtkmm3" != "no"; then
         AC_VMW_CHECK_LIBXX([gtkmm-3.0],
                            [GTKMM],
                            [gtkmm-3.0],
                            [],
                            [3.0.0],
                            [gtkmm.h],
                            [],
                            [GTKMM_CPPFLAGS="$GTKMM_CPPFLAGS -DHAVE_GTKMM"],
                            [AC_MSG_ERROR([gtkmm3 library not found. Please install the libgtkmm devel package(s), or re-configure using --without-gtkmm3.])])

      elif test "$with_gtkmm" != "no"; then
         CUSTOM_GTKMM_CPPFLAGS="$CUSTOM_GTKMM_CPPFLAGS $GTK_CPPFLAGS"
         AC_VMW_CHECK_LIBXX([gtkmm-2.4],
                            [GTKMM],
                            [gtkmm-2.4],
                            [],
                            [2.4.0],
                            [gtkmm.h],
                            [],
                            [GTKMM_CPPFLAGS="$GTKMM_CPPFLAGS -DHAVE_GTKMM"],
                            [AC_MSG_ERROR([gtkmm library not found. Please install the libgtkmm devel package(s), or re-configure using --without-gtkmm.])])
      fi

      #
      # libsigc++-2.0 >= 2.5.1 requires C++11 support
      #
      # Calling AC_VMW_CHECK_LIBXX would duplicate the sigc++
      # flags we would have already got as part of gtkmm call
      # above. This is OK because we are not seeing any issues
      # because of this and calling AC_VMW_CHECK_LIBXX function
      # provides other benefits like picking custom flags.
      #
      # AC_VMW_CHECK_LIBXX sets CPPFLAGS and LIBS variables only,
      # so we have to use SIGCXX_CPPFLAGS instead of SIGCXX_CXXFLAGS.
      #
      AC_VMW_CHECK_LIBXX([sigc++-2.0],
                         [SIGCXX],
                         [sigc++-2.0],
                         [],
                         [2.5.1],
                         [sigc++.h],
                         [],
                         [SIGCXX_CPPFLAGS="$SIGCXX_CPPFLAGS -std=c++11"],
                         [SIGCXX_CPPFLAGS="$SIGCXX_CPPFLAGS"])
   fi
fi # End of checks for X libraries

AC_CHECK_LIB(
   [crypt],
   [crypt],
   [HAVE_CRYPT="yes"],
   [AC_MSG_ERROR(
      [libcrypt not found. Please install the libc/libcrypt devel package(s).])])

AC_CHECK_FUNCS(
   dlopen,
   ,
   [AC_CHECK_LIB(
      dl,
      dlopen,
      [VIX_LIBADD="$VIX_LIBADD -ldl"
       LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -ldl"
       VGAUTH_LIBADD="$VGAUTH_LIBADD -ldl"
       RESOLUTIONSET_LIBADD="$RESOLUTIONSET_LIBADD -ldl"
      ],
      [AC_MSG_ERROR(
         [dlopen was not found, but is required for open-vm-tools to function properly. Please contact your OS vendor.])])])

AC_CHECK_FUNCS([ecvt])
AC_CHECK_FUNCS([fcvt])

AC_CHECK_FUNC([mkdtemp], [have_mkdtemp=yes])

if test "$os" = "freebsd" -a "$osVersion" -ge 600000; then
   AC_CHECK_LIB(
      [thr],
      [pthread_mutex_init],
      [THREAD_LIBS=-lthr],
      [AC_MSG_ERROR(
         [Unable to locate required threading library libthr.])])
else
   AC_CHECK_LIB(
      [pthread],
      [pthread_mutex_init],
      [THREAD_LIBS=-lpthread],
      [AC_MSG_ERROR(
         [libpthread not found. Please install the libc/libpthread devel package(s).])])
fi

# PAM prefix
AC_ARG_WITH([pam-prefix],
	    [AS_HELP_STRING([--with-pam-prefix],
		[specifies where pam files go. Default is $(sysconfdir)])],
	    [PAM_PREFIX="$withval"],
	    [if test "$os" = "freebsd" ; then PAM_PREFIX='$(sysconfdir)'; else PAM_PREFIX='/etc'; fi])

if test "$os" = "linux"; then
   with_dnet_dflt="no"
else
   with_dnet_dflt="yes"
fi

AC_ARG_WITH([dnet],
	    [AS_HELP_STRING([--with-dnet],
	    [specifies whether to build with libdnet - the default is without for Linux, and with it otherwise])],
	    [],
	    [with_dnet="$with_dnet_dflt"])

have_dnet="no"
if test "$with_dnet" = "yes"; then
	# On Debian, dnet is installed via the libdumbnet package. We need to
	# detect this so that our source files include dumbnet.h instead of
	# dnet.h, which is part of a different package altogether.
   AC_VMW_CHECK_LIB([dumbnet],
                    [DNET],
                    [],
                    [dumbnet-config],
                    [],
                    [dumbnet.h],
                    [intf_open],
                    [have_dnet="yes";
                     AC_DEFINE([DNET_IS_DUMBNET], 1, [Define to 1 if substituting libdumbnet for libdnet in Debian.])],
                    [])

   if test $have_dnet = "no"; then
      AC_VMW_CHECK_LIB([dnet],
                       [DNET],
                       [],
                       [dnet-config],
                       [],
                       [dnet.h],
                       [intf_open],
                       [have_dnet="yes"],
                       [])
   fi

   if test $have_dnet = "no"; then
		AC_MSG_ERROR(
		   [dnet-config was not found on your PATH. Please configure without dnet or install dnet - http://libdnet.sourceforge.net])
   fi
fi

if test "$with_dnet" != "yes"; then
AC_DEFINE([NO_DNET], 1, [Define to 1 if building without libdnet.])
fi

AC_ARG_WITH([icu],
            [AS_HELP_STRING([--with-icu],
              [enables support for ICU])],
            [],
            [with_icu=no])

if test "$have_x" = "yes" -o "$with_icu" = "yes"; then
   # AC_CHECK_PROG fails to locate CXX if it's set to an absolute
   # path and it includes arguments, which is often the case when
   # cross compling.  Try to handle this case directly.
   case "$CXX" in
      [/*])
         AS_IF(test -x ${CXX%% *}, [have_cxx=yes], [have_cxx=no])
         ;;
      [*])
         AC_CHECK_PROG([have_cxx], [$CXX], [yes], [no])
         ;;
   esac
   if test "$have_cxx" = "no"; then
      AC_MSG_ERROR([C++ compiler not found. Make sure you have a C++ compiler installed or configure without X11 (using --without-x) and without ICU (using --without-icu).])
   fi
fi

if test "$with_icu" = "yes"; then
   AC_VMW_CHECK_LIBXX([icui18n],
                      [ICU],
                      [icu-i18n],
                      [icu-config],
                      [],
                      [unicode/utf.h],
                      [],
                      [ICU_CPPFLAGS="$ICU_CPPFLAGS -DUSE_ICU"],
                      [AC_MSG_ERROR([ICU library not found. Please configure without ICU (using --without-icu) or install ICU - http://www.icu-project.org])])

   # Check whether we have ICU >= 3.8.
   AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([for ucasemap_utf8ToTitle in ICU])
   ORIGINAL_CPPFLAGS="$CPPFLAGS"
   CPPFLAGS="$CPPFLAGS $ICU_CPPFLAGS"
   AC_TRY_COMPILE([#include <unicode/ucasemap.h>],
                  [
                     (void) &ucasemap_utf8ToTitle;
                     return 0;
                  ],
                  [
                     ICU_CPPFLAGS="$ICU_CPPFLAGS -DHAVE_ICU_38"
                     AC_MSG_RESULT([yes])
                  ],
                  [AC_MSG_RESULT([no])])
   CPPFLAGS="$ORIGINAL_CPPFLAGS"
   AC_LANG_POP([C++])

   # Easier to give all modules the ICU defines/includes...
   CPPFLAGS="$CPPFLAGS $ICU_CPPFLAGS"
else
   CPPFLAGS="$CPPFLAGS -DNO_ICU"
fi


if test "$with_tirpc" != "no"; then
   AC_VMW_CHECK_LIB([libtirpc],
                    [TIRPC],
                    [libtirpc],
                    [],
                    [],
                    [],
                    [],
                    [have_tirpc="yes"],
                    [have_tirpc="no"])
fi

if test "$with_tirpc" != "yes"; then
   AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
      #include <rpc/types.h>
      #include <rpc/xdr.h>

      int main()
      {
         struct xdr_ops xdr;
#if defined(__GLIBC__)
         xdr.x_putint32(NULL, NULL);
#endif
         return 0;
      }
      ]])], [need_tirpc="no"], [need_tirpc="yes"])
   AC_MSG_NOTICE([tirpc is needed: $need_tirpc])
fi

if test "$with_tirpc" = "no"; then
   if test "$need_tirpc" = "yes"; then
      AC_MSG_ERROR([libtirpc is required but it is disabled.])
   fi
   use_tirpc="no"
elif test "$with_tirpc" = "auto"; then
   if test "$need_tirpc" = "yes" -a "$have_tirpc" = "no"; then
      AC_MSG_ERROR([cannot find libtirpc but it is required.])
   fi
   use_tirpc=$need_tirpc
elif test "$with_tirpc" = "yes"; then
   if test "$have_tirpc" = "no"; then
      AC_MSG_ERROR([cannot build with libtirpc because it cannot be found.])
   fi
   use_tirpc="yes"
fi

XDR_LIBS=
XDR_CPPFLAGS=
if test "$use_tirpc" = "yes"; then
   AC_MSG_NOTICE([building with libtirpc])
   XDR_LIBS="$TIRPC_LIBS"
   XDR_CPPFLAGS="-DUSE_TIRPC $TIRPC_CPPFLAGS"
else
   AC_MSG_NOTICE([building without libtirpc])
   # In Solaris, the XDR-related functions are not in libc like in Linux and
   # FreeBSD, so binaries need to be linked to some extra libraries.
   if test "$os" = "solaris"; then
      XDR_LIBS="-lnsl -lrpcsvc"
   fi
fi

AC_PATH_PROG(
   [RPCGEN],
   [rpcgen],
   [not_found])
if test "$RPCGEN" == "not_found"; then
   AC_MSG_ERROR([rpcgen not found.])
fi

###
### Headers
###

AC_CHECK_HEADERS([crypt.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([wchar.h])
AC_CHECK_HEADERS([sys/inttypes.h])
AC_CHECK_HEADERS([sys/io.h])
AC_CHECK_HEADERS([sys/param.h]) # Required to make the sys/user.h check work correctly on FreeBSD
AC_CHECK_HEADERS([sys/sysinfo.h])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([sys/user.h],
   [],
   [],
   [
#ifdef HAVE_SYS_PARAM_H
#   include <sys/param.h>
#endif
   ])
AC_CHECK_HEADERS([sys/vfs.h])
AC_CHECK_HEADERS([syslimits.h])

# On Freebsd, the unwind.h header file is available in the libunwind
# package, but the necessary functions are only available if __GNU_SOURCE
# is defined to enable "all" GCC extensions.
if test "$os" = "freebsd"; then
   ac_cv_header_unwind_h="no"
fi
AC_CHECK_HEADERS([unwind.h])

AC_CHECK_HEADER(
   [wchar.h],
   [HAVE_WCHAR_H="yes"],
   [HAVE_WCHAR_H="no"])

if test "$os" = "linux"; then
   # Make sure kernel-headers package is installed.
   AC_CHECK_HEADER(
      [linux/unistd.h],
      [],
      [AC_MSG_ERROR(linux/unistd.h is not found. Please install kernel-headers/linux-userspace-headers/linux-libc-dev package.)])
fi

if test "$enable_multimon" != "no"; then
   AC_CHECK_HEADER(
      [X11/extensions/panoramiXproto.h],
      [],
      [AC_MSG_ERROR(
         [panoramiXproto.h not found. Please configure without multimon (using --disable-multimon) or install the libXinerama devel package(s).])],
      [#include <X11/X.h>
       #include <X11/Xmd.h>])
fi

###
### Typdefs, structs, and compiler quarks.
###
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

###
### Specific features and OS/arch flags / actions
###

### General flags / actions
CFLAGS="$CFLAGS -Wall"
CFLAGS="$CFLAGS -Werror"

# -Wno-unknown-pragmas is due to gcc not understanding '#pragma ident'
# in Xlib.h on OpenSolaris.
# Glib2 keep changing mutex APIs so we also need to disable 'deprecated'
# warnings for now (-Wno-deprecated-declarations).
for TEST_CFLAG in -Wno-pointer-sign -Wno-unused-value -fno-strict-aliasing \
    -Wno-unknown-pragmas -Wno-uninitialized -Wno-deprecated-declarations \
    -Wno-unused-const-variable; do
    AC_MSG_CHECKING([for GCC flag $TEST_CFLAG])
    ORIGINAL_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $TEST_CFLAG"
    NEW_CFLAG=""
    AC_TRY_COMPILE(
       [],
       [
	return 0;
       ],
   [NEW_CFLAG=" $TEST_CFLAG"
    AC_MSG_RESULT(yes)],
   [AC_MSG_RESULT(no)])
   CFLAGS="$ORIGINAL_CFLAGS$NEW_CFLAG"
done
CPPFLAGS="$CPPFLAGS -DVMX86_TOOLS"
CPPFLAGS="$CPPFLAGS"

# -fvisibility is used by "core service" plugins, but not required.
ORIGINAL_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden"
AC_MSG_CHECKING([for GCC flag -fvisibility])
AC_TRY_COMPILE([],
               [return 0;],
               [PLUGIN_CPPFLAGS="-fvisibility=hidden -DGCC_EXPLICIT_EXPORT";
                AC_MSG_RESULT(yes)],
               [AC_MSG_RESULT(no)])
CFLAGS="$ORIGINAL_CFLAGS"

# Detect "unused-but-set-variable" gcc warning and disable it.
ORIGINAL_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wno-unused-but-set-variable"
AC_MSG_CHECKING([for GCC flag -Wno-unused-but-set-variable])
AC_TRY_COMPILE([],
               [return 0;],
               [ORIGINAL_CFLAGS="$ORIGINAL_CFLAGS -Wno-unused-but-set-variable";
                AC_MSG_RESULT(yes)],
               [AC_MSG_RESULT(no)])
CFLAGS="$ORIGINAL_CFLAGS"


BUILDDIR="`pwd`"

INCLUDE_DIR="`cd $srcdir; pwd`/lib/include"
BLD_INCLUDE_DIR="$BUILDDIR/lib/include"
CPPFLAGS="-I$INCLUDE_DIR -I$BLD_INCLUDE_DIR $CPPFLAGS"

###
### Documentation.
###

if test "$enable_docs" = "yes"; then
   AC_CHECK_PROG([have_doxygen],
                 [doxygen],
                 [yes],
                 [no])
   if test "$have_doxygen" = "no"; then
      AC_MSG_WARN([doxygen not found; API documentation will not be generated.])
   else
      AC_PATH_PROG([DOT], [dot], [])
      if test "$DOT" = ""; then
         HAVE_DOT=NO
      else
         DOT=`dirname $DOT`
         HAVE_DOT=YES
      fi
      AC_SUBST([DOT])
      AC_SUBST([HAVE_DOT])

      AC_PATH_PROG([MSCGEN],
                   [mscgen],
                   [no])
      if test "$MSCGEN" != "no"; then
         MSCGEN_DIR="`dirname $MSCGEN`"
      else
         MSCGEN_DIR=
      fi
      AC_SUBST([MSCGEN_DIR])
   fi
fi

###
### OS/arch-specific flags / actions
###

MODULES=""
MODULES_OS="$os"
TARGET_OS="$os"
MODULES_DIR=""

if test "$os" = "linux"; then
   MODULES_DIR="$LINUXDIR/kernel/"

   CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64"
   CPPFLAGS="$CPPFLAGS -D_LARGEFILE64_SOURCE"
   CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=500"
   CPPFLAGS="$CPPFLAGS -D_BSD_SOURCE"
   CPPFLAGS="$CPPFLAGS -D_SVID_SOURCE"
   CPPFLAGS="$CPPFLAGS -D_DEFAULT_SOURCE"

   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lrt"

   # vmxnet is not supported for kernels 3.3.0 and newer
   if test "$osVersion" -lt 303000; then
      MODULES="$MODULES vmxnet"
   fi
   # See if we need vmhgfs module. Starting with 3.10.0 we use FUSE
   if test "$osVersion" -lt 310000; then
      MODULES="$MODULES vmhgfs"
   fi
   # See if we need vmci and vsock modules. Starting with 3.9 they made
   # their way into mainline kernel.
   if test "$osVersion" -lt 309000; then
      MODULES="$MODULES vmci vsock"
   fi
   if test "$osVersion" -lt 300000; then
      MODULES="$MODULES vmblock vmsync"
   fi
   if test "x$enable_resolutionkms" != "xno" && test "x$enable_vmwgfxctrl" != "xno"; then
      PKG_CHECK_MODULES(
         [LIBUDEV],
         libdrm libudev,
         [LIBUDEV_CFLAGS="$LIBUDEV_CFLAGS -DHAVE_LIBUDEV";
         enable_vmwgfxctrl="yes"],
         [AC_MSG_WARN(
            [Missing libdrm or libudev. vmwgfxctrl will be disabled and the resolutionKMS plugin will search for them at run-time.]);
            enable_vmwgfxctrl="no"
            ])
      enable_resolutionkms="yes"
   fi
fi

if test "$os" = "freebsd" || test "$os" = "kfreebsd-gnu"; then
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lkvm"
   MODULES_DIR="/boot/modules"

   if test "$osVersion" -ge 302000; then
      MODULES="$MODULES vmmemctl"
   fi
   if test "$osVersion" -ge 600000; then
      MODULES="$MODULES vmblock"
   fi

   if test "$with_kernel_modules" = "yes"; then
      echo "****************************************************************"
      echo "   You are building FreeBSD kernel modules. Make sure you use   "
      echo "   'make' to build open-vm-tools, and not GNU make ('gmake').   "
      echo "****************************************************************"
   fi
fi

if test "$os" = "solaris"; then
   LIB_IMPERSONATE_CPPFLAGS="$LIB_IMPERSONATE_CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS"
   LIB_USER_CPPFLAGS="$LIB_USER_CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS"

   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lsocket"
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lnsl"
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lresolv"
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lrpcsvc"
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lrt"

   # Setup defines to identify the OS version.
   if test "$osVersion" -eq 509000; then
      CPPFLAGS="$CPPFLAGS -DSOL9"
   fi
   if test "$osVersion" -eq 510000; then
      CPPFLAGS="$CPPFLAGS -DSOL10"
   fi
   if test "$osVersion" -eq 511000; then
      CPPFLAGS="$CPPFLAGS -DSOL11"
   fi

   MODULES="$MODULES vmxnet vmmemctl"

   # HGFS and vmblock need Solaris 10 at least.
   if test "$osVersion" -ge 510000; then
      MODULES="$MODULES vmhgfs vmblock"
   fi

   # vmxnet3 is built on Solaris 10 / 11 only if GLDv3 is installed.
   if test "$osVersion" -gt 510000; then
      AC_CHECK_HEADER(
         [sys/mac.h],
         [MODULES="$MODULES vmxnet3"],
         [AC_MSG_WARN([GLDv3 (sys/mac.h) is not installed, vmxnet3 will not be compiled.])])
   fi

   if test "$with_kernel_modules" = "yes"; then
      echo "****************************************************************"
      echo "   You are building Solaris kernel modules. Make sure you use   "
      echo "   GNU make to build open-vm-tools.                             "
      echo "****************************************************************"
   fi
fi

if test "$os" = "linux"; then
    have_udev="yes"
    AC_ARG_WITH([udev-rules-dir],
                [AS_HELP_STRING([--with-udev-rules-dir=DIR],
                    [where to install udev rules])],
                [UDEVRULESDIR="$withval"],
                [
                    UDEVRULESDIR="/lib/udev/rules.d"
                    if test "$PKG_CONFIG" != "not_found"; then
                        udevdir=$($PKG_CONFIG udev --variable=udevdir)
                        if test "x$udevdir" != "x"; then
                            UDEVRULESDIR="$udevdir/rules.d"
                        fi
                    fi
                ])
else
    have_udev="no"
    UDEVRULESDIR=""
fi

if test "x$enable_resolutionkms" = "xauto"; then
   enable_resolutionkms="no"
fi

if test "x$enable_resolutionkms" = "xyes"; then
   CPPFLAGS="$CPPFLAGS -DENABLE_RESOLUTIONKMS"
fi

if test "x$enable_vmwgfxctrl" = "xauto"; then
   enable_vmwgfxctrl="no"
fi

AM_CONDITIONAL(LINUX, test "$os" = "linux")
AM_CONDITIONAL(SOLARIS, test "$os" = "solaris")
AM_CONDITIONAL(FREEBSD, test "$os" = "freebsd" -o "$os" = "kfreebsd-gnu")
AM_CONDITIONAL(FREEBSD_CUSTOM_SYSDIR,
               test \( "$os" = "freebsd" -o "$os" = "kfreebsd-gnu" \) -a -n "$SYSDIR")
AM_CONDITIONAL(ARCH_X32, test "$arch" = "x32")
AM_CONDITIONAL(ARCH_X64, test "$arch" = "x64")
AM_CONDITIONAL(ARCH_ARM64, test "$arch" = "arm64")
AM_CONDITIONAL(HAVE_X11, test "$have_x" = "yes")
AM_CONDITIONAL(HAVE_ICU, test "$with_icu" = "yes")
AM_CONDITIONAL(WITH_KERNEL_MODULES, test "$with_kernel_modules" = "yes")
AM_CONDITIONAL(HAVE_XSM, test "$have_xsm" = "yes")
AM_CONDITIONAL(HAVE_XCOMPOSITE, test "$have_xcomposite" = "yes")
AM_CONDITIONAL(ENABLE_TESTS, test "$have_cunit" = "yes")
AM_CONDITIONAL(WITH_ROOT_PRIVILEGES, test "$with_root_privileges" = "yes")
AM_CONDITIONAL(HAVE_DOXYGEN, test "$have_doxygen" = "yes")
AM_CONDITIONAL(HAVE_FUSE, test "$have_fuse" = "yes" || test "$have_fuse3" = "yes")
AM_CONDITIONAL(HAVE_FUSE3, test "$have_fuse3" = "yes")
AM_CONDITIONAL(HAVE_GNU_LD, test "$with_gnu_ld" = "yes")
AM_CONDITIONAL(HAVE_GTKMM, test "$have_x" = "yes" -a \( "$with_gtkmm" = "yes" -o "$with_gtkmm3" = "yes" \) )
AM_CONDITIONAL(HAVE_PAM, test "$with_pam" = "yes")
AM_CONDITIONAL(USE_SLASH_PROC, test "$os" = "linux")
AM_CONDITIONAL(ENABLE_CONTAINERINFO, test "$enable_containerinfo" = "yes")
AM_CONDITIONAL(ENABLE_DEPLOYPKG, test "$enable_deploypkg" = "yes")
AM_CONDITIONAL(ENABLE_VGAUTH, test "$enable_vgauth" = "yes")
AM_CONDITIONAL(HAVE_VSOCK, test "$os" = "linux")
AM_CONDITIONAL(HAVE_MKDTEMP, test "$have_mkdtemp" = "yes")
AM_CONDITIONAL(HAVE_UDEV, test "$have_udev" = "yes")
AM_CONDITIONAL(ENABLE_RESOLUTIONKMS, test "x$enable_resolutionkms" = "xyes")
AM_CONDITIONAL(ENABLE_VMWGFXCTRL, test "x$enable_vmwgfxctrl" = "xyes")
AM_CONDITIONAL(VGAUTH_USE_CXX, test "$with_icu" = "yes")
AM_CONDITIONAL(ENABLE_LIBAPPMONITOR, test "$enable_libappmonitor" = "yes")
AM_CONDITIONAL(ENABLE_SDMP, test "$enable_servicediscovery" = "yes")
AM_CONDITIONAL(ENABLE_SALTMINION, test "$enable_saltminion" = "yes" -a \( "$arch" = "x64" \) )

if test "$have_xsm" != "yes"; then
AC_DEFINE([NO_XSM], 1, [])
fi

if test "$have_xcomposite" != "yes"; then
   AC_DEFINE([NO_XCOMPOSITE])
fi

### Feature-specific flags / actions
# Combine where possible

# If control reaches this point and multimon is still enabled, then we know
# all of the tests for required components have passed and it's safe to allow
# multimon. Otherwise, it should be disabled.
if test "$enable_multimon" = "no"; then
   # XXX: For consistency, change this to ENABLE_MULTIMON. This will require
   # some additional code cleanup.
   AC_DEFINE([NO_MULTIMON], 1, [Define to 1 if building without multimon support.])
fi

LIB_AUTH_CPPFLAGS="$LIB_AUTH_CPPFLAGS $PAM_CPPFLAGS"
if test "$HAVE_CRYPT" = "yes"; then
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lcrypt"
   VIX_LIBADD="$VIX_LIBADD -lcrypt"
fi


LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD $THREAD_LIBS"
VIX_LIBADD="$VIX_LIBADD $THREAD_LIBS"

### Core Services definitions.

HGFS_LIBS="$BUILDDIR/libhgfs/libhgfs.la"

VMTOOLS_LIBS="$BUILDDIR/libvmtools/libvmtools.la $GLIB2_LIBS"
VMTOOLS_CPPFLAGS="-DVMTOOLS_USE_GLIB $GLIB2_CPPFLAGS"

PLUGIN_CPPFLAGS="$VMTOOLS_CPPFLAGS $PLUGIN_CPPFLAGS"
PLUGIN_LDFLAGS="-Wl,-z,defs -Wl,-lc -Wl,--as-needed -shared -module -avoid-version"

# Installation directories for core services plugins.
TEST_PLUGIN_INSTALLDIR=$datadir/open-vm-tools/tests
COMMON_PLUGIN_INSTALLDIR=$libdir/open-vm-tools/plugins/common
VMSVC_PLUGIN_INSTALLDIR=$libdir/open-vm-tools/plugins/vmsvc
VMUSR_PLUGIN_INSTALLDIR=$libdir/open-vm-tools/plugins/vmusr


# General definitions
INSTVMSG='$(SHELL) $(top_srcdir)/scripts/build/instvmsg.sh'
RPCGEN_WRAPPER='$(SHELL) $(top_builddir)/scripts/build/rpcgen_wrapper.sh'

### General substs

AC_SUBST([HGFS_LIBS])
AC_SUBST([TOOLS_VERSION])
AC_SUBST([TARGET_OS])
AC_SUBST([KERNEL_RELEASE])
AC_SUBST([LINUXINCLUDE])
AC_SUBST([MODULES_OS])
AC_SUBST([MODULES_DIR])
AC_SUBST([MODULES])
AC_SUBST([COMMON_XLIBS])
AC_SUBST([XSM_LIBS])
AC_SUBST([XCOMPOSITE_LIBS])
AC_SUBST([PAM_PREFIX])
AC_SUBST([PLUGIN_CPPFLAGS])
AC_SUBST([PLUGIN_LDFLAGS])
AC_SUBST([VMTOOLS_CPPFLAGS])
AC_SUBST([VMTOOLS_LIBS])
AC_SUBST([THREAD_LIBS])
AC_SUBST([RPCGENFLAGS])
AC_SUBST([XDR_LIBS])
AC_SUBST([XDR_CPPFLAGS])
AC_SUBST([TEST_PLUGIN_INSTALLDIR])
AC_SUBST([COMMON_PLUGIN_INSTALLDIR])
AC_SUBST([VMSVC_PLUGIN_INSTALLDIR])
AC_SUBST([VMUSR_PLUGIN_INSTALLDIR])
if test "$os" = "freebsd" -a -n "$SYSDIR"; then
   # If SYSDIR is not defined, AC_SUBST expands to nothing, so we need something
   # inside this block.
   true
   AC_SUBST([SYSDIR])
fi
AC_SUBST([INSTVMSG])
AC_SUBST([RPCGEN_WRAPPER])

### Lib substs

AC_SUBST([LIB_AUTH_CPPFLAGS])
AC_SUBST([LIB_IMPERSONATE_CPPFLAGS])
AC_SUBST([LIB_USER_CPPFLAGS])
AC_SUBST([LIBVMTOOLS_LIBADD])
AC_SUBST([RESOLUTIONSET_LIBADD])

### Program substs

AC_SUBST([VIX_LIBADD])
AC_SUBST([VGAUTH_LIBADD])

AC_SUBST([UDEVRULESDIR])

###
### Create the Makefiles
###
AC_CONFIG_FILES([                      \
   Makefile                            \
   lib/Makefile                        \
   lib/auth/Makefile                   \
   lib/backdoor/Makefile               \
   lib/asyncsocket/Makefile            \
   lib/sslDirect/Makefile              \
   lib/pollGtk/Makefile                \
   lib/poll/Makefile                   \
   lib/dataMap/Makefile                \
   lib/hashMap/Makefile                \
   lib/dict/Makefile                   \
   lib/dynxdr/Makefile                 \
   lib/err/Makefile                    \
   lib/file/Makefile                   \
   lib/foundryMsg/Makefile             \
   lib/glibUtils/Makefile              \
   lib/globalConfig/Makefile           \
   lib/guestApp/Makefile               \
   lib/guestRpc/Makefile               \
   lib/guestStoreClientHelper/Makefile \
   lib/hgfs/Makefile                   \
   lib/hgfsBd/Makefile                 \
   lib/hgfsHelper/Makefile             \
   lib/hgfsServer/Makefile             \
   lib/hgfsServerManagerGuest/Makefile \
   lib/hgfsServerPolicyGuest/Makefile  \
   lib/hgfsUri/Makefile                \
   lib/impersonate/Makefile            \
   lib/lock/Makefile                   \
   lib/message/Makefile                \
   lib/misc/Makefile                   \
   lib/netUtil/Makefile                \
   lib/nicInfo/Makefile                \
   lib/panic/Makefile                  \
   lib/panicDefault/Makefile           \
   lib/procMgr/Makefile                \
   lib/rpcChannel/Makefile             \
   lib/rpcIn/Makefile                  \
   lib/rpcOut/Makefile                 \
   lib/rpcVmx/Makefile                 \
   lib/slashProc/Makefile              \
   lib/string/Makefile                 \
   lib/jsmn/Makefile                   \
   lib/stubs/Makefile                  \
   lib/syncDriver/Makefile             \
   lib/system/Makefile                 \
   lib/unicode/Makefile                \
   lib/user/Makefile                   \
   lib/vmCheck/Makefile                \
   lib/vmSignal/Makefile               \
   lib/wiper/Makefile                  \
   lib/xdg/Makefile                    \
   services/Makefile                   \
   services/vmtoolsd/Makefile          \
   services/plugins/Makefile           \
   services/plugins/gdp/Makefile       \
   services/plugins/appInfo/Makefile   \
   services/plugins/componentMgr/Makefile  \
   services/plugins/containerInfo/Makefile   \
   services/plugins/serviceDiscovery/Makefile   \
   services/plugins/desktopEvents/Makefile \
   services/plugins/dndcp/Makefile     \
   services/plugins/guestInfo/Makefile \
   services/plugins/guestStore/Makefile \
   services/plugins/hgfsServer/Makefile \
   services/plugins/powerOps/Makefile  \
   services/plugins/resolutionSet/Makefile \
   services/plugins/resolutionKMS/Makefile \
   services/plugins/timeSync/Makefile  \
   services/plugins/vix/Makefile       \
   services/plugins/vmbackup/Makefile  \
   services/plugins/deployPkg/Makefile  \
   vmware-user-suid-wrapper/Makefile   \
   toolbox/Makefile                    \
   hgfsclient/Makefile                 \
   checkvm/Makefile                    \
   vmwgfxctrl/Makefile                 \
   rpctool/Makefile                    \
   vgauth/Makefile                     \
   vgauth/lib/Makefile                 \
   vgauthImport/Makefile               \
   namespacetool/Makefile              \
   vgauth/cli/Makefile                 \
   vgauth/test/Makefile                 \
   vgauth/service/Makefile             \
   libguestlib/Makefile                \
   libguestlib/vmguestlib.pc           \
   libDeployPkg/Makefile               \
   libDeployPkg/libDeployPkg.pc        \
   libhgfs/Makefile                    \
   libguestStoreClient/Makefile        \
   libvmtools/Makefile                 \
   xferlogs/Makefile                   \
   modules/Makefile                    \
   vmblock-fuse/Makefile               \
   vmhgfs-fuse/Makefile                \
   vmblockmounter/Makefile             \
   tests/Makefile                      \
   tests/vmrpcdbg/Makefile             \
   tests/testDebug/Makefile            \
   tests/testPlugin/Makefile           \
   tests/testVmblock/Makefile          \
   docs/Makefile                       \
   docs/api/Makefile                   \
   scripts/Makefile                    \
   scripts/build/rpcgen_wrapper.sh     \
   udev/Makefile                       \
   libappmonitor/Makefile              \
   libappmonitor/appmonitor.pc         \
])









###
### Output
###
AC_OUTPUT
