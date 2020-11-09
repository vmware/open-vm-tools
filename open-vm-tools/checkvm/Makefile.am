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

bin_PROGRAMS = vmware-checkvm

vmware_checkvm_SOURCES =
vmware_checkvm_SOURCES += checkvm.c

vmware_checkvm_LDADD =
vmware_checkvm_LDADD += @GLIB2_LIBS@
vmware_checkvm_LDADD += @VMTOOLS_LIBS@

vmware_checkvm_CPPFLAGS =
vmware_checkvm_CPPFLAGS += @GLIB2_CPPFLAGS@

if HAVE_ICU
   vmware_checkvm_LDADD += @ICU_LIBS@
   vmware_checkvm_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS)     \
                            $(LIBTOOLFLAGS) --mode=link $(CXX)       \
                            $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                            $(LDFLAGS) -o $@
else
   vmware_checkvm_LINK = $(LINK)
endif

