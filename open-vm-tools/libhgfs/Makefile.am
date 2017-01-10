################################################################################
### Copyright (C) 2010-2016 VMware, Inc.  All rights reserved.
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

lib_LTLIBRARIES = libhgfs.la

libhgfs_la_LIBADD =
libhgfs_la_LIBADD += ../lib/hgfs/libHgfs.la
libhgfs_la_LIBADD += ../lib/hgfsHelper/libHgfsHelper.la
libhgfs_la_LIBADD += ../lib/hgfsServer/libHgfsServer.la
if HAVE_GTKMM
libhgfs_la_LIBADD += ../lib/hgfsUri/libHgfsUri.la
endif
libhgfs_la_LIBADD += ../lib/hgfsServerManagerGuest/libHgfsServerManagerGuest.la
libhgfs_la_LIBADD += ../lib/hgfsServerPolicyGuest/libHgfsServerPolicyGuest.la
libhgfs_la_LIBADD += @GLIB2_LIBS@
libhgfs_la_LIBADD += @GTHREAD_LIBS@
libhgfs_la_LIBADD += @VMTOOLS_LIBS@

libhgfs_la_SOURCES =
libhgfs_la_SOURCES += hgfslib.c

libhgfs_la_CPPFLAGS =
libhgfs_la_CPPFLAGS += -I$(top_srcdir)/libvmtools

libhgfs_la_LDFLAGS =
# We require GCC, so we're fine passing compiler-specific flags.
libhgfs_la_LDFLAGS += -Wl,-z,defs
# Needed for OS's that don't link shared libraries against libc by
#default, e.g. FreeBSD
libhgfs_la_LDFLAGS += -Wl,-lc

