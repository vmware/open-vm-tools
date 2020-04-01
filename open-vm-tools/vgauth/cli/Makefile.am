################################################################################
### Copyright (C) 2014-2016, 2020 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmware-vgauth-cmd

vmware_vgauth_cmd_SOURCES =
vmware_vgauth_cmd_SOURCES += main.c

vmware_vgauth_cmd_CPPFLAGS =
vmware_vgauth_cmd_CPPFLAGS += -DVMTOOLS_USE_GLIB
vmware_vgauth_cmd_CPPFLAGS += @GLIB2_CPPFLAGS@
vmware_vgauth_cmd_CPPFLAGS += -I$(top_srcdir)/libvmtools
vmware_vgauth_cmd_CPPFLAGS += -I$(top_srcdir)/vgauth/public
vmware_vgauth_cmd_CPPFLAGS += -I$(top_srcdir)/vgauth/common

vmware_vgauth_cmd_LDADD =
vmware_vgauth_cmd_LDADD += @VMTOOLS_LIBS@
vmware_vgauth_cmd_LDADD += @GLIB2_LIBS@
vmware_vgauth_cmd_LDADD += @GTHREAD_LIBS@
vmware_vgauth_cmd_LDADD += @SSL_LIBS@
vmware_vgauth_cmd_LDADD += ../lib/libvgauth.la
vmware_vgauth_cmd_LDADD += -lssl
vmware_vgauth_cmd_LDADD += -lcrypto

if HAVE_ICU
   vmware_vgauth_cmd_LDADD += @ICU_LIBS@
   vmware_vgauth_cmd_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS)     \
                            $(LIBTOOLFLAGS) --mode=link $(CXX)       \
                            $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                            $(LDFLAGS) -o $@
else
   vmware_vgauth_cmd_LINK = $(LINK)
endif

# Message catalogs.
# rename file name to "VGAuthCli" which used in main
install-data-hook:
	@INSTVMSG@ VGAuthCli $(srcdir)/l10n $(DESTDIR)$(datadir)