################################################################################
### Copyright 2007 VMware, Inc.  All rights reserved.
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
AM_CFLAGS += -I$(top_builddir)/include

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

CFLAGS += -Wno-unused

EXTRA_DIST = vmguestlib.pc.in

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = vmguestlib.pc

$(pkgconfig_DATA): $(top_builddir)/config.status

guestlibIoctl.h: guestlibIoctl.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                     \
	   cp -f $(top_srcdir)/libguestlib/guestlibIoctl.x $(builddir);   \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/guestlibIoctl.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@

guestlibV3.h: guestlibV3.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                     \
	   cp -f $(top_srcdir)/libguestlib/guestlibV3.x $(builddir);      \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/guestlibV3.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@

guestlibV3_xdr.c: guestlibV3.x guestlibV3.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/guestlibV3.x

guestlibIoctl_xdr.c: guestlibIoctl.x guestlibIoctl.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/guestlibIoctl.x

