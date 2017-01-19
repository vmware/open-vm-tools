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

sbin_PROGRAMS = mount.vmhgfs

mount_vmhgfs_LDADD =
mount_vmhgfs_LDADD += ../lib/string/libString.la
mount_vmhgfs_LDADD += ../lib/panicDefault/libPanicDefault.la
mount_vmhgfs_LDADD += ../lib/panic/libPanic.la
mount_vmhgfs_LDADD += ../lib/lock/libLock.la
mount_vmhgfs_LDADD += ../lib/misc/libMisc.la
mount_vmhgfs_LDADD += ../lib/stubs/libStubs.la

mount_vmhgfs_SOURCES =
mount_vmhgfs_SOURCES += hgfsmounter.c

if FREEBSD
install-exec-hook:
	mv $(DESTDIR)$(sbindir)/mount.vmhgfs \
		$(DESTDIR)$(sbindir)/mount_vmhgfs
	-$(MKDIR_P) $(DESTDIR)/sbin
	-$(LN_S) $(DESTDIR)$(sbindir)/mount_vmhgfs \
		$(DESTDIR)/sbin/mount_vmhgfs &> /dev/null
uninstall-hook:
	rm -f $(DESTDIR)$(sbindir)/mount_vmhgfs
else
install-exec-hook:
	-$(MKDIR_P) $(DESTDIR)/sbin
	-$(LN_S) $(DESTDIR)$(sbindir)/mount.vmhgfs \
		$(DESTDIR)/sbin/mount.vmhgfs &> /dev/null
uninstall-hook:
	rm -f $(DESTDIR)/sbin/mount.vmhgfs
endif !FREEBSD
