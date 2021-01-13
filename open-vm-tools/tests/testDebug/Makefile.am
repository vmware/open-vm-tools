################################################################################
### Copyright (c) 2009-2016, 2020 VMware, Inc.  All rights reserved.
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

plugindir = @TEST_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libtestDebug.la

libtestDebug_la_CPPFLAGS =
libtestDebug_la_CPPFLAGS += @CUNIT_CPPFLAGS@
libtestDebug_la_CPPFLAGS += @GOBJECT_CPPFLAGS@
libtestDebug_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libtestDebug_la_CPPFLAGS += @XDR_CPPFLAGS@

libtestDebug_la_LDFLAGS =
libtestDebug_la_LDFLAGS += @PLUGIN_LDFLAGS@

libtestDebug_la_LIBADD =
libtestDebug_la_LIBADD += @CUNIT_LIBS@
libtestDebug_la_LIBADD += @GOBJECT_LIBS@
libtestDebug_la_LIBADD += @VMTOOLS_LIBS@
libtestDebug_la_LIBADD += @XDR_LIBS@
libtestDebug_la_LIBADD += ../vmrpcdbg/libvmrpcdbg.la

libtestDebug_la_SOURCES =
libtestDebug_la_SOURCES += testDebug.c
libtestDebug_la_SOURCES += testData_xdr.c

BUILT_SOURCES =
BUILT_SOURCES += testData_xdr.c
BUILT_SOURCES += testData.h

# XXX: see explanation in lib/guestRpc/Makefile.am
CFLAGS += -Wno-unused

CLEANFILES =
CLEANFILES += testData_xdr.c
CLEANFILES += testData.h

testData.h: $(top_srcdir)/tests/testPlugin/testData.x
	@RPCGEN_WRAPPER@ tests/testPlugin/testData.x $@

testData_xdr.c: testData.h
	@RPCGEN_WRAPPER@ tests/testPlugin/testData.x $@

