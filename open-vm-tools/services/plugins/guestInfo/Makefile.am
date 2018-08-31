################################################################################
### Copyright (C) 2009-2018 VMware, Inc.  All rights reserved.
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
plugin_LTLIBRARIES = libguestInfo.la

libguestInfo_la_CPPFLAGS =
libguestInfo_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libguestInfo_la_CPPFLAGS += @XDR_CPPFLAGS@

libguestInfo_la_LDFLAGS =
libguestInfo_la_LDFLAGS += @PLUGIN_LDFLAGS@

libguestInfo_la_LIBADD =
libguestInfo_la_LIBADD += @VMTOOLS_LIBS@
libguestInfo_la_LIBADD += @XDR_LIBS@

libguestInfo_la_LIBADD += @DNET_LIBS@

libguestInfo_la_SOURCES =
libguestInfo_la_SOURCES += guestInfoServer.c
libguestInfo_la_SOURCES += perfMonLinux.c
libguestInfo_la_SOURCES += diskInfo.c
libguestInfo_la_SOURCES += diskInfoPosix.c
