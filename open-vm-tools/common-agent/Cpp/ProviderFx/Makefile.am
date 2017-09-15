################################################################################
### Copyright (C) 2016 VMware, Inc.  All rights reserved.
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

AUTOMAKE_OPTIONS = subdir-objects
caflibdir = @CAF_PME_LIB_DIR@
caflib_LTLIBRARIES =
caflib_LTLIBRARIES += libProviderFx.la

libProviderFx_la_SOURCES=
libProviderFx_la_SOURCES += ProviderFx/src/CProviderCdifFormatter.cpp
libProviderFx_la_SOURCES += ProviderFx/src/CProviderDocHelper.cpp
libProviderFx_la_SOURCES += ProviderFx/src/CProviderDriver.cpp
libProviderFx_la_SOURCES += ProviderFx/src/ProviderFx.cpp

libProviderFx_la_CPPFLAGS =
libProviderFx_la_CPPFLAGS += @GLIB2_CPPFLAGS@
libProviderFx_la_CPPFLAGS += @LOG4CPP_CPPFLAGS@
libProviderFx_la_CPPFLAGS += @SSL_CPPFLAGS@
libProviderFx_la_CPPFLAGS += @LIBRABBITMQ_CPPFLAGS@

libProviderFx_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
libProviderFx_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ProviderFx/ProviderFx/include
libProviderFx_la_LIBADD =
libProviderFx_la_LIBADD += @GLIB2_LIBS@
libProviderFx_la_LIBADD += @LOG4CPP_LIBS@
libProviderFx_la_LIBADD += -ldl
libProviderFx_la_LIBADD += ../Framework/libFramework.la

libProviderFx_la_LDFLAGS = -shared

