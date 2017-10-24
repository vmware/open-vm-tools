################################################################################
### Copyright (C) 2009-2017 VMware, Inc.  All rights reserved.
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
noinst_LTLIBRARIES = libresolutionCommon.la

if HAVE_X11
plugin_LTLIBRARIES = libresolutionSet.la

libresolutionSet_la_CPPFLAGS =
libresolutionSet_la_CPPFLAGS += @GTK_CPPFLAGS@
libresolutionSet_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libresolutionSet_la_CPPFLAGS += -DRESOLUTION_X11

libresolutionSet_la_LDFLAGS =
libresolutionSet_la_LDFLAGS += @PLUGIN_LDFLAGS@

libresolutionSet_la_LIBADD =
libresolutionSet_la_LIBADD += @COMMON_XLIBS@
libresolutionSet_la_LIBADD += @GTK_LIBS@
libresolutionSet_la_LIBADD += @VMTOOLS_LIBS@
libresolutionSet_la_LIBADD += @RESOLUTIONSET_LIBADD@
libresolutionSet_la_LIBADD += libresolutionCommon.la

libresolutionSet_la_SOURCES =
libresolutionSet_la_SOURCES += libvmwarectrl.c
libresolutionSet_la_SOURCES += resolutionSet.c
libresolutionSet_la_SOURCES += resolutionX11.c
libresolutionSet_la_SOURCES += resolutionRandR12.c
endif

libresolutionCommon_la_CPPFLAGS =
libresolutionCommon_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libresolutionCommon_la_CPPFLAGS += @LIBUDEV_CFLAGS@

libresolutionCommon_la_SOURCES =
libresolutionCommon_la_SOURCES += resolutionCommon.c
libresolutionCommon_la_SOURCES += resolutionCommon.h
libresolutionCommon_la_SOURCES += resolutionDL.c
libresolutionCommon_la_SOURCES += resolutionDL.h
libresolutionCommon_la_LDFLAGS =
libresolutionCommon_la_LDFLAGS += @PLUGIN_LDFLAGS@

libresolutionCommon_la_LIBADD =
libresolutionCommon_la_LIBADD += @LIBUDEV_LIBS@
