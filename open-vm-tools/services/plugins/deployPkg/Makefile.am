################################################################################
### Copyright (C) 2014-2017 VMware, Inc.  All rights reserved.
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
plugin_LTLIBRARIES = libdeployPkgPlugin.la

libdeployPkgPlugin_la_CPPFLAGS =
libdeployPkgPlugin_la_CPPFLAGS += @PLUGIN_CPPFLAGS@

libdeployPkgPlugin_la_LDFLAGS =
libdeployPkgPlugin_la_LDFLAGS += @PLUGIN_LDFLAGS@

libdeployPkgPlugin_la_LIBADD =
libdeployPkgPlugin_la_LIBADD += @VMTOOLS_LIBS@
libdeployPkgPlugin_la_LIBADD += @GOBJECT_LIBS@
libdeployPkgPlugin_la_LIBADD += $(top_builddir)/libDeployPkg/libDeployPkg.la

libdeployPkgPlugin_la_SOURCES =
libdeployPkgPlugin_la_SOURCES += deployPkg.c
libdeployPkgPlugin_la_SOURCES += deployPkgInt.h
libdeployPkgPlugin_la_SOURCES += deployPkgLog.c
libdeployPkgPlugin_la_SOURCES += deployPkgPlugin.c
