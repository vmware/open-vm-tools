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

noinst_LTLIBRARIES = libvix.la

libvix_la_CPPFLAGS =
libvix_la_CPPFLAGS += @PLUGIN_CPPFLAGS@

libvix_la_LDFLAGS =
libvix_la_LDFLAGS += @PLUGIN_LDFLAGS@

libvix_la_LIBADD =
libvix_la_LIBADD += @VIX_LIBADD@
libvix_la_LIBADD += @VMTOOLS_LIBS@
libvix_la_LIBADD += ../hgfsServer/libhgfsServer.la
libvix_la_LIBADD += $(top_builddir)/lib/auth/libAuth.la
libvix_la_LIBADD += $(top_builddir)/lib/foundryMsg/libFoundryMsg.la
libvix_la_LIBADD += $(top_builddir)/lib/impersonate/libImpersonate.la

libvix_la_SOURCES =
libvix_la_SOURCES += foundryToolsDaemon.c
libvix_la_SOURCES += vixPlugin.c
libvix_la_SOURCES += $(top_srcdir)/lib/vixTools/vixTools.c

install-exec-local:
	mkdir -p $(DESTDIR)$(VMSVC_PLUGIN_INSTALLDIR)
	cp -L $(builddir)/.libs/libvix.so $(DESTDIR)$(VMSVC_PLUGIN_INSTALLDIR)
if HAVE_X11
	mkdir -p $(DESTDIR)$(VMUSR_PLUGIN_INSTALLDIR)
	cp -L $(builddir)/.libs/libvix.so $(DESTDIR)$(VMUSR_PLUGIN_INSTALLDIR)
endif

