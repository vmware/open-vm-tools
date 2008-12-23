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

sbin_PROGRAMS = vmware-guestd
pamdir = $(PAM_PREFIX)/pam.d

AM_CFLAGS = -I$(top_builddir)/include

vmware_guestd_LDADD =
vmware_guestd_LDADD += ../lib/backdoor/libBackdoor.la
vmware_guestd_LDADD += ../lib/conf/libConf.la
vmware_guestd_LDADD += ../lib/dnd/libDnd.la
vmware_guestd_LDADD += ../lib/err/libErr.la
vmware_guestd_LDADD += ../lib/eventManager/libEventManager.la
vmware_guestd_LDADD += ../lib/file/libFile.la
vmware_guestd_LDADD += ../lib/guestApp/libGuestApp.la
vmware_guestd_LDADD += ../lib/guestInfo/libGuestInfo.la
vmware_guestd_LDADD += ../lib/guestRpc/libGuestRpc.la
vmware_guestd_LDADD += ../lib/hgfsServer/libHgfsServer.la
vmware_guestd_LDADD += ../lib/hgfsServerManagerGuest/libHgfsServerManagerGuest.la
vmware_guestd_LDADD += ../lib/hgfsServerPolicyGuest/libHgfsServerPolicyGuest.la
vmware_guestd_LDADD += ../lib/deployPkg/libDeployPkg.la
vmware_guestd_LDADD += ../lib/impersonate/libImpersonate.la
vmware_guestd_LDADD += ../lib/netUtil/libNetUtil.la
vmware_guestd_LDADD += ../lib/printer/libPrinter.la
vmware_guestd_LDADD += ../lib/procMgr/libProcMgr.la
vmware_guestd_LDADD += ../lib/rpcIn/libRpcIn.la
vmware_guestd_LDADD += ../lib/rpcOut/libRpcOut.la
vmware_guestd_LDADD += ../lib/rpcVmx/libRpcVmx.la
vmware_guestd_LDADD += ../lib/SLPv2Parser/libSLPv2Parser.la
vmware_guestd_LDADD += ../lib/socketMgr/libSocketMgr.la
vmware_guestd_LDADD += ../lib/string/libString.la
vmware_guestd_LDADD += ../lib/sync/libSync.la
vmware_guestd_LDADD += ../lib/syncDriver/libSyncDriver.la
vmware_guestd_LDADD += ../lib/system/libSystem.la
vmware_guestd_LDADD += ../lib/user/libUser.la
vmware_guestd_LDADD += ../lib/vixTools/libVixTools.la
vmware_guestd_LDADD += ../lib/vmBackupLib/libVmBackupLib.la
vmware_guestd_LDADD += ../lib/vmCheck/libVmCheck.la
vmware_guestd_LDADD += ../lib/vmSignal/libVmSignal.la
vmware_guestd_LDADD += ../lib/wiper/libWiper.la
# In the absence of the linker options --start-group and --end-group (which
# can't be put in LDADD), we need to bring the following libraries out of the
# alphabetical order so their symbols are properly resolved.
vmware_guestd_LDADD += ../lib/dict/libDict.la
vmware_guestd_LDADD += ../lib/dynxdr/libDynxdr.la
vmware_guestd_LDADD += ../lib/hgfs/libHgfs.la
vmware_guestd_LDADD += ../lib/message/libMessage.la
vmware_guestd_LDADD += ../lib/unicode/libUnicode.la
vmware_guestd_LDADD += ../lib/misc/libMisc.la
vmware_guestd_LDADD += ../lib/foundryMsg/libFoundryMsg.la
vmware_guestd_LDADD += ../lib/auth/libAuth.la
vmware_guestd_LDADD += ../lib/panicDefault/libPanicDefault.la
vmware_guestd_LDADD += ../lib/panic/libPanic.la
vmware_guestd_LDADD += ../lib/stubs/libStubs.la
if SOLARIS
   HGFS_BD = ../lib/hgfsBd/libHgfsBd.la
else
   HGFS_BD =
endif
vmware_guestd_LDADD += $(HGFS_BD)
vmware_guestd_LDADD += @GUESTD_LDADD@

vmware_guestd_SOURCES =
vmware_guestd_SOURCES += debugStdio.c
vmware_guestd_SOURCES += foreignVMToolsDaemon.c
vmware_guestd_SOURCES += foreignVMToolsNetworking.c
vmware_guestd_SOURCES += foundryToolsDaemon.c
vmware_guestd_SOURCES += main.c
vmware_guestd_SOURCES += toolsDaemon.c

if HAVE_ICU
   vmware_guestd_LDADD += @ICU_LIBS@
   vmware_guestd_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                        $(LIBTOOLFLAGS) --mode=link $(CXX) $(AM_CXXFLAGS) \
                        $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
else
   vmware_guestd_LINK = $(LINK)
endif

if LINUX
if PAMD
if THIRTY_TWO_BIT_USERSPACE
pam_SCRIPTS = ../scripts/linux/pam.d/vmware-guestd
else
pam_SCRIPTS = ../scripts/linux/pam.d/vmware-guestd-x64
endif !THIRTY_TWO_BIT_USERSPACE
endif PAMD
endif LINUX

install-exec-hook:
	$(INSTALL) -d $(DESTDIR)/etc/vmware-tools
	echo 'disable-tools-version = "true"' > $(DESTDIR)/etc/vmware-tools/tools.conf

uninstall-hook:
	$(RM) -rf $(DESTDIR)/etc/vmware-tools
