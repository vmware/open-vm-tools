################################################################################
### Copyright (C) 2007-2016, 2020 VMware, Inc.  All rights reserved.
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

noinst_LTLIBRARIES = libHgfsServer.la

libHgfsServer_la_SOURCES =
libHgfsServer_la_SOURCES += hgfsCacheStub.c
libHgfsServer_la_SOURCES += hgfsServer.c
libHgfsServer_la_SOURCES += hgfsServerLinux.c
libHgfsServer_la_SOURCES += hgfsServerPacketUtil.c
libHgfsServer_la_SOURCES += hgfsDirNotifyStub.c
libHgfsServer_la_SOURCES += hgfsServerParameters.c
libHgfsServer_la_SOURCES += hgfsServerOplock.c
libHgfsServer_la_SOURCES += hgfsServerOplockMonitor.c
libHgfsServer_la_SOURCES += hgfsServerOplockLinux.c
libHgfsServer_la_SOURCES += hgfsThreadpoolStub.c

AM_CFLAGS =
AM_CFLAGS += -DVMTOOLS_USE_GLIB
AM_CFLAGS += @GLIB2_CPPFLAGS@

