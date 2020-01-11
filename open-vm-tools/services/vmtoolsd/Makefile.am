################################################################################
### Copyright (C) 2009-2019 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmtoolsd
pamdir = $(PAM_PREFIX)/pam.d


vmtoolsd_CPPFLAGS =
vmtoolsd_CPPFLAGS += @VMTOOLS_CPPFLAGS@
vmtoolsd_CPPFLAGS += @GMODULE_CPPFLAGS@
vmtoolsd_CPPFLAGS += @GOBJECT_CPPFLAGS@
vmtoolsd_CPPFLAGS += @GTHREAD_CPPFLAGS@
vmtoolsd_CPPFLAGS += -I$(builddir)
vmtoolsd_CPPFLAGS += -DVMTOOLSD_PLUGIN_ROOT=\"$(pkglibdir)/plugins\"

vmtoolsd_LDADD =
vmtoolsd_LDADD += @VMTOOLS_LIBS@
vmtoolsd_LDADD += @GMODULE_LIBS@
vmtoolsd_LDADD += @GOBJECT_LIBS@
vmtoolsd_LDADD += @GTHREAD_LIBS@

vmtoolsd_SOURCES =
vmtoolsd_SOURCES += cmdLine.c
vmtoolsd_SOURCES += mainLoop.c
vmtoolsd_SOURCES += mainPosix.c
vmtoolsd_SOURCES += pluginMgr.c
vmtoolsd_SOURCES += serviceObj.c
vmtoolsd_SOURCES += threadPool.c
vmtoolsd_SOURCES += toolsRpc.c
vmtoolsd_SOURCES += svcSignals.c
vmtoolsd_SOURCES += toolsHangDetector.c

BUILT_SOURCES =
BUILT_SOURCES += svcSignals.c
BUILT_SOURCES += svcSignals.h

CLEANFILES =
CLEANFILES += svcSignals.c
CLEANFILES += svcSignals.h

EXTRA_DIST =
EXTRA_DIST += svcSignals.gm

svcSignals.c: $(top_srcdir)/services/vmtoolsd/svcSignals.gm
	glib-genmarshal --body $(top_srcdir)/services/vmtoolsd/svcSignals.gm > \
	   $@ || (rm -f $@ && exit 1)

svcSignals.h: $(top_srcdir)/services/vmtoolsd/svcSignals.gm
	glib-genmarshal --header $(top_srcdir)/services/vmtoolsd/svcSignals.gm > \
	   $@ || (rm -f $@ && exit 1)

if HAVE_ICU
   vmtoolsd_LDADD += @ICU_LIBS@
   vmtoolsd_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS)     \
                      $(LIBTOOLFLAGS) --mode=link $(CXX)       \
                      $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                      $(LDFLAGS) -o $@
else
   vmtoolsd_LINK = $(LINK)
endif

# PAM support is currently only available for Linux, so HAVE_PAM is only
# defined for that OS.
if HAVE_PAM
pam_SCRIPTS = $(top_srcdir)/scripts/linux/pam.d/vmtoolsd
endif HAVE_PAM

# Message catalogs.
install-data-hook:
	@INSTVMSG@ vmtoolsd $(srcdir)/l10n $(DESTDIR)$(datadir)

install-exec-hook:
	$(INSTALL) -d $(DESTDIR)/etc/vmware-tools

uninstall-hook:
	rm -rf $(DESTDIR)/etc/vmware-tools

