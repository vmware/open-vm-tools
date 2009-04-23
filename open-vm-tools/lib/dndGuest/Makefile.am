################################################################################
### Copyright 2009 VMware, Inc.  All rights reserved.
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

noinst_LTLIBRARIES = libDndGuest.la

AM_CFLAGS = @GTK_CPPFLAGS@
AM_CFLAGS += @GTKMM_CPPFLAGS@
AM_CFLAGS += @VMWARE_USER_CPPFLAGS@
AM_CFLAGS += -I$(top_builddir)/include

AM_CPPFLAGS = @GTK_CPPFLAGS@
AM_CPPFLAGS += @GTKMM_CPPFLAGS@
AM_CPPFLAGS += @VMWARE_USER_CPPFLAGS@
AM_CPPFLAGS += -I$(top_builddir)/include

libDndGuest_la_SOURCES =
libDndGuest_la_SOURCES  += copyPaste.cc
libDndGuest_la_SOURCES  += copyPasteRpcV3.cc
libDndGuest_la_SOURCES  += dnd.cc
libDndGuest_la_SOURCES  += dndFileList.cc
libDndGuest_la_SOURCES  += dndRpcV3.cc
libDndGuest_la_SOURCES  += dndTransportGuestRpc.cc
