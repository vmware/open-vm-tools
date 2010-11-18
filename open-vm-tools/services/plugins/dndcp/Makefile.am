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

plugindir = @VMUSR_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libdndcp.la

CFLAGS += -Wno-unused

libdndcp_la_CPPFLAGS =
libdndcp_la_CPPFLAGS += @GTK_CPPFLAGS@
libdndcp_la_CPPFLAGS += @GTKMM_CPPFLAGS@
libdndcp_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/dndcp/dnd
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/dndcp/dndGuest
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/dndcp/stringxx
libdndcp_la_CPPFLAGS += -I$(top_builddir)/include

libdndcp_la_LDFLAGS =
libdndcp_la_LDFLAGS += @PLUGIN_LDFLAGS@

libdndcp_la_LIBADD =
libdndcp_la_LIBADD += @COMMON_XLIBS@
libdndcp_la_LIBADD += @GTK_LIBS@
libdndcp_la_LIBADD += @GTKMM_LIBS@
libdndcp_la_LIBADD += @VMTOOLS_LIBS@
libdndcp_la_LIBADD += @HGFS_LIBS@

libdndcp_la_SOURCES =

libdndcp_la_SOURCES += dnd/dndClipboard.c
libdndcp_la_SOURCES += dnd/dndCommon.c
libdndcp_la_SOURCES += dnd/dndLinux.c
libdndcp_la_SOURCES += dnd/dndMsg.c

libdndcp_la_SOURCES += dndGuest/copyPaste.cc
libdndcp_la_SOURCES += dndGuest/copyPasteRpcV3.cc
libdndcp_la_SOURCES += dndGuest/dnd.cc
libdndcp_la_SOURCES += dndGuest/dndFileList.cc
libdndcp_la_SOURCES += dndGuest/dndRpcV3.cc
libdndcp_la_SOURCES += dndGuest/dndTransportGuestRpc.cc

libdndcp_la_SOURCES += stringxx/string.cc

libdndcp_la_SOURCES += copyPasteCompatX11.c
libdndcp_la_SOURCES += copyPasteDnDWrapper.cpp
libdndcp_la_SOURCES += copyPasteDnDX11.cpp
libdndcp_la_SOURCES += copyPasteUIX11.cpp
libdndcp_la_SOURCES += dndUIX11.cpp
libdndcp_la_SOURCES += dndcp.cpp
libdndcp_la_SOURCES += dragDetWndX11.cpp
libdndcp_la_SOURCES += pointer.cpp

BUILT_SOURCES =
BUILT_SOURCES += cpFileContents.h
BUILT_SOURCES += cpFileContents_xdr.c

CLEANFILES =
CLEANFILES += $(BUILT_SOURCES)

libdndcp_la_SOURCES += cpFileContents_xdr.c

cpFileContents.h: cpFileContents.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                               \
	   cp -f $(top_srcdir)/services/plugins/dndcp/cpFileContents.x $(builddir); \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/cpFileContents.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

cpFileContents_xdr.c: cpFileContents.x cpFileContents.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/cpFileContents.x


