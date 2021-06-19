################################################################################
### Copyright (c) 2007-2018,2020-2021 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmware-toolbox-cmd

vmware_toolbox_cmd_LDADD =
vmware_toolbox_cmd_LDADD += ../libguestlib/libguestlib.la
vmware_toolbox_cmd_LDADD += @VMTOOLS_LIBS@
vmware_toolbox_cmd_LDADD += @XDR_LIBS@

vmware_toolbox_cmd_CPPFLAGS =
vmware_toolbox_cmd_CPPFLAGS += @VMTOOLS_CPPFLAGS@
vmware_toolbox_cmd_CPPFLAGS += @XDR_CPPFLAGS@

vmware_toolbox_cmd_SOURCES =
vmware_toolbox_cmd_SOURCES += toolbox-cmd.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-config.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-devices.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-info.c
if LINUX
   vmware_toolbox_cmd_SOURCES += toolboxcmd-gueststore.c
   vmware_toolbox_cmd_SOURCES += toolboxcmd-globalconf.c
   vmware_toolbox_cmd_LDADD   += ../lib/globalConfig/libGlobalConfig.la
   vmware_toolbox_cmd_LDADD   += @GOBJECT_LIBS@
endif
vmware_toolbox_cmd_SOURCES += toolboxcmd-logging.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-scripts.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-shrink.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-stat.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-time.c

if HAVE_ICU
   vmware_toolbox_cmd_LDADD += @ICU_LIBS@
   vmware_toolbox_cmd_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                             $(LIBTOOLFLAGS) --mode=link $(CXX) \
                             $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                             $(LDFLAGS) -o $@
else
   vmware_toolbox_cmd_LINK = $(LINK)
endif

# Message catalogs.
install-data-hook:
	@INSTVMSG@ toolboxcmd $(srcdir)/l10n $(DESTDIR)$(datadir)

