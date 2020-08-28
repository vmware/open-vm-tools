################################################################################
### Copyright (C) 2019 VMware, Inc.  All rights reserved.
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

SUBDIRS =

plugindir = @VMSVC_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libappInfo.la

libappInfo_la_CPPFLAGS =
libappInfo_la_CPPFLAGS += @PLUGIN_CPPFLAGS@

libappInfo_la_LDFLAGS =
libappInfo_la_LDFLAGS += @PLUGIN_LDFLAGS@

libappInfo_la_LIBADD =
libappInfo_la_LIBADD += @VMTOOLS_LIBS@
libappInfo_la_LIBADD += @GOBJECT_LIBS@

libappInfo_la_SOURCES =
libappInfo_la_SOURCES += appInfo.c
libappInfo_la_SOURCES += appInfoInt.h
libappInfo_la_SOURCES += appInfoUtil.c
libappInfo_la_SOURCES += appInfoPosix.c