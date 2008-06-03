################################################################################
### Copyright 2007 VMware, Inc.  All rights reserved.
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
mount_vmhgfs_LDADD += ../lib/string/libString.a
mount_vmhgfs_LDADD += ../lib/strUtil/libStrUtil.a

mount_vmhgfs_SOURCES =
mount_vmhgfs_SOURCES += hgfsmounter.c
mount_vmhgfs_SOURCES += stub.c

if FREEBSD
install-exec-hook:
	$(MV) $(sbindir)/mount.vmhgfs $(sbindir)/mount_vmhgfs
	chown root $(sbindir)/mount_vmhgfs
	chmod u+s $(sbindir)/mount_vmhgfs
uninstall-hook:
	$(RM) -f $(sbindir)/mount_vmhgfs
else
install-exec-hook:
	chown root $(sbindir)/mount.vmhgfs
	chmod u+s $(sbindir)/mount.vmhgfs
endif !FREEBSD
