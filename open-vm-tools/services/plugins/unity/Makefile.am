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

libunity_la_SOURCES += unityPlugin.cpp
libunity_la_SOURCES += unityPluginEntry.cpp
libunity_la_SOURCES += unityPlugin.h
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
libunity_la_SOURCES += unitylib/unityPlatform.h
libunity_la_SOURCES += unitylib/unityPlatformX11.cc
libunity_la_SOURCES += unitylib/unityPlatformX11Settings.cc
libunity_la_SOURCES += unitylib/unityPlatformX11Window.cc
libunity_la_SOURCES += unitylib/unityX11.h
libunity_la_SOURCES += unitylib/x11Event.cc
libunity_la_SOURCES += unitylib/x11Util.cc

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
RPCGEN_SRCS += ghiTrashFolder_xdr.c
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
RPCGEN_HDRS += ghiTrashFolder.h
RPCGEN_HDRS += unityActive.h
RPCGEN_HDRS += unity.h

BUILT_SOURCES =
BUILT_SOURCES += $(RPCGEN_HDRS)
BUILT_SOURCES += $(RPCGEN_SRCS)

libunity_la_SOURCES += $(RPCGEN_SRCS)

## `make clean` should also remove the copied headers.
clean-local:
	rm -rf $(top_builddir)/lib/include/guestrpc
	if [ $(top_builddir) != $(top_srcdir) ]; then	\
	   rm -f *.x;                                   \
	   rm -f ghiCommonDefines.h;                    \
	fi

ghiGetBinaryHandlers.h: ghiGetBinaryHandlers.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiGetBinaryHandlers.x $(builddir);\
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiGetBinaryHandlers.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiGetBinaryHandlers_xdr.c: ghiGetBinaryHandlers.x ghiGetBinaryHandlers.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiGetBinaryHandlers.x

ghiGetExecInfoHash.h: ghiGetExecInfoHash.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiGetExecInfoHash.x $(builddir);  \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiGetExecInfoHash.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiGetExecInfoHash_xdr.c: ghiGetExecInfoHash.x ghiGetExecInfoHash.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiGetExecInfoHash.x

ghiProtocolHandler.h: ghiProtocolHandler.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiProtocolHandler.x $(builddir);  \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiProtocolHandler.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiProtocolHandler_xdr.c: ghiProtocolHandler.x ghiProtocolHandler.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiProtocolHandler.x

ghiStartMenu.h: ghiStartMenu.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiStartMenu.x $(builddir);        \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiStartMenu.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiStartMenu_xdr.c: ghiStartMenu.x ghiStartMenu.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiStartMenu.x

ghiShellAction.h: ghiShellAction.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiShellAction.x $(builddir);      \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiShellAction.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiShellAction_xdr.c: ghiShellAction.x ghiShellAction.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiShellAction.x

ghiTrayIcon.h: ghiTrayIcon.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiTrayIcon.x $(builddir);         \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiTrayIcon.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiTrayIcon_xdr.c: ghiTrayIcon.x ghiTrayIcon.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiTrayIcon.x

ghiSetFocusedWindow.h: ghiSetFocusedWindow.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiSetFocusedWindow.x $(builddir);         \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiSetFocusedWindow.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiSetFocusedWindow_xdr.c: ghiSetFocusedWindow.x ghiSetFocusedWindow.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiSetFocusedWindow.x

ghiSetGuestHandler.h: ghiSetGuestHandler.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiSetGuestHandler.x $(builddir);         \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiSetGuestHandler.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiSetGuestHandler_xdr.c: ghiSetGuestHandler.x ghiSetGuestHandler.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiSetGuestHandler.x

ghiSetOutlookTempFolder.h: ghiSetOutlookTempFolder.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiSetOutlookTempFolder.x $(builddir);         \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiSetOutlookTempFolder.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiSetOutlookTempFolder_xdr.c: ghiSetOutlookTempFolder.x ghiSetOutlookTempFolder.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiSetOutlookTempFolder.x

ghiTrashFolder.h: ghiTrashFolder.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiCommonDefines.h $(builddir);    \
	   cp -f $(top_srcdir)/services/plugins/unity/ghiTrashFolder.x $(builddir);         \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiTrashFolder.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiTrashFolder_xdr.c: ghiTrashFolder.x ghiTrashFolder.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiTrashFolder.x

unityActive.h: unityActive.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/unityActive.x $(builddir);         \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/unityActive.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

unityActive_xdr.c: unityActive.x unityActive.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/unityActive.x

unity.h: unity.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                                    \
	   cp -f $(top_srcdir)/services/plugins/unity/unity.x $(builddir);               \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/unity.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

unity_xdr.c: unity.x unity.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/unity.x

