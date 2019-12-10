################################################################################
### Copyright (C) 2018 VMware, Inc.  All rights reserved.
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

lib_LTLIBRARIES = libappmonitor.la

AM_CFLAGS =
AM_CFLAGS += -I$(top_srcdir)/lib/include

libappmonitor_la_LIBADD =
libappmonitor_la_LIBADD += ../lib/backdoor/libBackdoor.la
libappmonitor_la_LIBADD += ../lib/dataMap/libDataMap.la
libappmonitor_la_LIBADD += ../lib/err/libErr.la
libappmonitor_la_LIBADD += ../lib/hashMap/libHashMap.la
libappmonitor_la_LIBADD += ../lib/message/libMessage.la
libappmonitor_la_LIBADD += ../lib/misc/libMisc.la
libappmonitor_la_LIBADD += ../lib/rpcOut/libRpcOut.la
libappmonitor_la_LIBADD += ../lib/string/libString.la
libappmonitor_la_LIBADD += ../lib/unicode/libUnicode.la
libappmonitor_la_LIBADD += ../lib/vmCheck/libVmCheck.la
libappmonitor_la_LIBADD += ../lib/vmSignal/libVmSignal.la
libappmonitor_la_LIBADD += @ICU_LIBS@

libappmonitor_rpcchanneldir = $(top_srcdir)/lib/rpcChannel

libappmonitor_la_SOURCES =
libappmonitor_la_SOURCES += vmGuestAppMonitorLib.c
libappmonitor_la_SOURCES += stub-debug.c
libappmonitor_la_SOURCES += $(top_srcdir)/lib/stubs/stub-panic.c
libappmonitor_la_SOURCES += $(libappmonitor_rpcchanneldir)/bdoorChannel.c
libappmonitor_la_SOURCES += $(libappmonitor_rpcchanneldir)/rpcChannel.c
libappmonitor_la_SOURCES += $(libappmonitor_rpcchanneldir)/glib_stubs.c
if HAVE_VSOCK
libappmonitor_la_SOURCES += $(libappmonitor_rpcchanneldir)/vsockChannel.c
libappmonitor_la_SOURCES += $(libappmonitor_rpcchanneldir)/simpleSocket.c
endif

libappmonitor_la_LDFLAGS =
libappmonitor_la_LDFLAGS += -Wl,-ldl
libappmonitor_la_LDFLAGS += -Wl,-lrt
# We require GCC, so we're fine passing compiler-specific flags.
# Needed for OS's that don't link shared libraries against libc by default, e.g. FreeBSD
libappmonitor_la_LDFLAGS += -Wl,-lc

libappmonitor_includedir = $(includedir)/libappmonitor

libappmonitor_include_HEADERS =
libappmonitor_include_HEADERS += $(top_srcdir)/lib/include/vmGuestAppMonitorLib.h

libappmonitor_la_CPPFLAGS =
libappmonitor_la_CPPFLAGS += -DVMTOOLS_USE_GLIB
libappmonitor_la_CPPFLAGS += -DUSE_RPCI_ONLY
libappmonitor_la_CPPFLAGS += @GLIB2_CPPFLAGS@

EXTRA_DIST = appmonitor.pc.in

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = appmonitor.pc

$(pkgconfig_DATA): $(top_builddir)/config.status

