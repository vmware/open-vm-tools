################################################################################
### Copyright (c) 2020-2021 VMware, Inc.  All rights reserved.
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

SUBDIRS =

plugindir = @VMSVC_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libserviceDiscovery.la

libserviceDiscovery_la_CPPFLAGS =
libserviceDiscovery_la_CPPFLAGS += @PLUGIN_CPPFLAGS@
libserviceDiscovery_la_CPPFLAGS += -DVMTOOLS_SERVICE_DISCOVERY_SCRIPTS=\"$(pkglibdir)/serviceDiscovery/scripts\"

libserviceDiscovery_la_LDFLAGS =
libserviceDiscovery_la_LDFLAGS += @PLUGIN_LDFLAGS@

libserviceDiscovery_la_LIBADD =
libserviceDiscovery_la_LIBADD += @VMTOOLS_LIBS@
libserviceDiscovery_la_LIBADD += @GOBJECT_LIBS@

libserviceDiscovery_la_SOURCES =
libserviceDiscovery_la_SOURCES += serviceDiscovery.c
libserviceDiscovery_la_SOURCES += serviceDiscoveryPosix.c
libserviceDiscovery_la_SOURCES += serviceDiscoveryInt.h

install-data-local:
	$(INSTALL) -d $(DESTDIR)/$(pkglibdir)/serviceDiscovery/scripts/
	$(INSTALL) -m 755 $(srcdir)/get-connection-info.sh \
			  $(DESTDIR)/$(pkglibdir)/serviceDiscovery/scripts/
	$(INSTALL) -m 755 $(srcdir)/get-listening-process-info.sh \
			  $(DESTDIR)/$(pkglibdir)/serviceDiscovery/scripts/
	$(INSTALL) -m 755 $(srcdir)/get-listening-process-perf-metrics.sh \
			  $(DESTDIR)/$(pkglibdir)/serviceDiscovery/scripts/
	$(INSTALL) -m 755 $(srcdir)/get-versions.sh \
			  $(DESTDIR)/$(pkglibdir)/serviceDiscovery/scripts/


