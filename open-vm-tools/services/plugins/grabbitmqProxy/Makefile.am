################################################################################
### Copyright (C) 2014-2016 VMware, Inc.  All rights reserved.
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

plugindir = @VMSVC_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libgrabbitmqProxy.la

libgrabbitmqProxy_la_CPPFLAGS =
libgrabbitmqProxy_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libgrabbitmqProxy_la_CPPFLAGS += @SSL_CPPFLAGS@

libgrabbitmqProxy_la_LDFLAGS =
libgrabbitmqProxy_la_LDFLAGS += @PLUGIN_LDFLAGS@

libgrabbitmqProxy_la_LIBADD =
libgrabbitmqProxy_la_LIBADD += @VMTOOLS_LIBS@
libgrabbitmqProxy_la_LIBADD += @SSL_LIBS@

libgrabbitmqProxy_la_SOURCES =
libgrabbitmqProxy_la_SOURCES += grabbitmqProxyPlugin.c
