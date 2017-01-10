################################################################################
### Copyright (C) 2015-2016 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmhgfs-fuse

AM_CFLAGS =
AM_CFLAGS += @FUSE_CPPFLAGS@
AM_CFLAGS += @GLIB2_CPPFLAGS@

vmhgfs_fuse_LDADD =
vmhgfs_fuse_LDADD += @FUSE_LIBS@
vmhgfs_fuse_LDADD += @GLIB2_LIBS@
vmhgfs_fuse_LDADD += @VMTOOLS_LIBS@

# The linker processes the libraries in sequence, and order matters here.
vmhgfs_fuse_LDADD += ../lib/hgfs/libHgfs.la
vmhgfs_fuse_LDADD += ../lib/hgfsBd/libHgfsBd.la
vmhgfs_fuse_LDADD += ../lib/rpcOut/libRpcOut.la
vmhgfs_fuse_LDADD += ../lib/message/libMessage.la
vmhgfs_fuse_LDADD += ../lib/backdoor/libBackdoor.la

vmhgfs_fuse_LDADD += ../lib/string/libString.la

vmhgfs_fuse_SOURCES =
vmhgfs_fuse_SOURCES += bdhandler.c
vmhgfs_fuse_SOURCES += cache.c
vmhgfs_fuse_SOURCES += config.c
vmhgfs_fuse_SOURCES += dir.c
vmhgfs_fuse_SOURCES += file.c
vmhgfs_fuse_SOURCES += filesystem.c
vmhgfs_fuse_SOURCES += fsutil.c
vmhgfs_fuse_SOURCES += link.c
vmhgfs_fuse_SOURCES += main.c
vmhgfs_fuse_SOURCES += request.c
vmhgfs_fuse_SOURCES += session.c
vmhgfs_fuse_SOURCES += transport.c

#vmhgfs_fuse_SOURCES += stubs.c
vmhgfs_fuse_SOURCES += $(top_srcdir)/lib/stubs/stub-debug.c
vmhgfs_fuse_SOURCES += $(top_srcdir)/lib/stubs/stub-log.c
vmhgfs_fuse_SOURCES += $(top_srcdir)/lib/stubs/stub-panic.c

