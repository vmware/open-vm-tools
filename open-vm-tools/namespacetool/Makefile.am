################################################################################
### Copyright (C) 2016 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmware-namespace-cmd

vmware_namespace_cmd_SOURCES =
vmware_namespace_cmd_SOURCES += namespacetool.c

vmware_namespace_cmd_CPPFLAGS =
vmware_namespace_cmd_CPPFLAGS += -DVMTOOLS_USE_GLIB
vmware_namespace_cmd_CPPFLAGS += @GLIB2_CPPFLAGS@
vmware_namespace_cmd_CPPFLAGS += -I$(top_srcdir)/libvmtools

vmware_namespace_cmd_LDADD =
vmware_namespace_cmd_LDADD += @VMTOOLS_LIBS@
vmware_namespace_cmd_LDADD += @GLIB2_LIBS@

if HAVE_ICU
   vmware_namespace_cmd_LDADD += @ICU_LIBS@
   vmware_namespace_cmd_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS)  \
                               $(LIBTOOLFLAGS) --mode=link $(CXX)       \
                               $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                               $(LDFLAGS) -o $@
else
   vmware_namespace_cmd_LINK = $(LINK)
endif