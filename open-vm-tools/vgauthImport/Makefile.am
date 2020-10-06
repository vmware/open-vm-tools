################################################################################
### Copyright (C) 2020 VMware, Inc.  All rights reserved.
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
bin_PROGRAMS = vmware-alias-import

vmware_alias_import_LDADD =
vmware_alias_import_LDADD += @GLIB2_LIBS@
vmware_alias_import_LDADD += ../lib/rpcOut/libRpcOut.la
vmware_alias_import_LDADD += ../lib/message/libMessage.la
vmware_alias_import_LDADD += ../lib/backdoor/libBackdoor.la
vmware_alias_import_LDADD += ../lib/dataMap/libDataMap.la
vmware_alias_import_LDADD += ../lib/hashMap/libHashMap.la
vmware_alias_import_LDADD += ../lib/err/libErr.la
vmware_alias_import_LDADD += ../lib/string/libString.la
vmware_alias_import_LDADD += ../lib/vmCheck/libVmCheck.la
vmware_alias_import_LDADD += ../lib/vmSignal/libVmSignal.la
vmware_alias_import_LDADD += ../lib/misc/libMisc.la
vmware_alias_import_LDADD += ../vgauth/lib/libvgauth.la
vmware_alias_import_LDADD += ../lib/stubs/libStubs.la

vmware_alias_import_CPPFLAGS =
vmware_alias_import_CPPFLAGS += @GLIB2_CPPFLAGS@
vmware_alias_import_CPPFLAGS += -DUSE_RPCI_ONLY
vmware_alias_import_CPPFLAGS += -I$(top_srcdir)/vgauth/public

vmware_alias_import_rpcchandir = $(top_srcdir)/lib/rpcChannel

vmware_alias_import_SOURCES =
vmware_alias_import_SOURCES += vgauthImport.c
vmware_alias_import_SOURCES += $(vmware_alias_import_rpcchandir)/bdoorChannel.c
vmware_alias_import_SOURCES += $(vmware_alias_import_rpcchandir)/rpcChannel.c
if HAVE_VSOCK
vmware_alias_import_SOURCES += $(vmware_alias_import_rpcchandir)/vsockChannel.c
vmware_alias_import_SOURCES += $(vmware_alias_import_rpcchandir)/simpleSocket.c
endif
