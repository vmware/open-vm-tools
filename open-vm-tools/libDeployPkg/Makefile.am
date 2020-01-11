################################################################################
### Copyright (C) 2014-2019 VMware, Inc.  All rights reserved.
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

lib_LTLIBRARIES = libDeployPkg.la

AM_CFLAGS =
AM_CFLAGS += -I$(top_srcdir)/include
AM_CFLAGS += $(MSPACK_CPPFLAGS)
AM_CFLAGS += -DVMTOOLS_USE_GLIB
AM_CFLAGS += @GLIB2_CPPFLAGS@

libDeployPkg_la_LIBADD =
libDeployPkg_la_LIBADD += @MSPACK_LIBS@

libDeployPkg_la_SOURCES =
libDeployPkg_la_SOURCES += deployPkgFormat.h
libDeployPkg_la_SOURCES += linuxDeployment.c
libDeployPkg_la_SOURCES += mspackConfig.h
libDeployPkg_la_SOURCES += mspackWrapper.c
libDeployPkg_la_SOURCES += mspackWrapper.h
libDeployPkg_la_SOURCES += processPosix.c
libDeployPkg_la_SOURCES += linuxDeploymentUtilities.c
libDeployPkg_la_SOURCES += linuxDeploymentUtilities.h

libDeployPkg_la_LDFLAGS =
# We require GCC, so we're fine passing compiler-specific flags.
# Needed for OS's that don't link shared libraries against libc by default, e.g. FreeBSD
libDeployPkg_la_LDFLAGS += -Wl,-lc

libDeployPkg_includedir = $(includedir)/libDeployPkg

libDeployPkg_include_HEADERS =
libDeployPkg_include_HEADERS += $(top_srcdir)/lib/include/includeCheck.h
libDeployPkg_include_HEADERS += $(top_srcdir)/lib/include/imgcust-common/imgcust-api.h
libDeployPkg_include_HEADERS += $(top_srcdir)/lib/include/imgcust-common/log.h
libDeployPkg_include_HEADERS += $(top_srcdir)/lib/include/imgcust-common/process.h
libDeployPkg_include_HEADERS += $(top_srcdir)/lib/include/vm_basic_types.h
libDeployPkg_include_HEADERS += $(top_srcdir)/lib/include/vmware/tools/guestrpc.h
libDeployPkg_include_HEADERS += $(top_srcdir)/lib/include/deployPkg/linuxDeployment.h
libDeployPkg_include_HEADERS += $(top_srcdir)/lib/include/vmware/guestrpc/deploypkg.h
libDeployPkg_include_HEADERS += $(top_srcdir)/lib/include/vmware/guestrpc/guestcust-events.h

EXTRA_DIST = libDeployPkg.pc.in

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = libDeployPkg.pc

$(pkgconfig_DATA): $(top_builddir)/config.status
