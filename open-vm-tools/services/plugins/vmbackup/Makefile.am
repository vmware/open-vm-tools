################################################################################
### Copyright 2009 VMware, Inc.  All rights reserved.
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

libvmbackup_la_LDFLAGS =
libvmbackup_la_LDFLAGS += @PLUGIN_LDFLAGS@

libvmbackup_la_LIBADD =
libvmbackup_la_LIBADD += @VMTOOLS_LIBS@

libvmbackup_la_SOURCES =
libvmbackup_la_SOURCES += scriptOps.c
libvmbackup_la_SOURCES += stateMachine.c
libvmbackup_la_SOURCES += syncDriverOps.c

