################################################################################
### Copyright (C) 2011-2016 VMware, Inc.  All rights reserved.
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

sbin_PROGRAMS = mount.vmblock

mount_vmblock_LDADD =
mount_vmblock_LDADD += ../lib/stubs/libStubs.la

mount_vmblock_SOURCES =
mount_vmblock_SOURCES += vmblockmounter.c

if FREEBSD
install-exec-hook:
	mv $(DESTDIR)$(sbindir)/mount.vmblock \
		$(DESTDIR)$(sbindir)/mount_vmblock
	-$(MKDIR_P) $(DESTDIR)/sbin
	-$(LN_S) $(DESTDIR)$(sbindir)/mount_vmblock \
		$(DESTDIR)/sbin/mount_vmblock &> /dev/null
uninstall-hook:
	rm -f $(DESTDIR)$(sbindir)/mount_vmblock
else
install-exec-hook:
	-$(MKDIR_P) $(DESTDIR)/sbin
	-$(LN_S) $(DESTDIR)$(sbindir)/mount.vmblock \
		$(DESTDIR)/sbin/mount.vmblock &> /dev/null
uninstall-hook:
	rm -f $(DESTDIR)/sbin/mount.vmblock
endif !FREEBSD
