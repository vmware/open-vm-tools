################################################################################
### Copyright (C) 2014-2016 VMware, Inc.  All rights reserved.
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

lib_LTLIBRARIES = libvgauth.la

libvgauth_la_LIBADD =
libvgauth_la_LIBADD += @VGAUTH_LIBADD@
libvgauth_la_LIBADD += @GLIB2_LIBS@
libvgauth_la_LIBADD += @GTHREAD_LIBS@
libvgauth_la_LIBADD += @VMTOOLS_LIBS@
libvgauth_la_LIBADD += @SSL_LIBS@
libvgauth_la_LIBADD += -lssl
libvgauth_la_LIBADD += -lcrypto

libvgauth_la_SOURCES =
libvgauth_la_SOURCES += alias.c
libvgauth_la_SOURCES += auth.c
libvgauth_la_SOURCES += authPosix.c
libvgauth_la_SOURCES += comm.c
libvgauth_la_SOURCES += common.c
libvgauth_la_SOURCES += errortext.c
libvgauth_la_SOURCES += impersonate.c
libvgauth_la_SOURCES += impersonateLinux.c
libvgauth_la_SOURCES += netPosix.c
libvgauth_la_SOURCES += proto.c
libvgauth_la_SOURCES += ../common/audit.c
libvgauth_la_SOURCES += ../common/certverify.c
libvgauth_la_SOURCES += ../common/i18n.c
libvgauth_la_SOURCES += ../common/prefs.c
libvgauth_la_SOURCES += ../common/usercheck.c
libvgauth_la_SOURCES += ../common/VGAuthLog.c
libvgauth_la_SOURCES += ../common/VGAuthUtil.c

libvgauth_la_CPPFLAGS =
libvgauth_la_CPPFLAGS += @GLIB2_CPPFLAGS@
libvgauth_la_CPPFLAGS += -DG_LOG_DOMAIN=\"VCGA\"
libvgauth_la_CPPFLAGS += @PAM_CPPFLAGS@
libvgauth_la_CPPFLAGS += @SSL_CPPFLAGS@
libvgauth_la_CPPFLAGS += -I$(top_srcdir)/libvmtools
libvgauth_la_CPPFLAGS += -I$(top_srcdir)/vgauth/public
libvgauth_la_CPPFLAGS += -I$(top_srcdir)/vgauth/common

libvgauth_la_LDFLAGS =
# We require GCC, so we're fine passing compiler-specific flags.
libvgauth_la_LDFLAGS += -Wl,-z,defs
# Needed for OS's that don't link shared libraries against libc by
#default, e.g. FreeBSD
libvgauth_la_LDFLAGS += -Wl,-lc

# Message catalogs.
install-data-hook:
	@INSTVMSG@ libvgauth.la $(srcdir)/l10n $(DESTDIR)$(datadir)
