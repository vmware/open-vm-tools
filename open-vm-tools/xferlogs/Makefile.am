################################################################################
### Copyright (c) 2007-2016,2020 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmware-xferlogs

vmware_xferlogs_LDADD =
vmware_xferlogs_LDADD += @GLIB2_LIBS@
vmware_xferlogs_LDADD += ../lib/rpcOut/libRpcOut.la
vmware_xferlogs_LDADD += ../lib/rpcVmx/libRpcVmx.la
vmware_xferlogs_LDADD += ../lib/message/libMessage.la
vmware_xferlogs_LDADD += ../lib/backdoor/libBackdoor.la
vmware_xferlogs_LDADD += ../lib/string/libString.la
vmware_xferlogs_LDADD += ../lib/vmCheck/libVmCheck.la
vmware_xferlogs_LDADD += ../lib/vmSignal/libVmSignal.la
vmware_xferlogs_LDADD += ../lib/misc/libMisc.la
vmware_xferlogs_LDADD += ../lib/stubs/libStubs.la

vmware_xferlogs_CPPFLAGS =
vmware_xferlogs_CPPFLAGS += @GLIB2_CPPFLAGS@

vmware_xferlogs_SOURCES =
vmware_xferlogs_SOURCES += xferlogs.c

if HAVE_ICU
   vmware_xferlogs_LDADD += @ICU_LIBS@
   vmware_xferlogs_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                          $(LIBTOOLFLAGS) --mode=link $(CXX) $(AM_CXXFLAGS) \
                          $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
else
   vmware_xferlogs_LINK = $(LINK)
endif
