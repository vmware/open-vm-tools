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

bin_PROGRAMS = vmware-hgfsclient

vmware_hgfsclient_LDADD =
vmware_hgfsclient_LDADD += ../lib/conf/libConf.la
vmware_hgfsclient_LDADD += ../lib/guestApp/libGuestApp.la
vmware_hgfsclient_LDADD += ../lib/hgfs/libHgfs.la
vmware_hgfsclient_LDADD += ../lib/hgfsBd/libHgfsBd.la
vmware_hgfsclient_LDADD += ../lib/rpcOut/libRpcOut.la
vmware_hgfsclient_LDADD += ../lib/string/libString.la
vmware_hgfsclient_LDADD += ../lib/toolsLogger/libToolsLogger.la
vmware_hgfsclient_LDADD += ../lib/vmCheck/libVmCheck.la
vmware_hgfsclient_LDADD += ../lib/vmSignal/libVmSignal.la
# In the absence of the linker options --start-group and --end-group (which can't
# be put in LDADD), we need to bring the following libraries out of the alphebetical
# order so their symbols are properly resolved.
vmware_hgfsclient_LDADD += ../lib/backdoor/libBackdoor.la
vmware_hgfsclient_LDADD += ../lib/dict/libDict.la
vmware_hgfsclient_LDADD += ../lib/err/libErr.la
vmware_hgfsclient_LDADD += ../lib/file/libFile.la
vmware_hgfsclient_LDADD += ../lib/message/libMessage.la
vmware_hgfsclient_LDADD += ../lib/rpcVmx/libRpcVmx.la
vmware_hgfsclient_LDADD += ../lib/unicode/libUnicode.la
vmware_hgfsclient_LDADD += ../lib/sync/libSync.la
vmware_hgfsclient_LDADD += ../lib/misc/libMisc.la
vmware_hgfsclient_LDADD += ../lib/stubs/libStubs.la

vmware_hgfsclient_SOURCES =
vmware_hgfsclient_SOURCES += hgfsclient.c
vmware_hgfsclient_SOURCES += stub-user-util.c

if HAVE_ICU
   vmware_hgfsclient_LDADD += @ICU_LIBS@
   vmware_hgfsclient_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                            $(LIBTOOLFLAGS) --mode=link $(CXX) \
                            $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                            $(LDFLAGS) -o $@
else
   vmware_hgfsclient_LINK = $(LINK)
endif
