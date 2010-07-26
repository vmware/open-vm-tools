################################################################################
### Copyright 2007 VMware, Inc.  All rights reserved.
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

noinst_LTLIBRARIES = libGuestInfo.la

libGuestInfo_la_SOURCES =
libGuestInfo_la_SOURCES += compareNicInfo.c
libGuestInfo_la_SOURCES += guestInfo.c
libGuestInfo_la_SOURCES += guestInfoPosix.c
libGuestInfo_la_SOURCES += util.c

libGuestInfo_la_CPPFLAGS =
libGuestInfo_la_CPPFLAGS += @GLIB2_CPPFLAGS@
libGuestInfo_la_CPPFLAGS += -I$(srcdir)/..

AM_CFLAGS = $(DNET_CPPFLAGS)

