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

noinst_PROGRAMS = hgfsclient

AM_CFLAGS =
AM_CFLAGS += @COMMON_CFLAGS@

hgfsclient_LDADD =
hgfsclient_LDADD += ../lib/conf/libConf.a
hgfsclient_LDADD += ../lib/guestApp/libGuestApp.a
hgfsclient_LDADD += ../lib/hgfs/libHgfs.a
hgfsclient_LDADD += ../lib/hgfsBd/libHgfsBd.a
hgfsclient_LDADD += ../lib/rpcOut/libRpcOut.a
hgfsclient_LDADD += ../lib/strUtil/libStrUtil.a
hgfsclient_LDADD += ../lib/string/libString.a
hgfsclient_LDADD += ../lib/toolsLogger/libToolsLogger.a
hgfsclient_LDADD += ../lib/vmCheck/libVmCheck.a
hgfsclient_LDADD += ../lib/vmSignal/libVmSignal.a
# In the absence of the linker options --start-group and --end-group (which can't
# be put in LDADD), we need to bring the following libraries out of the alphebetical
# order so their symbols are properly resolved.
hgfsclient_LDADD += ../lib/backdoor/libBackdoor.a
hgfsclient_LDADD += ../lib/dict/libDict.a
hgfsclient_LDADD += ../lib/err/libErr.a
hgfsclient_LDADD += ../lib/file/libFile.a
hgfsclient_LDADD += ../lib/message/libMessage.a
hgfsclient_LDADD += ../lib/rpcVmx/libRpcVmx.a
hgfsclient_LDADD += ../lib/unicode/libUnicode.a
hgfsclient_LDADD += ../lib/sync/libSync.a
hgfsclient_LDADD += ../lib/misc/libMisc.a

hgfsclient_SOURCES =
hgfsclient_SOURCES += hgfsclient.c
hgfsclient_SOURCES += stub.c
