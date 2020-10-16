################################################################################
### Copyright (C) 2009-2016,2020 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmware-rpctool

vmware_rpctool_SOURCES =
vmware_rpctool_SOURCES += rpctool.c

vmware_rpctool_LDADD =
vmware_rpctool_LDADD += ../lib/rpcOut/libRpcOut.la
vmware_rpctool_LDADD += ../lib/message/libMessage.la
vmware_rpctool_LDADD += ../lib/backdoor/libBackdoor.la
vmware_rpctool_LDADD += ../lib/string/libString.la
vmware_rpctool_LDADD += ../lib/vmCheck/libVmCheck.la
vmware_rpctool_LDADD += ../lib/vmSignal/libVmSignal.la
vmware_rpctool_LDADD += ../lib/misc/libMisc.la
vmware_rpctool_LDADD += ../lib/stubs/libStubs.la

