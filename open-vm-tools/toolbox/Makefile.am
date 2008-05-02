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

noinst_PROGRAMS = toolbox

AM_CFLAGS =
AM_CFLAGS += @GTK_CFLAGS@
AM_CFLAGS += @COMMON_CFLAGS@

toolbox_LDADD =
toolbox_LDADD += ../lib/conf/libConf.a
toolbox_LDADD += ../lib/eventManager/libEventManager.a
toolbox_LDADD += ../lib/file/libFile.a
toolbox_LDADD += ../lib/guestApp/libGuestApp.a
toolbox_LDADD += ../lib/hgfs/libHgfs.a
toolbox_LDADD += ../lib/procMgr/libProcMgr.a
toolbox_LDADD += ../lib/rpcIn/libRpcIn.a
toolbox_LDADD += ../lib/rpcOut/libRpcOut.a
toolbox_LDADD += ../lib/strUtil/libStrUtil.a
toolbox_LDADD += ../lib/string/libString.a
toolbox_LDADD += ../lib/system/libSystem.a
toolbox_LDADD += ../lib/user/libUser.a
toolbox_LDADD += ../lib/vmCheck/libVmCheck.a
toolbox_LDADD += ../lib/vmSignal/libVmSignal.a
toolbox_LDADD += ../lib/wiper/libWiper.a
# In the absence of the linker options --start-group and --end-group (which
# can't be put in LDADD), we need to bring the following libraries out of
# the alphabetical order so their symbols are properly resolved.
toolbox_LDADD += ../lib/err/libErr.a
toolbox_LDADD += ../lib/backdoor/libBackdoor.a
toolbox_LDADD += ../lib/dict/libDict.a
toolbox_LDADD += ../lib/message/libMessage.a
toolbox_LDADD += ../lib/unicode/libUnicode.a
toolbox_LDADD += ../lib/sync/libSync.a
toolbox_LDADD += ../lib/misc/libMisc.a
toolbox_LDADD += ../lib/panicDefault/libPanicDefault.a
toolbox_LDADD += ../lib/panic/libPanic.a
toolbox_LDADD += -lX11
toolbox_LDADD += @GTK_LIBS@
toolbox_LDADD += @ICU_LIBS@

toolbox_SOURCES =
toolbox_SOURCES += debugStdio.c
toolbox_SOURCES += stub.c
toolbox_SOURCES += toolbox-gtk.c
toolbox_SOURCES += toolboxAbout.c
toolbox_SOURCES += toolboxDevices.c
toolbox_SOURCES += toolboxOptions.c
toolbox_SOURCES += toolboxScripts.c
toolbox_SOURCES += toolboxShrink.c
