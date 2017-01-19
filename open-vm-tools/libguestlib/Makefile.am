################################################################################
### Copyright (C) 2007-2016 VMware, Inc.  All rights reserved.
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

lib_LTLIBRARIES = libguestlib.la

AM_CFLAGS =
AM_CFLAGS += -I$(top_srcdir)/include

libguestlib_la_LIBADD =
libguestlib_la_LIBADD += @VMTOOLS_LIBS@

libguestlib_la_SOURCES =
libguestlib_la_SOURCES += guestlibV3_xdr.c
libguestlib_la_SOURCES += guestlibIoctl_xdr.c
libguestlib_la_SOURCES += vmGuestLib.c

libguestlib_la_LDFLAGS =
# We require GCC, so we're fine passing compiler-specific flags.
# Needed for OS's that don't link shared libraries against libc by default, e.g. FreeBSD
libguestlib_la_LDFLAGS += -Wl,-lc

libguestlib_includedir = $(includedir)/vmGuestLib

libguestlib_include_HEADERS =
libguestlib_include_HEADERS += $(top_srcdir)/lib/include/includeCheck.h
libguestlib_include_HEADERS += $(top_srcdir)/lib/include/vmGuestLib.h
libguestlib_include_HEADERS += $(top_srcdir)/lib/include/vmSessionId.h
libguestlib_include_HEADERS += $(top_srcdir)/lib/include/vm_basic_types.h

BUILT_SOURCES =
BUILT_SOURCES += guestlibV3.h
BUILT_SOURCES += guestlibV3_xdr.c
BUILT_SOURCES += guestlibIoctl.h
BUILT_SOURCES += guestlibIoctl_xdr.c

CLEANFILES =
CLEANFILES += $(BUILT_SOURCES)

# XXX: see explanation in lib/guestRpc/Makefile.am
CFLAGS += -Wno-unused

libguestlib_la_CPPFLAGS =
libguestlib_la_CPPFLAGS += -DVMTOOLS_USE_GLIB
libguestlib_la_CPPFLAGS += @GLIB2_CPPFLAGS@

EXTRA_DIST = vmguestlib.pc.in

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = vmguestlib.pc

$(pkgconfig_DATA): $(top_builddir)/config.status

guestlibIoctl.h: guestlibIoctl.x
	@RPCGEN_WRAPPER@ libguestlib/guestlibIoctl.x $@

guestlibIoctl_xdr.c: guestlibIoctl.x guestlibIoctl.h
	@RPCGEN_WRAPPER@ libguestlib/guestlibIoctl.x $@

guestlibV3.h: guestlibV3.x
	@RPCGEN_WRAPPER@ libguestlib/guestlibV3.x $@

guestlibV3_xdr.c: guestlibV3.x guestlibV3.h
	@RPCGEN_WRAPPER@ libguestlib/guestlibV3.x $@


