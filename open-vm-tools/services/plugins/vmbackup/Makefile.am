################################################################################
### Copyright (C) 2009-2018 VMware, Inc.  All rights reserved.
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

plugindir = @VMSVC_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libvmbackup.la

libvmbackup_la_CPPFLAGS =
libvmbackup_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libvmbackup_la_CPPFLAGS += @XDR_CPPFLAGS@

libvmbackup_la_LDFLAGS =
libvmbackup_la_LDFLAGS += @PLUGIN_LDFLAGS@

libvmbackup_la_LIBADD =
libvmbackup_la_LIBADD += @GOBJECT_LIBS@
libvmbackup_la_LIBADD += @VMTOOLS_LIBS@
libvmbackup_la_LIBADD += @XDR_LIBS@

libvmbackup_la_SOURCES =
libvmbackup_la_SOURCES += nullProvider.c
libvmbackup_la_SOURCES += scriptOps.c
libvmbackup_la_SOURCES += stateMachine.c
libvmbackup_la_SOURCES += syncDriverOps.c
if LINUX
libvmbackup_la_SOURCES += syncManifest.c
endif
libvmbackup_la_SOURCES += vmBackupSignals.c

BUILT_SOURCES =
BUILT_SOURCES += vmBackupSignals.c
BUILT_SOURCES += vmBackupSignals.h
BUILT_SOURCES += guestQuiesce.h
BUILT_SOURCES += guestQuiesce_xdr.c

CLEANFILES =
CLEANFILES += vmBackupSignals.c
CLEANFILES += vmBackupSignals.h
CLEANFILES += guestQuiesce.h
CLEANFILES += guestQuiesce_xdr.c

EXTRA_DIST =
EXTRA_DIST += vmBackupSignals.gm

libvmbackup_la_SOURCES += guestQuiesce_xdr.c

guestQuiesce.h: guestQuiesce.x
		@RPCGEN_WRAPPER@ services/plugins/vmbackup/guestQuiesce.x $@

guestQuiesce_xdr.c: guestQuiesce.x guestQuiesce.h
		@RPCGEN_WRAPPER@ services/plugins/vmbackup/guestQuiesce.x $@

vmBackupSignals.c: $(top_srcdir)/services/plugins/vmbackup/vmBackupSignals.gm
	glib-genmarshal --body $(top_srcdir)/services/plugins/vmbackup/vmBackupSignals.gm > \
	   $@ || (rm -f $@ && exit 1)

vmBackupSignals.h: $(top_srcdir)/services/plugins/vmbackup/vmBackupSignals.gm
	glib-genmarshal --header $(top_srcdir)/services/plugins/vmbackup/vmBackupSignals.gm > \
	   $@ || (rm -f $@ && exit 1)

