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

noinst_PROGRAMS = vmware-user

AM_CFLAGS =
AM_CFLAGS += @GTK_CFLAGS@
AM_CFLAGS += @COMMON_CFLAGS@
AM_CFLAGS += @VMWARE_USER_CFLAGS@

vmware_user_LDADD =
vmware_user_LDADD += ../lib/conf/libConf.a
vmware_user_LDADD += ../lib/dnd/libDnd.a
vmware_user_LDADD += ../lib/eventManager/libEventManager.a
vmware_user_LDADD += ../lib/file/libFile.a
vmware_user_LDADD += ../lib/ghIntegrationStub/libghIntegrationStub.a
vmware_user_LDADD += ../lib/guestApp/libGuestApp.a
vmware_user_LDADD += ../lib/guestInfo/libGuestInfo.a
vmware_user_LDADD += ../lib/hgfsServer/libHgfsServer.a
vmware_user_LDADD += ../lib/hgfsServerManagerGuest/libHgfsServerManagerGuest.a
vmware_user_LDADD += ../lib/hgfsServerPolicyGuest/libHgfsServerPolicyGuest.a
vmware_user_LDADD += ../lib/impersonate/libImpersonate.a
vmware_user_LDADD += ../lib/netUtil/libNetUtil.a
vmware_user_LDADD += ../lib/printer/libPrinter.a
vmware_user_LDADD += ../lib/rpcIn/libRpcIn.a
vmware_user_LDADD += ../lib/rpcOut/libRpcOut.a
vmware_user_LDADD += ../lib/rpcVmx/libRpcVmx.a
vmware_user_LDADD += ../lib/SLPv2Parser/libSLPv2Parser.a
vmware_user_LDADD += ../lib/socketMgr/libSocketMgr.a
vmware_user_LDADD += ../lib/strUtil/libStrUtil.a
vmware_user_LDADD += ../lib/string/libString.a
vmware_user_LDADD += ../lib/sync/libSync.a
vmware_user_LDADD += ../lib/syncDriver/libSyncDriver.a
vmware_user_LDADD += ../lib/system/libSystem.a
vmware_user_LDADD += ../lib/unityStub/libunityStub.a
vmware_user_LDADD += ../lib/user/libUser.a
vmware_user_LDADD += ../lib/vixTools/libVixTools.a
vmware_user_LDADD += ../lib/vmCheck/libVmCheck.a
vmware_user_LDADD += ../lib/vmSignal/libVmSignal.a
vmware_user_LDADD += ../lib/wiper/libWiper.a
# In the absence of the linker options --start-group and --end-group (which
# can't be put in LDADD), we need to bring the following libraries out of
# the alphabetical order so their symbols are properly resolved.
vmware_user_LDADD += ../lib/err/libErr.a
vmware_user_LDADD += ../lib/foundryMsg/libFoundryMsg.a
vmware_user_LDADD += ../lib/backdoor/libBackdoor.a
vmware_user_LDADD += ../lib/dict/libDict.a
vmware_user_LDADD += ../lib/hgfs/libHgfs.a
vmware_user_LDADD += ../lib/message/libMessage.a
vmware_user_LDADD += ../lib/unicode/libUnicode.a
vmware_user_LDADD += ../lib/misc/libMisc.a
vmware_user_LDADD += ../lib/procMgr/libProcMgr.a
vmware_user_LDADD += ../lib/auth/libAuth.a
vmware_user_LDADD += ../lib/panicDefault/libPanicDefault.a
vmware_user_LDADD += ../lib/panic/libPanic.a
vmware_user_LDADD += -lcrypt
vmware_user_LDADD += @GTK_LIBS@
vmware_user_LDADD += @VMWARE_USER_LDADD@
vmware_user_LDADD += @COMMON_XLIBS@

vmware_user_SOURCES =
vmware_user_SOURCES += copyPaste.c
vmware_user_SOURCES += debugStdio.c
vmware_user_SOURCES += dnd.c
vmware_user_SOURCES += foreignVMToolsDaemon.c
vmware_user_SOURCES += foreignVMToolsNetworking.c
vmware_user_SOURCES += foundryToolsDaemon.c
vmware_user_SOURCES += libvmwarectrl.c
vmware_user_SOURCES += pointer.c
vmware_user_SOURCES += resolution.c
vmware_user_SOURCES += stub.c
vmware_user_SOURCES += vmware-user.c
