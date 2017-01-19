################################################################################
### Copyright (C) 2007-2016 VMware, Inc.  All rights reserved.
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
vmware_hgfsclient_LDADD += ../lib/hgfsBd/libHgfsBd.la
vmware_hgfsclient_LDADD += @VMTOOLS_LIBS@
vmware_hgfsclient_LDADD += @HGFS_LIBS@

vmware_hgfsclient_CPPFLAGS =
vmware_hgfsclient_CPPFLAGS += @VMTOOLS_CPPFLAGS@

vmware_hgfsclient_SOURCES =
vmware_hgfsclient_SOURCES += hgfsclient.c

if HAVE_ICU
   vmware_hgfsclient_LDADD += @ICU_LIBS@
   vmware_hgfsclient_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                            $(LIBTOOLFLAGS) --mode=link $(CXX) \
                            $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                            $(LDFLAGS) -o $@
else
   vmware_hgfsclient_LINK = $(LINK)
endif
