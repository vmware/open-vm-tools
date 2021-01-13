################################################################################
### Copyright (c) 2009-2016, 2020 VMware, Inc.  All rights reserved.
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

noinst_LTLIBRARIES = libvmrpcdbg.la

libvmrpcdbg_la_CPPFLAGS =
libvmrpcdbg_la_CPPFLAGS += @CUNIT_CPPFLAGS@
libvmrpcdbg_la_CPPFLAGS += @GMODULE_CPPFLAGS@
libvmrpcdbg_la_CPPFLAGS += @VMTOOLS_CPPFLAGS@
libvmrpcdbg_la_CPPFLAGS += @XDR_CPPFLAGS@
libvmrpcdbg_la_CPPFLAGS += -I$(top_srcdir)/lib/rpcChannel

libvmrpcdbg_la_LDFLAGS =
libvmrpcdbg_la_LDFLAGS += @PLUGIN_LDFLAGS@

libvmrpcdbg_la_LIBADD =
libvmrpcdbg_la_LIBADD += @CUNIT_LIBS@
libvmrpcdbg_la_LIBADD += @GMODULE_LIBS@
libvmrpcdbg_la_LIBADD += @VMTOOLS_LIBS@
libvmrpcdbg_la_LIBADD += @XDR_LIBS@

libvmrpcdbg_la_SOURCES =
libvmrpcdbg_la_SOURCES += debugChannel.c
libvmrpcdbg_la_SOURCES += vmrpcdbg.c

