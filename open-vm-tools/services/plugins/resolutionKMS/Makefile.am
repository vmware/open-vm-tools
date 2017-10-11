################################################################################
### Copyright (C) 2009-2016 VMware, Inc.  All rights reserved.
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

plugindir = @VMSVC_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libresolutionKMS.la

libresolutionKMS_la_CPPFLAGS =
libresolutionKMS_la_CPPFLAGS += @LIBUDEV_CFLAGS@
libresolutionKMS_la_CPPFLAGS += @PLUGIN_CPPFLAGS@

libresolutionKMS_la_LDFLAGS =
libresolutionKMS_la_LDFLAGS += @PLUGIN_LDFLAGS@

libresolutionKMS_la_LIBADD =
libresolutionKMS_la_LIBADD += @VMTOOLS_LIBS@
libresolutionKMS_la_LIBADD += @RESOLUTIONSET_LIBADD@
libresolutionKMS_la_LIBADD += $(top_builddir)/services/plugins/resolutionSet/libresolutionCommon.la

libresolutionKMS_la_SOURCES =
libresolutionKMS_la_SOURCES += resolutionKMS.c

libresolutionKMS_la_DEPENDENCIES =
libresolutionKMS_la_DEPENDENCIES += $(top_builddir)/services/plugins/resolutionSet/libresolutionCommon.la
