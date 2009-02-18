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

noinst_LTLIBRARIES = libvixUser.la

libvixUser_la_CPPFLAGS =
libvixUser_la_CPPFLAGS += @PLUGIN_CPPFLAGS@

libvixUser_la_LDFLAGS =
libvixUser_la_LDFLAGS += @PLUGIN_LDFLAGS@

libvixUser_la_LIBADD =
libvixUser_la_LIBADD += @VIX_LIBADD@
libvixUser_la_LIBADD += @COMMON_XLIBS@
libvixUser_la_LIBADD += @VMTOOLS_LIBS@
libvixUser_la_LIBADD += $(top_builddir)/lib/foundryMsg/libFoundryMsg.la
libvixUser_la_LIBADD += $(top_builddir)/lib/guestApp/libGuestAppX11.la

libvixUser_la_SOURCES =
libvixUser_la_SOURCES += vixUser.c

install-exec-local:
	mkdir -p $(DESTDIR)$(VMUSR_PLUGIN_INSTALLDIR)
	cp -L $(builddir)/.libs/libvixUser.so $(DESTDIR)$(VMUSR_PLUGIN_INSTALLDIR)

