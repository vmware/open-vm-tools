################################################################################
### Copyright 2008 VMware, Inc.  All rights reserved.
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

noinst_LTLIBRARIES = libUnity.la

libUnity_la_SOURCES =
libUnity_la_SOURCES += unity.c
libUnity_la_SOURCES += unityPlatformX11.c
libUnity_la_SOURCES += unityPlatformX11Settings.c
libUnity_la_SOURCES += unityPlatformX11Window.c
libUnity_la_SOURCES += x11Event.c

AM_CFLAGS = @GTK_CPPFLAGS@
AM_CFLAGS += @LIB_FILE_CPPFLAGS@
AM_CFLAGS += @URIPARSER_CPPFLAGS@
