################################################################################
### Copyright (C) 2009-2019 VMware, Inc.  All rights reserved.
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

plugindir = @VMUSR_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libdndcp.la

libdndcp_la_CPPFLAGS =
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/lib/include
libdndcp_la_CPPFLAGS += @GTK_CPPFLAGS@
libdndcp_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/dndcp/dnd
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/dndcp/dndGuest
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/dndcp/dndGuestBase
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/dndcp/stringxx
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/dndcp/xutils
libdndcp_la_CPPFLAGS += @XDR_CPPFLAGS@

# Passing C++ related flags to CPPFLAGS generates error.
# So, we need to pass these to C++ compilation only.
libdndcp_la_CXXFLAGS =
libdndcp_la_CXXFLAGS += @SIGCXX_CPPFLAGS@
libdndcp_la_CXXFLAGS += @GTKMM_CPPFLAGS@

libdndcp_la_LDFLAGS =
libdndcp_la_LDFLAGS += @PLUGIN_LDFLAGS@

libdndcp_la_LIBADD =
libdndcp_la_LIBADD += @COMMON_XLIBS@
libdndcp_la_LIBADD += @GTK_LIBS@
libdndcp_la_LIBADD += @GTKMM_LIBS@
libdndcp_la_LIBADD += @VMTOOLS_LIBS@
libdndcp_la_LIBADD += @HGFS_LIBS@
libdndcp_la_LIBADD += @THREAD_LIBS@
libdndcp_la_LIBADD += $(top_builddir)/lib/hgfsUri/hgfsUriPosix.lo
libdndcp_la_LIBADD += @XDR_LIBS@

libdndcp_la_SOURCES =

libdndcp_la_SOURCES += dnd/dndClipboard.c
libdndcp_la_SOURCES += dnd/dndCommon.c
libdndcp_la_SOURCES += dnd/dndCPMsgV4.c
libdndcp_la_SOURCES += dnd/dndLinux.c
libdndcp_la_SOURCES += dnd/dndMsg.c

libdndcp_la_SOURCES += dndGuestBase/copyPasteDnDWrapper.cpp
libdndcp_la_SOURCES += dndGuestBase/dndFileList.cc
libdndcp_la_SOURCES += dndGuestBase/dndRpcV3.cc
libdndcp_la_SOURCES += dndGuestBase/guestDnDCPMgr.cc
libdndcp_la_SOURCES += dndGuestBase/guestDnDDest.cc
libdndcp_la_SOURCES += dndGuestBase/guestDnDSrc.cc
libdndcp_la_SOURCES += dndGuestBase/guestDnDMgr.cc
libdndcp_la_SOURCES += dndGuestBase/guestFileTransfer.cc

libdndcp_la_SOURCES += dndGuest/copyPasteRpcV3.cc
libdndcp_la_SOURCES += dndGuest/guestCopyPasteDest.cc
libdndcp_la_SOURCES += dndGuest/guestCopyPasteMgr.cc
libdndcp_la_SOURCES += dndGuest/guestCopyPasteSrc.cc
libdndcp_la_SOURCES += dndGuest/vmGuestDnDCPMgr.cc
libdndcp_la_SOURCES += dndGuest/vmGuestFileTransfer.cc
libdndcp_la_SOURCES += dndGuest/vmGuestDnDSrc.cc
libdndcp_la_SOURCES += dndGuest/copyPasteRpcV4.cc
libdndcp_la_SOURCES += dndGuest/vmGuestDnDMgr.cc
libdndcp_la_SOURCES += dndGuest/dndRpcV4.cc
libdndcp_la_SOURCES += dndGuest/fileTransferRpcV4.cc
libdndcp_la_SOURCES += dndGuest/rpcV3Util.cpp
libdndcp_la_SOURCES += dndGuest/rpcV4Util.cpp
libdndcp_la_SOURCES += dndGuest/dndCPTransportGuestRpc.cpp

libdndcp_la_SOURCES += stringxx/string.cc
libdndcp_la_SOURCES += xutils/xutils.cc

libdndcp_la_SOURCES += copyPasteCompat.c
libdndcp_la_SOURCES += copyPasteCompatX11.c
libdndcp_la_SOURCES += vmCopyPasteDnDWrapper.cpp
libdndcp_la_SOURCES += copyPasteDnDX11.cpp
libdndcp_la_SOURCES += copyPasteUIX11.cpp
libdndcp_la_SOURCES += dndUIX11.cpp
libdndcp_la_SOURCES += dndcp.cpp
libdndcp_la_SOURCES += dragDetWndX11.cpp
libdndcp_la_SOURCES += pointer.cpp

if LINUX
libdndcp_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/dndcp/fakeMouse
libdndcp_la_SOURCES  += fakeMouseWayland/fakeMouseWayland.cpp
libdndcp_la_CPPFLAGS += -DUSE_UINPUT
endif

if HAVE_MKDTEMP
libdndcp_la_CPPFLAGS += -DDND_IS_XDG
libdndcp_la_SOURCES  += dnd/dndXdg.c
endif

# DND_VM is used to distinguish the dnd feature between WS/FS and View
libdndcp_la_CPPFLAGS += -DDND_VM

BUILT_SOURCES =
BUILT_SOURCES += cpFileContents.h
BUILT_SOURCES += cpFileContents_xdr.c

CLEANFILES =
CLEANFILES += $(BUILT_SOURCES)

libdndcp_la_SOURCES += cpFileContents_xdr.c

cpFileContents.h: cpFileContents.x
	@RPCGEN_WRAPPER@ services/plugins/dndcp/cpFileContents.x $@

cpFileContents_xdr.c: cpFileContents.x cpFileContents.h
	@RPCGEN_WRAPPER@ services/plugins/dndcp/cpFileContents.x $@
