################################################################################
### Copyright 2010 VMware, Inc.  All rights reserved.
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
plugin_LTLIBRARIES = libunity.la

CFLAGS += -Wno-unused

libunity_la_CPPFLAGS =
libunity_la_CPPFLAGS += @GTK_CPPFLAGS@
libunity_la_CPPFLAGS += @GTKMM_CPPFLAGS@
libunity_la_CPPFLAGS += @GIO_CPPFLAGS@
libunity_la_CPPFLAGS += @LIBPNG_CPPFLAGS@
libunity_la_CPPFLAGS += @URIPARSER_CPPFLAGS@
libunity_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libunity_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/unity/ghIntegration
libunity_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/unity/unitylib
libunity_la_CPPFLAGS += -I$(top_srcdir)/services/plugins/unity/unityWindowTracker
libunity_la_CPPFLAGS += -I$(top_builddir)/include

libunity_la_LDFLAGS =
libunity_la_LDFLAGS += @PLUGIN_LDFLAGS@

libunity_la_LIBADD =
libunity_la_LIBADD += @COMMON_XLIBS@
libunity_la_LIBADD += @XCOMPOSITE_LIBS@
libunity_la_LIBADD += @GTK_LIBS@
libunity_la_LIBADD += @GTKMM_LIBS@
libunity_la_LIBADD += @GIO_LIBS@
libunity_la_LIBADD += @LIBPNG_LIBS@
libunity_la_LIBADD += @URIPARSER_LIBS@
libunity_la_LIBADD += @VMTOOLS_LIBS@
libunity_la_LIBADD += $(top_builddir)/lib/appUtil/libAppUtil.la
libunity_la_LIBADD += $(top_builddir)/lib/image/libImage.la
libunity_la_LIBADD += $(top_builddir)/lib/raster/libRaster.la
libunity_la_LIBADD += $(top_builddir)/lib/region/libRegion.la
libunity_la_LIBADD += $(top_builddir)/lib/xdg/libXdg.la

libunity_la_SOURCES =

libunity_la_SOURCES += toolsPlugin.cpp
libunity_la_SOURCES += unityPlugin.cpp
libunity_la_SOURCES += unityPluginEntry.cpp
libunity_la_SOURCES += unityPlugin.h
libunity_la_SOURCES += unityPluginPosix.h
libunity_la_SOURCES += unityPluginPosix.cpp
libunity_la_SOURCES += ghiTclo.h
libunity_la_SOURCES += ghiTclo.cpp
libunity_la_SOURCES += unityTclo.h
libunity_la_SOURCES += unityTclo.cpp

libunity_la_SOURCES += ghIntegration/ghIntegration.cpp
libunity_la_SOURCES += ghIntegration/ghIntegrationInt.h
libunity_la_SOURCES += ghIntegration/icon.cc
libunity_la_SOURCES += ghIntegration/platform.cc
libunity_la_SOURCES += ghIntegration/pseudoAppMgr.cc

libunity_la_SOURCES += unitylib/unity.c
libunity_la_SOURCES += unitylib/unityInt.h
libunity_la_SOURCES += unitylib/unityPlatform.h
libunity_la_SOURCES += unitylib/debug.c
libunity_la_SOURCES += unitylib/event.cc
libunity_la_SOURCES += unitylib/settings.cc
libunity_la_SOURCES += unitylib/util.cc
libunity_la_SOURCES += unitylib/window.cc
libunity_la_SOURCES += unitylib/windowPathFactory.cc
libunity_la_SOURCES += unitylib/x11Platform.cc
libunity_la_SOURCES += unitylib/x11Platform.h

libunity_la_SOURCES += unityWindowTracker/unityWindowTracker.c

RPCGEN_SRCS =
RPCGEN_SRCS += ghiGetBinaryHandlers_xdr.c
RPCGEN_SRCS += ghiGetExecInfoHash_xdr.c
RPCGEN_SRCS += ghiProtocolHandler_xdr.c
RPCGEN_SRCS += ghiStartMenu_xdr.c
RPCGEN_SRCS += ghiShellAction_xdr.c
RPCGEN_SRCS += ghiTrayIcon_xdr.c
RPCGEN_SRCS += ghiSetFocusedWindow_xdr.c
RPCGEN_SRCS += ghiSetGuestHandler_xdr.c
RPCGEN_SRCS += ghiSetOutlookTempFolder_xdr.c
RPCGEN_SRCS += unityActive_xdr.c
RPCGEN_SRCS += unity_xdr.c

RPCGEN_HDRS =
RPCGEN_HDRS += ghiGetBinaryHandlers.h
RPCGEN_HDRS += ghiGetExecInfoHash.h
RPCGEN_HDRS += ghiProtocolHandler.h
RPCGEN_HDRS += ghiStartMenu.h
RPCGEN_HDRS += ghiShellAction.h
RPCGEN_HDRS += ghiTrayIcon.h
RPCGEN_HDRS += ghiSetFocusedWindow.h
RPCGEN_HDRS += ghiSetGuestHandler.h
RPCGEN_HDRS += ghiSetOutlookTempFolder.h
RPCGEN_HDRS += unityActive.h
RPCGEN_HDRS += unity.h

BUILT_SOURCES =
BUILT_SOURCES += $(RPCGEN_HDRS)
BUILT_SOURCES += $(RPCGEN_SRCS)

CLEANFILES =
CLEANFILES += $(BUILT_SOURCES)

libunity_la_SOURCES += $(RPCGEN_SRCS)

## `make clean` should also remove the copied headers.
clean-local:
	rm -rf $(top_builddir)/lib/include/guestrpc
	if [ $(top_builddir) != $(top_srcdir) ]; then	\
	   rm -f *.x;                                   \
	   rm -f ghiCommonDefines.h;                    \
	fi

ghiGetBinaryHandlers.h: ghiGetBinaryHandlers.x
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiGetBinaryHandlers.x $@ \
	   services/plugins/unity/ghiCommonDefines.h

ghiGetBinaryHandlers_xdr.c: ghiGetBinaryHandlers.x ghiGetBinaryHandlers.h
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiGetBinaryHandlers.x $@

ghiGetExecInfoHash.h: ghiGetExecInfoHash.x
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiGetExecInfoHash.x $@ \
	   services/plugins/unity/ghiCommonDefines.h

ghiGetExecInfoHash_xdr.c: ghiGetExecInfoHash.x ghiGetExecInfoHash.h
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiGetExecInfoHash.x $@

ghiProtocolHandler.h: ghiProtocolHandler.x
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiProtocolHandler.x $@ \
	   services/plugins/unity/ghiCommonDefines.h

ghiProtocolHandler_xdr.c: ghiProtocolHandler.x ghiProtocolHandler.h
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiProtocolHandler.x $@

ghiStartMenu.h: ghiStartMenu.x
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiStartMenu.x $@ \
	   services/plugins/unity/ghiCommonDefines.h

ghiStartMenu_xdr.c: ghiStartMenu.x ghiStartMenu.h
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiStartMenu.x $@

ghiShellAction.h: ghiShellAction.x
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiShellAction.x $@ \
	   services/plugins/unity/ghiCommonDefines.h

ghiShellAction_xdr.c: ghiShellAction.x ghiShellAction.h
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiShellAction.x $@

ghiTrayIcon.h: ghiTrayIcon.x
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiTrayIcon.x $@ \
	   services/plugins/unity/ghiCommonDefines.h

ghiTrayIcon_xdr.c: ghiTrayIcon.x ghiTrayIcon.h
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiTrayIcon.x $@

ghiSetFocusedWindow.h: ghiSetFocusedWindow.x
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiSetFocusedWindow.x $@ \
	   services/plugins/unity/ghiCommonDefines.h

ghiSetFocusedWindow_xdr.c: ghiSetFocusedWindow.x ghiSetFocusedWindow.h
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiSetFocusedWindow.x $@

ghiSetGuestHandler.h: ghiSetGuestHandler.x
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiSetGuestHandler.x $@ \
	   services/plugins/unity/ghiCommonDefines.h

ghiSetGuestHandler_xdr.c: ghiSetGuestHandler.x ghiSetGuestHandler.h
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiSetGuestHandler.x $@

ghiSetOutlookTempFolder.h: ghiSetOutlookTempFolder.x
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiSetOutlookTempFolder.x $@ \
	   services/plugins/unity/ghiCommonDefines.h

ghiSetOutlookTempFolder_xdr.c: ghiSetOutlookTempFolder.x ghiSetOutlookTempFolder.h
	@RPCGEN_WRAPPER@ services/plugins/unity/ghiSetOutlookTempFolder.x $@

unityActive.h: unityActive.x
	@RPCGEN_WRAPPER@ services/plugins/unity/unityActive.x $@

unityActive_xdr.c: unityActive.x unityActive.h
	@RPCGEN_WRAPPER@ services/plugins/unity/unityActive.x $@

unity.h: unity.x
	@RPCGEN_WRAPPER@ services/plugins/unity/unity.x $@

unity_xdr.c: unity.x unity.h
	@RPCGEN_WRAPPER@ services/plugins/unity/unity.x $@

