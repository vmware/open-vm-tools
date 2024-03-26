################################################################################
### Copyright (C) 2007-2017 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmware-user-suid-wrapper

AM_CPPFLAGS =
AM_CPPFLAGS += -DVMTOOLSD_PATH=\"$(bindir)/vmtoolsd\"
# See post-install on Makefile of ports.
AM_CPPFLAGS += -DVMWARE_KMOD_DIR=\"$(prefix)/lib/vmware-tools/modules/drivers\"

vmware_user_suid_wrapper_SOURCES =
vmware_user_suid_wrapper_SOURCES += main.c
vmware_user_suid_wrapper_SOURCES += wrapper-@TARGET_OS@.c

vmware_user_suid_wrapper_LDADD =
vmware_user_suid_wrapper_LDADD += ../lib/vmSignal/libVmSignal.la

desktopfile = vmware-user.desktop
desktopfilesrc = $(top_srcdir)/vmware-user-suid-wrapper/vmware-user.desktop.in

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = $(desktopfile)

CLEANFILES = $(desktopfile)

$(desktopfile): $(desktopfilesrc)
	sed 's,_bindir_,$(bindir),' $(desktopfilesrc) > $(desktopfile)

# The desktopEvents plugin will look for 'vmware-user'
# to restart.
install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	  ln -fs vmware-user-suid-wrapper \
	  vmware-user

uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/vmware-user

