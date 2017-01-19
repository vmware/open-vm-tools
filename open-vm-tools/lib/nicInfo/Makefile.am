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

noinst_LTLIBRARIES = libNicInfo.la

libNicInfo_la_SOURCES =
libNicInfo_la_SOURCES += compareNicInfo.c
libNicInfo_la_SOURCES += util.c
libNicInfo_la_SOURCES += nicInfo.c
libNicInfo_la_SOURCES += nicInfoPosix.c

libNicInfo_la_CPPFLAGS =
libNicInfo_la_CPPFLAGS += @GLIB2_CPPFLAGS@

AM_CFLAGS = $(DNET_CPPFLAGS)
if USE_SLASH_PROC
AM_CFLAGS += -DUSE_SLASH_PROC
endif
libNicInfo_la_LIBADD =
if HAVE_DNET
   libNicInfo_la_LIBADD += @DNET_LIBS@
endif

