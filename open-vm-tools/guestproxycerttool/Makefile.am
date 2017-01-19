################################################################################
### Copyright (C) 2014-2016 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmware-guestproxycerttool

vmware_guestproxycerttool_LDADD =
vmware_guestproxycerttool_LDADD += @SSL_LIBS@
vmware_guestproxycerttool_LDADD += -lcrypto
vmware_guestproxycerttool_LDADD += @GLIB2_LIBS@

vmware_guestproxycerttool_CPPFLAGS =
vmware_guestproxycerttool_CPPFLAGS += @SSL_CPPFLAGS@
vmware_guestproxycerttool_CPPFLAGS += @GLIB2_CPPFLAGS@

vmware_guestproxycerttool_SOURCES =
vmware_guestproxycerttool_SOURCES += cert_key.c
vmware_guestproxycerttool_SOURCES += cert_util.c
vmware_guestproxycerttool_SOURCES += cert_tool.c

if HAVE_ICU
   vmware_guestproxycerttool_LDADD += @ICU_LIBS@
   vmware_guestproxycerttool_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                            $(LIBTOOLFLAGS) --mode=link $(CXX) \
                            $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                            $(LDFLAGS) -o $@
else
   vmware_guestproxycerttool_LINK = $(LINK)
endif
