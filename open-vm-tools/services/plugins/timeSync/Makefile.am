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

noinst_LTLIBRARIES = libtimeSync.la

libtimeSync_la_CPPFLAGS =
libtimeSync_la_CPPFLAGS += @PLUGIN_CPPFLAGS@

libtimeSync_la_LDFLAGS =
libtimeSync_la_LDFLAGS += @PLUGIN_LDFLAGS@

libtimeSync_la_LIBADD =
libtimeSync_la_LIBADD += @VMTOOLS_LIBS@

libtimeSync_la_SOURCES =
libtimeSync_la_SOURCES += timeSync.c

install-exec-local:
	mkdir -p $(DESTDIR)$(VMSVC_PLUGIN_INSTALLDIR)
	cp -L $(builddir)/.libs/libtimeSync.so $(DESTDIR)$(VMSVC_PLUGIN_INSTALLDIR)

