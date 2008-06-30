################################################################################
### Copyright 2007 VMware, Inc.  All rights reserved.
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
# etc.), this script also accepts:
# $CUSTOM_GTK_CPPFLAGS - compile-time flags to find Gtk headers.
# $CUSTOM_GTK_LIBS - link-time flags to find and link the Gtk libraries.
# $CUSTOM_PROCPS_CPPFLAGS - compile-time flags to find procps headers
# $CUSTOM_PROCPS_LIBS - link-time flags to find and link the procps library.
################################################################################

###
### Initialization
###

AC_INIT(
   [open-vm-tools],
   [2008.06.20-100027],
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

AC_CANONICAL_HOST
AC_CANONICAL_BUILD

# Quote the regular expressions
case "$host_cpu" in
   [i[3456]86])
      userSpaceBitness="32"
      ;;
   [x86_64])
      userSpaceBitness="64"
      ;;
   [*])
      AC_MSG_ERROR([Unknown architecture.])
      ;;
esac

# Kernel args
# The kernel args have to go here otherwise the KERNEL_RELEASE won't be visible
# to getOsVersion()
AC_ARG_WITH([kernel-modules],
	    [AS_HELP_STRING([--without-kernel-modules],
		[Do not compile or install the kernel modules])],
	    [],
	    [with_kernel_modules=yes])

AC_ARG_WITH([kernel-release],
	    [AS_HELP_STRING([--with-kernel-release],
		[Specify the kernel release you want to build against])],
	    [KERNEL_RELEASE="$withval"],
	    [KERNEL_RELEASE=`uname -r`])

AC_ARG_WITH([linuxdir],
	    [AS_HELP_STRING([--with-linuxdir],
		[Specify the Linux directory you want to use])],
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
   [solaris*])
      os="solaris"
      ;;
   [*])
      AC_MSG_WARN([This is an untested and unsupported Operating System. Proceed at your own peril.])
      ;;
esac
osVersion="`getOsVersion`"
if test "$os" = "linux"; then
   if test "$with_kernel_modules" = "yes"; then
      if test ! -d "$LINUXDIR/kernel/"; then
      	 AC_MSG_ERROR([$LINUXDIR/kernel does not exist])
      fi
   fi
fi

AC_ARG_ENABLE(
   multimon,
   AS_HELP_STRING(
      [--disable-multimon],
      [Disables multimon, enabled by default]),
   [MULTIMON_ENABLED="$enableval"],
   [MULTIMON_ENABLED="yes"])

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

CPPFLAGS="$CPPFLAGS -DUSING_AUTOCONF=1"

###
### Programs
###
AC_PROG_CPP
AC_PROG_CC
AC_PROG_INSTALL
# Needed for creating the archives in lib/ and the shared libraries.
AC_PROG_LIBTOOL
if test "$ac_cv_prog_AR" = false; then
   AC_MSG_ERROR([The 'ar' utility was not found. Please put ar on the path.])
fi

# We use pkg-config to set up the cflags and libs for gtk.
AC_CHECK_PROG(
   [HAVE_PKG_CONFIG],
   [pkg-config],
   [yes],
   [no])

if test "$GCC" != "yes"; then
   AC_MSG_ERROR([Only GCC is currently supported. Please put gcc in the path.])
fi

###
### Libraries
###

AC_PATH_XTRA

# If the user explicitly disables X11, then don't try to detect the X-related libraries
if test "$have_x" = "disabled"; then
   MULTIMON_ENABLED="no"
elif test "$have_x" != "yes"; then
   AC_MSG_ERROR(
      [The X11 libraries were not found. Please configure without X11 (using --without-x), or install the libX11 devel package(s).])
else
   LIBS="$LIBS $X_LIBS"
   CPPFLAGS="$CPPFLAGS $X_CFLAGS"
   COMMON_XLIBS="-lX11"
   COMMON_XLIBS="-lXext $COMMON_XLIBS"

   AC_CHECK_HEADER(
      [X11/extensions/extutil.h],
      [],
      [AC_MSG_ERROR(
         [X11/extensions/extutil.h header not found - you're probably on Solaris 10 or older. Please copy that header file onto your system manually, or configure without X11 (using --without-x).])],
      [#include <X11/Xlib.h>
       #include <X11/Xproto.h>])

   if test "$MULTIMON_ENABLED" != "no"; then
      AC_CHECK_LIB(
	 [Xinerama],
	 [XineramaQueryVersion],
	 [COMMON_XLIBS="-lXinerama $COMMON_XLIBS"],
	 [AC_MSG_ERROR(
	    [libXinerama not found. Please configure without multimon (using --disable-multimon), configure without X11 (using --without-x), or install the libXinerama devel package(s).])],
	 [$COMMON_XLIBS])
   fi

   AC_CHECK_LIB(
      [Xrandr],
      [XRRQueryVersion],
      [COMMON_XLIBS="-lXrandr $COMMON_XLIBS"
       COMMON_XLIBS="-lXrender $COMMON_XLIBS"],
      [AC_MSG_ERROR(
	 [libXrandr not found. Please configure without X11 (using --without-x) or install the libXrandr devel package(s).])],
      [-lXrender $COMMON_XLIBS])

   AC_CHECK_LIB(
      [Xtst],
      [XTestQueryExtension],
      [COMMON_XLIBS="-lXtst $COMMON_XLIBS"],
      [AC_MSG_ERROR(
	 [libXtst not found. Please configure without X11 (using --without-x) or install the libXtst devel package(s).])],
      [$COMMON_XLIBS])

   if test -n "$CUSTOM_GTK_CPPFLAGS" -o -n "$CUSTOM_GTK_LIBS"; then
   # Check to see if we have some version of GTK. gtk_accel_group_add
   # is a random function that is present in Gtk 1.2 but not Gtk
   # 2.0. Similarly, gtk_tree_view_new is a random function in Gtk 2.0
   # which is not present in Gtk 1.2.
      AC_CHECK_LIB(
	 [gtk],
	 [gtk_accel_group_add],
	 [HAVE_GTK1="yes"],,
	 [$CUSTOM_GTK_LIBS])
      AC_CHECK_LIB(
	 [gtk-x11-2.0],
	 [gtk_tree_view_new],
	 [HAVE_GTK2="yes"],,
	 [$CUSTOM_GTK_LIBS])
   fi

   # Sometimes pkg-config can find stuff that AC_CHECK_LIB can't, so we use that as well.
   if test "$HAVE_PKG_CONFIG" = "yes"; then
      AC_MSG_CHECKING([for Gtk+ 2.0 (via pkg-config)])
      if pkg-config --exists gtk+-2.0; then
	 HAVE_GTK2="yes"
         AC_MSG_RESULT([yes])
      else
         AC_MSG_RESULT([no])
      fi

      AC_MSG_CHECKING([for Gtk+ 1.x (via pkg-config)])
      # The latter half of the test is because pkg-config's fallback
      # to gtk-config is broken on systems such as Ubuntu that use
      # bash-completion for unknown commands. (On Ubuntu, typing
      # 'gtk-config' on a system that doesn't have libgtk1.2-dev
      # installed outputs hints about using apt-get, in addition to the
      # regular bash "command not found" message.)
      if pkg-config --exists gtk+ \
	 && test -n "`pkg-config --libs gtk+ 2>/dev/null | grep -v '^ *$'`"; then
	 HAVE_GTK1="yes"
         AC_MSG_RESULT([yes])
      else
         AC_MSG_RESULT([no])
      fi
   fi

   if test "$HAVE_GTK1" = "yes" -o "$HAVE_GTK2" = "yes"; then
      # This bit is to fix builds on OpenSolaris B71, where
      # 'pkg-config --cflags gtk+-2.0' gives an error.
      if test "$HAVE_GTK2" = "yes" -a "$HAVE_PKG_CONFIG" = "yes" \
	 && ! pkg-config --exists gtk+-2.0 \
	 && test -z "$CUSTOM_GTK_CPPFLAGS" -a -z "$CUSTOM_GTK_LIBS"; then
         HAVE_GTK2="no"
      fi

      if test "$HAVE_GTK2" = "yes"; then
      # Gtk 2.0
	 GTK_CPPFLAGS="$GTK_CPPFLAGS -DGTK2"
	 gtkPackageName="gtk+-2.0"
      elif test "$HAVE_GTK1" = "yes"; then
      # Gtk 1.2
	 gtkPackageName="gtk+"
      else
      # We found Gtk+ 2.0 that turned out to have a non-working
      # pkg-config setup, AND Gtk 1.2 wasn't installed.
	 AC_MSG_ERROR([The Gtk+ library installed on your system is not fully functional. Please contact your OS vendor.])
      fi

      # Prefer flags specified, in the environment, by the user.
      if test -n "$CUSTOM_GTK_CPPFLAGS" -o -n "$CUSTOM_GTK_LIBS"; then
	 GTK_CPPFLAGS="$GTK_CPPFLAGS $CUSTOM_GTK_CPPFLAGS"
	 GTK_LIBS="$GTK_LIBS $CUSTOM_GTK_LIBS"
      # Use pkg-config if available and there are no user-specified flags.
      elif test "$HAVE_PKG_CONFIG" = "yes"; then
	 GTK_CPPFLAGS="$GTK_CPPFLAGS $(pkg-config --cflags $gtkPackageName)"
	 GTK_LIBS="$GTK_LIBS $(pkg-config --libs $gtkPackageName)"
      # Otherwise, we have a problem.
      else
	 AC_MSG_ERROR([pkg-config not found. Get pkg-config or set CUSTOM_GTK_CPPFLAGS and CUSTOM_GTK_LIBS to specify gtk header file and library locations.])
      fi

   else
      AC_MSG_ERROR([libgtk not found. Please install the gtk devel package(s).])
   fi
fi # End of checks for X libraries

AC_CHECK_LIB(
   [crypt],
   [crypt],
   [HAVE_CRYPT="yes"
    GUESTD_LDADD="$GUESTD_LDADD -lcrypt"],
   [AC_MSG_ERROR(
      [libcrypt not found. Please install the libc/libcrypt devel package(s).])])

AC_CHECK_FUNCS(
   dlopen,
   ,
   [AC_CHECK_LIB(
      dl,
      dlopen,
      GUESTD_LDADD="$GUESTD_LDADD -ldl",
      [AC_MSG_ERROR(
         [dlopen was not found, but is required for open-vm-tools to function properly. Please contact your OS vendor.])])])

AC_CHECK_FUNCS([ecvt])
AC_CHECK_FUNCS([fcvt])

AC_CHECK_LIB(
   [pthread],
   [pthread_mutex_init],
   [GUESTD_LDADD="$GUESTD_LDADD -lpthread"
    VMWARE_USER_LDADD="$VMWARE_USER_LDADD -lpthread"],
   [AC_MSG_ERROR(
      [libpthread not found. Please install the libc/libpthread devel package(s).])])

# PAM prefix
AC_ARG_WITH([pam-prefix],
	    [AS_HELP_STRING([--with-pam-prefix],
		[specify where pam files go. Default is $(sysconfdir)])],
	    [PAM_PREFIX="$withval"],
	    [PAM_PREFIX='$(sysconfdir)'])

if test "$os" = "linux"; then
        AC_ARG_WITH([procps],
	    [AS_HELP_STRING([--without-procps],
		[compile without libproc (disables support for meminfo)])],
	    [],
	    [with_procps=yes])
else
	with_procps="no"
fi

if test "$with_procps" = "yes"; then
	ORIGINAL_LDFLAGS="$LDFLAGS"
	LDFLAGS="$CUSTOM_PROCPS_LDFLAGS $LDFLAGS"
	AC_CHECK_LIB(
	   [proc-3.2.7],
	   [getstat],
	   [if test -n "$CUSTOM_PROCPS_LIBS"; then
		    PROCPS_LIBS="$CUSTOM_PROCPS_LIBS"
	    else
		    PROCPS_LIBS="-lproc-3.2.7"
	    fi],
	   [AC_MSG_ERROR(
	      [libproc not found. Please configure without procps (using --without-procps) or install procps - http://procps.sourceforge.net])])
	LDFLAGS="$ORIGINAL_LDFLAGS"

	GUESTD_LDADD="$GUESTD_LDADD $PROCPS_LIBS"
	VMWARE_USER_LDADD="$VMWARE_USER_LDADD $PROCPS_LIBS"

	# On Debian, the 'libproc-dev' package does not contain the libproc
	# shared object; it is provided in the 'procps' package. Having the
	# 'procps' package installed without 'libproc-dev' would lead to
	# compilation errors down the line, so to avoid that we explicitly
	# check for one of the libproc headers that is consumed in the build.
	if test -n "$CUSTOM_PROCPS_CPPFLAGS"; then
		PROCPS_CPPFLAGS="$CUSTOM_PROCPS_CPPFLAGS"
	else
		# XXX: Unfortunately, the libproc devel package doesn't appear
		# to ship with a configuration script or pkg-config manifest
		# file, so there's no portable way to get the location of its
		# headers. For now we default to /usr/include.
		PROCPS_CPPFLAGS="-I/usr/include"
	fi
	ORIGINAL_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$PROCPS_CPPFLAGS $CPPFLAGS"
	AC_CHECK_HEADER(
	   [proc/sysinfo.h],
	   [],
	   [AC_MSG_ERROR(
	      [libproc headers not found. Please configure without procps (using --without-procps) or install libproc devel package])])
	CPPFLAGS="$ORIGINAL_CPPFLAGS"
fi

if test "$with_procps" != "yes"; then
AC_DEFINE([NO_PROCPS], 1, [Define to 1 if building without procps.])
fi

AC_ARG_WITH([dnet],
	    [AS_HELP_STRING([--without-dnet],
	    [compile without libdnet (disables support for nicinfo)])],
	    [],
	    [with_dnet=yes])

if test "$with_dnet" = "yes"; then

	# On Debian, dnet is installed via the libdumbnet package. We need to
	# detect this so that our source files include dumbnet.h instead of
	# dnet.h, which is part of a different package altogether.
	AC_CHECK_PROGS([DNET_CONFIG],
		       [dumbnet-config dnet-config],
		       [no])

	if test "$DNET_CONFIG" != no; then
		DNET_CPPFLAGS="`$DNET_CONFIG --cflags`"
		DNET_LIBS="`$DNET_CONFIG --libs`"

		GUESTD_LDADD="$GUESTD_LDADD $DNET_LIBS"
		VMWARE_USER_LDADD="$VMWARE_USER_LDADD $DNET_LIBS"
		if test "$DNET_CONFIG" == dumbnet-config; then
		AC_DEFINE([DNET_IS_DUMBNET], 1, [Define to 1 if substituting Debian's libdumbnet for libdnet.])
		fi
	else
		AC_MSG_ERROR(
		   [dnet-config was not found on your PATH. Please configure without dnet (using --without-dnet) or install dnet - http://libdnet.sourceforge.net])
	fi
fi

if test "$with_dnet" != "yes"; then
AC_DEFINE([NO_DNET], 1, [Define to 1 if building without libdnet.])
fi

AC_ARG_WITH([icu],
            [AS_HELP_STRING([--without-icu],
              [disable support for ICU])],
            [],
            [with_icu=yes])

if test "$with_icu" = "yes"; then

	AC_PATH_PROG([ICU_CONFIG],
		     [icu-config],
		     [no])

	if test "$ICU_CONFIG" != no; then
		ICU_CPPFLAGS="`$ICU_CONFIG --cppflags` -DUSE_ICU"
		ICU_LIBS="`$ICU_CONFIG --ldflags`"

		AC_CHECK_LIB([icuuc],
			     [ucasemap_utf8ToTitle_3_8],
			     [have_ICU_38="yes"],,
			     [$ICU_LIBS])

		if test "$have_ICU_38" = "yes"; then
			ICU_CPPFLAGS="$ICU_CPPFLAGS -DHAVE_ICU_38"
		fi

		# Easier to give all modules the ICU defines/includes...
		CPPFLAGS="$CPPFLAGS $ICU_CPPFLAGS"
	else
		AC_MSG_ERROR(
		   [icu-config was not found on your PATH. Please configure without ICU (using --without-icu) or install ICU - http://www.icu-project.org])
	fi
fi

AC_PATH_PROG(
   [RPCGEN],
   [rpcgen],
   [ AC_MSG_ERROR([rpcgen not found. Please install the libc devel package.]) ])

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
AC_CHECK_HEADERS([unwind.h])

AC_CHECK_HEADER(
   [wchar.h],
   [HAVE_WCHAR_H="yes"],
   [HAVE_WCHAR_H="no"])

if test "$MULTIMON_ENABLED" != "no"; then
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

# Check to see if SYS_setuid32 is defined. If SYS_setuid32 is unknown, we will
# certainly error out.
if test "$os$userSpaceBitness" = "linux32"; then
   AC_MSG_CHECKING([for SYS_setuid32])
   AC_TRY_COMPILE(
      [#include <sys/syscall.h>
       #include <unistd.h>],
      [
         syscall(SYS_setuid32, 1);
      ],
      [HAVE_SYS_SETUID="yes"],
      [HAVE_SYS_SETUID="no"])
   AC_MSG_RESULT($HAVE_SYS_SETUID)
fi

# Newer iconv (from glibc 2.2 and later on Linux) takes a char ** as the second
# argument. Here, we test for the older form and expect a compiler warning/error
# if the newer form is defined (because, in that case, we would be attempting to
# remove const'ness without an explicit cast).
AC_MSG_CHECKING([for the new format of iconv])
ORIGINAL_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Werror"
AC_TRY_COMPILE(
   [#include <iconv.h>],
   [
      iconv_t cd;
      char const *inbuf;
      size_t inbytesleft;
      char *outbuf;
      size_t outbytesleft;

      iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
   ],
   [HAVE_NEW_ICONV="no"],
   [HAVE_NEW_ICONV="yes"])
CFLAGS="$ORIGINAL_CFLAGS"
AC_MSG_RESULT($HAVE_NEW_ICONV)

if test "$os" = "linux"; then
   AC_CHECK_FUNCS(
      [lseek],
      [HAVE_LSEEK="yes"],
      [HAVE_LSEEK="no"])

   # setresuid() was wrapped beginning in glibc 2.3.2.
   AC_MSG_CHECKING([for setresuid])
   # Ensure an error is thrown if setresuid is unknown.
   ORIGINAL_CFLAGS="$CFLAGS"
   CFLAGS="$CFLAGS -Werror-implicit-function-declaration -Werror"
   AC_TRY_COMPILE(
      [#define _GNU_SOURCE
       #include <unistd.h>],
      [
         setresuid(0, 0, 0);
      ],
      [HAVE_SETRESUID="yes"],
      [HAVE_SETRESUID="no"])
   CFLAGS="$ORIGINAL_CFLAGS"
   AC_MSG_RESULT($HAVE_SETRESUID)
fi

### General flags / actions
CFLAGS="$CFLAGS -Wall"
CFLAGS="$CFLAGS -Werror"

# -Wno-unknown-pragmas is due to gcc not understanding '#pragma ident' in Xlib.h on OpenSolaris.
for TEST_CFLAG in -Wno-pointer-sign -Wno-unused-value -fno-strict-aliasing \
    -Wno-unknown-pragmas -Wno-uninitialized; do
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
CPPFLAGS="$CPPFLAGS -DNO_CORE_ICU"

INCLUDE_DIR="`cd $srcdir; pwd`/lib/include"
CPPFLAGS="$CPPFLAGS -I$INCLUDE_DIR"

### OS/arch-specific flags / actions

MODULES=""
MODULES_OS="$os"
TARGET_OS="$os"
MODULES_DIR=""
buildHgfsmounter=no

if test "$os" = "linux"; then
   MODULES_DIR="$LINUXDIR/kernel/"

   CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64"
   CPPFLAGS="$CPPFLAGS -D_LARGEFILE64_SOURCE"
   CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=500"
   CPPFLAGS="$CPPFLAGS -D_BSD_SOURCE"
   CPPFLAGS="$CPPFLAGS -D_SVID_SOURCE"

   if test "$osVersion" -ge 202000; then
      MODULES="$MODULES vmxnet vmmemctl"
   fi
   if test "$osVersion" -ge 204000; then
      MODULES="$MODULES vmhgfs vmblock"
      buildHgfsmounter=yes
   fi
   if test "$osVersion" -ge 206006; then
      MODULES="$MODULES vmsync"
   fi
fi

if test "$os" = "freebsd"; then
   MODULES_DIR="/boot/modules"

   if test "$osVersion" -gt 302000; then
      MODULES="$MODULES vmmemctl"
   fi
   if test "$osVersion" -gt 409000; then
      MODULES="$MODULES vmxnet"
   fi
   if test "$osVersion" -gt 600000; then
      MODULES="$MODULES vmhgfs vmblock"
      buildHgfsmounter=yes
   fi
fi

if test "$os" = "solaris"; then
   LIB_IMPERSONATE_CPPFLAGS="$LIB_IMPERSONATE_CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS"
   LIB_USER_CPPFLAGS="$LIB_USER_CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS"

   GUESTD_LDADD="$GUESTD_LDADD -lsocket"
   GUESTD_LDADD="$GUESTD_LDADD -lnsl"
   GUESTD_LDADD="$GUESTD_LDADD -lrpcsvc"

   VMWARE_USER_CPPFLAGS="$VMWARE_USER_CPPFLAGS -I/usr/X11/include/"
   VMWARE_USER_LDADD="$VMWARE_USER_LDADD -lXext"
   VMWARE_USER_LDADD="$VMWARE_USER_LDADD -lX11"
   VMWARE_USER_LDADD="$VMWARE_USER_LDADD -lsocket"
   VMWARE_USER_LDADD="$VMWARE_USER_LDADD -lnsl"
   VMWARE_USER_LDADD="$VMWARE_USER_LDADD -lrpcsvc"

   # uname -r on Solaris 10 reports "5.10"
   if test "$osVersion" -ge 510000; then
      CPPFLAGS="$CPPFLAGS -DSOL10"
   fi
fi

AM_CONDITIONAL(BUILD_HGFSMOUNTER, test "$buildHgfsmounter" = "yes")
AM_CONDITIONAL(LINUX, test "$os" = "linux")
AM_CONDITIONAL(SOLARIS, test "$os" = "solaris")
AM_CONDITIONAL(FREEBSD, test "$os" = "freebsd")
AM_CONDITIONAL(THIRTY_TWO_BIT_USERSPACE, test "$userSpaceBitness" = "32")
AM_CONDITIONAL(HAVE_X11, test "$have_x" = "yes")
AM_CONDITIONAL(NO_ICU, test "$with_icu" = "yes")
AM_CONDITIONAL(WITH_KERNEL_MODULES, test "$with_kernel_modules" = "yes")
AM_CONDITIONAL(PAMD, test -d /etc/pam.d)

### Feature-specific flags / actions
# Combine where possible

# If control reaches this point and multimon is still enabled, then we know all of the
# tests for required components have passed and it's safe to allow multimon.
# Otherwise, it should be disabled.
if test "$MULTIMON_ENABLED" = "no"; then
   AC_DEFINE([NO_MULTIMON], 1, [Define to 1 if building without multimon support.])
fi

if test "$os$userSpaceBitness" = "linux64" || test "$HAVE_SYS_SETUID" = "yes"; then
   LIB_MISC_CPPFLAGS="$LIB_MISC_CPPFLAGS -DGLIBC_VERSION_21"
fi

if test "$HAVE_NEW_ICONV" = "yes"; then
   LIB_MISC_CPPFLAGS="$LIB_MISC_CPPFLAGS -DGLIBC_VERSION_22"
fi

if test "$HAVE_LSEEK" = "yes"; then
   LIB_FILE_CPPFLAGS="$LIB_FILE_CPPFLAGS -DGLIBC_VERSION_21"
   LIB_HGFS_SERVER_CPPFLAGS="$LIB_HGFS_SERVER_CPPFLAGS -DGLIBC_VERSION_21"
fi

if test "$HAVE_SETRESUID" = "yes"; then
   LIB_PROC_MGR_CPPFLAGS="$LIB_PROC_MGR_CPPFLAGS -DGLIBC_VERSION_23"
fi

if test "$HAVE_CRYPT" = "yes"; then
   LIB_AUTH_CPPFLAGS="$LIB_AUTH_CPPFLAGS -DGLIBC_VERSION_22"
fi

if test "$HAVE_WCHAR_H" = "yes"; then
   LIB_STRING_CPPFLAGS="$LIB_STRING_CPPFLAGS -DGLIBC_VERSION_22"
fi

### General substs

AC_SUBST([TARGET_OS])
AC_SUBST([MODULES_OS])
AC_SUBST([MODULES_DIR])
AC_SUBST([MODULES])
AC_SUBST([COMMON_XLIBS])
AC_SUBST([GTK_CPPFLAGS])
AC_SUBST([GTK_LIBS])
AC_SUBST([DNET_CPPFLAGS])
AC_SUBST([DNET_LIBS])
AC_SUBST([PROCPS_CPPFLAGS])
AC_SUBST([PROCPS_LIBS])
AC_SUBST([ICU_CPPFLAGS])
AC_SUBST([ICU_LIBS])
AC_SUBST([PAM_PREFIX])

### Lib substs

AC_SUBST([LIB_AUTH_CPPFLAGS])
AC_SUBST([LIB_FILE_CPPFLAGS])
AC_SUBST([LIB_HGFS_SERVER_CPPFLAGS])
AC_SUBST([LIB_IMPERSONATE_CPPFLAGS])
AC_SUBST([LIB_MISC_CPPFLAGS])
AC_SUBST([LIB_PROC_MGR_CPPFLAGS])
AC_SUBST([LIB_STRING_CPPFLAGS])
AC_SUBST([LIB_USER_CPPFLAGS])

### Program substs

AC_SUBST([GUESTD_LDADD])

AC_SUBST([VMWARE_USER_CPPFLAGS])
AC_SUBST([VMWARE_USER_LDADD])

###
### Create the Makefiles
###
AC_CONFIG_FILES([                      \
   Makefile                            \
   lib/Makefile                        \
   lib/auth/Makefile                   \
   lib/backdoor/Makefile               \
   lib/backdoor/shared/Makefile        \
   lib/conf/Makefile                   \
   lib/dict/Makefile                   \
   lib/dnd/Makefile                    \
   lib/dynxdr/Makefile                 \
   lib/err/Makefile                    \
   lib/eventManager/Makefile           \
   lib/file/Makefile                   \
   lib/foundryMsg/Makefile             \
   lib/ghIntegrationStub/Makefile      \
   lib/guestApp/Makefile               \
   lib/guestInfo/Makefile              \
   lib/guestRpc/Makefile               \
   lib/hgfs/Makefile                   \
   lib/hgfsBd/Makefile                 \
   lib/hgfsServer/Makefile             \
   lib/hgfsServerManagerGuest/Makefile \
   lib/hgfsServerPolicyGuest/Makefile  \
   lib/deployPkg/Makefile              \
   lib/impersonate/Makefile            \
   lib/message/Makefile                \
   lib/message/shared/Makefile         \
   lib/misc/Makefile                   \
   lib/misc/shared/Makefile            \
   lib/netUtil/Makefile                \
   lib/panic/Makefile                  \
   lib/panicDefault/Makefile           \
   lib/printer/Makefile                \
   lib/procMgr/Makefile                \
   lib/resolution/Makefile             \
   lib/rpcIn/Makefile                  \
   lib/rpcOut/Makefile                 \
   lib/rpcOut/shared/Makefile          \
   lib/rpcVmx/Makefile                 \
   lib/SLPv2Parser/Makefile            \
   lib/socketMgr/Makefile              \
   lib/strUtil/Makefile                \
   lib/string/Makefile                 \
   lib/string/shared/Makefile          \
   lib/stubs/Makefile                  \
   lib/stubs/shared/Makefile           \
   lib/sync/Makefile                   \
   lib/sync/shared/Makefile            \
   lib/syncDriver/Makefile             \
   lib/system/Makefile                 \
   lib/toolsLogger/Makefile            \
   lib/unicode/Makefile                \
   lib/unityStub/Makefile              \
   lib/user/Makefile                   \
   lib/vixTools/Makefile               \
   lib/vmBackupLib/Makefile            \
   lib/vmCheck/Makefile                \
   lib/vmSignal/Makefile               \
   lib/wiper/Makefile                  \
   guestd/Makefile                     \
   vmware-user/Makefile                \
   vmware-user-suid-wrapper/Makefile   \
   toolbox/Makefile                    \
   hgfsclient/Makefile                 \
   hgfsmounter/Makefile                \
   checkvm/Makefile                    \
   libguestlib/Makefile                \
   xferlogs/Makefile                   \
   modules/Makefile                    \
   scripts/Makefile		       \
])

###
### Output
###
AC_OUTPUT
