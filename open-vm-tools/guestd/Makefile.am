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

###
### This Makefile describes 
###

noinst_PROGRAMS = guestd

AM_CFLAGS =
AM_CFLAGS += @COMMON_CFLAGS@

guestd_LDADD =
guestd_LDADD += ../lib/backdoor/libBackdoor.a
guestd_LDADD += ../lib/conf/libConf.a
guestd_LDADD += ../lib/dnd/libDnd.a
guestd_LDADD += ../lib/err/libErr.a
guestd_LDADD += ../lib/eventManager/libEventManager.a
guestd_LDADD += ../lib/file/libFile.a
guestd_LDADD += ../lib/guestApp/libGuestApp.a
guestd_LDADD += ../lib/guestInfo/libGuestInfo.a
guestd_LDADD += ../lib/hgfsServer/libHgfsServer.a
guestd_LDADD += ../lib/hgfsServerManagerGuest/libHgfsServerManagerGuest.a
guestd_LDADD += ../lib/hgfsServerPolicyGuest/libHgfsServerPolicyGuest.a
guestd_LDADD += ../lib/deployPkg/libDeployPkg.a
guestd_LDADD += ../lib/impersonate/libImpersonate.a
guestd_LDADD += ../lib/netUtil/libNetUtil.a
guestd_LDADD += ../lib/printer/libPrinter.a
guestd_LDADD += ../lib/procMgr/libProcMgr.a
guestd_LDADD += ../lib/rpcIn/libRpcIn.a
guestd_LDADD += ../lib/rpcOut/libRpcOut.a
guestd_LDADD += ../lib/rpcVmx/libRpcVmx.a
guestd_LDADD += ../lib/SLPv2Parser/libSLPv2Parser.a
guestd_LDADD += ../lib/socketMgr/libSocketMgr.a
guestd_LDADD += ../lib/strUtil/libStrUtil.a
guestd_LDADD += ../lib/string/libString.a
guestd_LDADD += ../lib/sync/libSync.a
guestd_LDADD += ../lib/syncDriver/libSyncDriver.a
guestd_LDADD += ../lib/system/libSystem.a
guestd_LDADD += ../lib/user/libUser.a
guestd_LDADD += ../lib/vixTools/libVixTools.a
guestd_LDADD += ../lib/vmBackupLib/libVmBackupLib.a
guestd_LDADD += ../lib/vmCheck/libVmCheck.a
guestd_LDADD += ../lib/vmSignal/libVmSignal.a
guestd_LDADD += ../lib/wiper/libWiper.a
# In the absence of the linker options --start-group and --end-group (which
# can't be put in LDADD), we need to bring the following libraries out of the
# alphabetical order so their symbols are properly resolved.
guestd_LDADD += ../lib/dict/libDict.a
guestd_LDADD += ../lib/hgfs/libHgfs.a
guestd_LDADD += ../lib/message/libMessage.a
guestd_LDADD += ../lib/unicode/libUnicode.a
guestd_LDADD += ../lib/misc/libMisc.a
guestd_LDADD += ../lib/foundryMsg/libFoundryMsg.a
guestd_LDADD += ../lib/auth/libAuth.a
guestd_LDADD += ../lib/panicDefault/libPanicDefault.a
guestd_LDADD += ../lib/panic/libPanic.a
if SOLARIS
   HGFS_BD = ../lib/hgfsBd/libHgfsBd.a
else
   HGFS_BD =
endif
guestd_LDADD += $(HGFS_BD)
guestd_LDADD += @ICU_LIBS@
guestd_LDADD += @GUESTD_LDADD@

guestd_SOURCES =
guestd_SOURCES += debugStdio.c
guestd_SOURCES += foreignVMToolsDaemon.c
guestd_SOURCES += foreignVMToolsNetworking.c
guestd_SOURCES += foundryToolsDaemon.c
guestd_SOURCES += main.c
guestd_SOURCES += stub.c
guestd_SOURCES += toolsDaemon.c
