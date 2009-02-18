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

bin_PROGRAMS = vmware-user

AM_CFLAGS =
AM_CFLAGS += @GTK_CPPFLAGS@
if HAVE_GTKMM
   AM_CFLAGS += @GTKMM_CPPFLAGS@
endif
AM_CFLAGS += @VMWARE_USER_CPPFLAGS@
AM_CFLAGS += -I$(top_builddir)/include
AM_CFLAGS += -DRESOLUTION_X11
AM_CFLAGS += -I$(top_srcdir)/services/plugins/vix

AM_CXXFLAGS = $(AM_CFLAGS)

vmware_user_LDADD =
vmware_user_LDADD += ../lib/conf/libConf.la
if HAVE_GTKMM
   vmware_user_LDADD += ../lib/dndGuest/libDndGuest.la
endif
vmware_user_LDADD += ../lib/eventManager/libEventManager.la
vmware_user_LDADD += ../lib/file/libFile.la
if ENABLE_UNITY
   vmware_user_LDADD += ../lib/ghIntegration/libGhIntegration.la
else
   vmware_user_LDADD += ../lib/ghIntegrationStub/libGhIntegrationStub.la
endif
vmware_user_LDADD += ../lib/guestApp/libGuestAppX11.la
vmware_user_LDADD += ../lib/guestInfo/libGuestInfo.la
vmware_user_LDADD += ../lib/hgfsServer/libHgfsServer.la
vmware_user_LDADD += ../lib/hgfsServerManagerGuest/libHgfsServerManagerGuest.la
vmware_user_LDADD += ../lib/hgfsServerPolicyGuest/libHgfsServerPolicyGuest.la
vmware_user_LDADD += ../lib/impersonate/libImpersonate.la
vmware_user_LDADD += ../lib/netUtil/libNetUtil.la
vmware_user_LDADD += ../lib/printer/libPrinter.la
vmware_user_LDADD += ../lib/resolution/libResolution.la
vmware_user_LDADD += ../lib/rpcIn/libRpcIn.la
vmware_user_LDADD += ../lib/rpcOut/libRpcOut.la
vmware_user_LDADD += ../lib/rpcVmx/libRpcVmx.la
vmware_user_LDADD += ../lib/SLPv2Parser/libSLPv2Parser.la
vmware_user_LDADD += ../lib/socketMgr/libSocketMgr.la
vmware_user_LDADD += ../lib/string/libString.la
vmware_user_LDADD += ../lib/sync/libSync.la
vmware_user_LDADD += ../lib/syncDriver/libSyncDriver.la
vmware_user_LDADD += ../lib/system/libSystem.la
if ENABLE_UNITY
   vmware_user_LDADD += ../lib/unity/libUnity.la
   vmware_user_LDADD += ../lib/unityWindowTracker/libUnityWindowTracker.la
else
   vmware_user_LDADD += ../lib/unityStub/libUnityStub.la
endif
vmware_user_LDADD += ../lib/guestRpc/libGuestRpc.la
vmware_user_LDADD += ../lib/user/libUser.la
vmware_user_LDADD += ../lib/vixTools/libVixTools.la
vmware_user_LDADD += ../lib/vmCheck/libVmCheck.la
vmware_user_LDADD += ../lib/vmSignal/libVmSignal.la
vmware_user_LDADD += ../lib/wiper/libWiper.la
# In the absence of the linker options --start-group and --end-group (which
# can't be put in LDADD), we need to bring the following libraries out of
# the alphabetical order so their symbols are properly resolved.
vmware_user_LDADD += ../lib/err/libErr.la
vmware_user_LDADD += ../lib/foundryMsg/libFoundryMsg.la
vmware_user_LDADD += ../lib/backdoor/libBackdoor.la
vmware_user_LDADD += ../lib/dict/libDict.la
vmware_user_LDADD += ../lib/dnd/libDnd.la
vmware_user_LDADD += ../lib/dynxdr/libDynxdr.la
vmware_user_LDADD += ../lib/hgfs/libHgfs.la
vmware_user_LDADD += ../lib/message/libMessage.la
vmware_user_LDADD += ../lib/unicode/libUnicode.la
vmware_user_LDADD += ../lib/misc/libMisc.la
vmware_user_LDADD += ../lib/procMgr/libProcMgr.la
vmware_user_LDADD += ../lib/auth/libAuth.la
vmware_user_LDADD += ../lib/panicDefault/libPanicDefault.la
vmware_user_LDADD += ../lib/panic/libPanic.la
vmware_user_LDADD += ../lib/stubs/libStubs.la
if ENABLE_UNITY
   vmware_user_LDADD += ../lib/appUtil/libAppUtil.la
   vmware_user_LDADD += ../lib/image/libImage.la
   vmware_user_LDADD += ../lib/raster/libRaster.la
   vmware_user_LDADD += ../lib/region/libRegion.la
endif
vmware_user_LDADD += -lcrypt
vmware_user_LDADD += -lstdc++
if LINUX
vmware_user_LDADD += -lgcc_s
endif
vmware_user_LDADD += @GTK_LIBS@
if HAVE_GTKMM
   vmware_user_LDADD += @GTKMM_LIBS@
endif
vmware_user_LDADD += @VMWARE_USER_LDADD@
vmware_user_LDADD += @COMMON_XLIBS@
if ENABLE_UNITY
   vmware_user_LDADD += @URIPARSER_LIBS@
   vmware_user_LDADD += @LIBPNG_LIBS@
   vmware_user_LDADD += @ZLIB_LIBS@
endif

vmware_user_SOURCES =
vmware_user_SOURCES += copyPaste.c
if HAVE_GTKMM
   vmware_user_SOURCES += copyPasteUI.cpp
endif
vmware_user_SOURCES += copyPasteWrapper.cpp
vmware_user_SOURCES += debugStdio.c
vmware_user_SOURCES += dnd.c
vmware_user_SOURCES += foreignVMToolsDaemon.c
vmware_user_SOURCES += foreignVMToolsNetworking.c
vmware_user_SOURCES += $(top_srcdir)/services/plugins/vix/foundryToolsDaemon.c
vmware_user_SOURCES += notify.c
vmware_user_SOURCES += pointer.c
if HAVE_GTKMM
   vmware_user_SOURCES += stringxx/string.cc
endif
vmware_user_SOURCES += vmware-user.cpp

if HAVE_ICU
   vmware_user_LDADD += @ICU_LIBS@
   vmware_user_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                      $(LIBTOOLFLAGS) --mode=link $(CXX) $(AM_CXXFLAGS) \
                      $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
else
   vmware_user_LINK = $(LINK)
endif

install-exec-hook:
	$(INSTALL) -d $(DESTDIR)$(datadir)/applications/
	$(INSTALL) -m 644 $(top_srcdir)/scripts/common/vmware-user.desktop \
		$(DESTDIR)$(datadir)/applications/
uninstall-hook:
	rm -f $(DESTDIR)$(datadir)/applications/vmware-user.desktop
