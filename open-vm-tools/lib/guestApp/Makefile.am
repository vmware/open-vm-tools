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


# We build this library in two flavors:  with X11 and without X11.  This is
# because the X11 code supports only GuestApp_OpenUrl, and we'd like to not
# force an X11 dependency on applications that don't need one (e.g., guestd).

noinst_LTLIBRARIES = libGuestApp.la

libGuestApp_la_SOURCES =
libGuestApp_la_SOURCES += guestApp.c
libGuestApp_la_SOURCES += guestAppPosix.c

if HAVE_X11
noinst_LTLIBRARIES += libGuestAppX11.la
libGuestAppX11_la_SOURCES = $(libGuestApp_la_SOURCES)
libGuestAppX11_la_SOURCES += guestAppPosixX11.c
libGuestAppX11_la_CPPFLAGS = -DGUESTAPP_HAS_X11
libGuestAppX11_la_CPPFLAGS += @GTK_CPPFLAGS@
endif
