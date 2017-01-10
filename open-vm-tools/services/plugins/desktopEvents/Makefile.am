################################################################################
### Copyright (C) 2010-2016 VMware, Inc.  All rights reserved.
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

plugindir = @VMUSR_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libdesktopEvents.la

libdesktopEvents_la_CPPFLAGS =
libdesktopEvents_la_CPPFLAGS += @GTK_CPPFLAGS@
libdesktopEvents_la_CPPFLAGS += @PLUGIN_CPPFLAGS@

libdesktopEvents_la_LDFLAGS =
libdesktopEvents_la_LDFLAGS += @PLUGIN_LDFLAGS@

libdesktopEvents_la_LIBADD =
libdesktopEvents_la_LIBADD += @COMMON_XLIBS@
libdesktopEvents_la_LIBADD += @GTK_LIBS@
libdesktopEvents_la_LIBADD += @VMTOOLS_LIBS@

libdesktopEvents_la_SOURCES =
libdesktopEvents_la_SOURCES += desktopEvents.c
libdesktopEvents_la_SOURCES += reload.c
libdesktopEvents_la_SOURCES += x11Lock.c
libdesktopEvents_la_SOURCES += xioError.c

if HAVE_XSM
libdesktopEvents_la_LIBADD += @XSM_LIBS@
libdesktopEvents_la_SOURCES += sessionMgr.c
libdesktopEvents_la_SOURCES += sessionMgrSignals.c

BUILT_SOURCES =
BUILT_SOURCES += sessionMgrSignals.c
BUILT_SOURCES += sessionMgrSignals.h

CLEANFILES =
CLEANFILES += sessionMgrSignals.c
CLEANFILES += sessionMgrSignals.h

EXTRA_DIST =
EXTRA_DIST += sessionMgrSignals.gm

sessionMgrSignals.c: $(top_srcdir)/services/plugins/desktopEvents/sessionMgrSignals.gm
	glib-genmarshal --body $(top_srcdir)/services/plugins/desktopEvents/sessionMgrSignals.gm > \
	   $@ || (rm -f $@ && exit 1)

sessionMgrSignals.h: $(top_srcdir)/services/plugins/desktopEvents/sessionMgrSignals.gm
	glib-genmarshal --header $(top_srcdir)/services/plugins/desktopEvents/sessionMgrSignals.gm > \
	   $@ || (rm -f $@ && exit 1)
endif
