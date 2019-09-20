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

SUBDIRS =
if HAVE_X11
   SUBDIRS += desktopEvents
endif
if HAVE_GTKMM
   SUBDIRS += dndcp
endif
SUBDIRS += guestInfo
SUBDIRS += hgfsServer
SUBDIRS += powerOps
if ENABLE_DEPLOYPKG
   SUBDIRS += deployPkg
endif
if HAVE_X11
   SUBDIRS += resolutionSet
endif
if ENABLE_RESOLUTIONKMS
if !HAVE_X11
# resolutionKMS needs resolutionCommon, which lives in
# the resolutionSet sub directory
   SUBDIRS += resolutionSet
endif
SUBDIRS += resolutionKMS
endif
SUBDIRS += timeSync
SUBDIRS += vix
SUBDIRS += vmbackup

#
# plugin_LTLIBRARIES causes both .la and .so files to be installed to the
# plugin directories. Clean up the .la files and keep just the shared
# libraries around. Sometimes, even though we're passing "-shared" to
# libtool, .a files are also generated, so clean up those too.
#
install-exec-local:
	rm -f $(DESTDIR)$(VMSVC_PLUGIN_INSTALLDIR)/*.a
	rm -f $(DESTDIR)$(VMSVC_PLUGIN_INSTALLDIR)/*.la
	rm -f $(DESTDIR)$(VMUSR_PLUGIN_INSTALLDIR)/*.a
	rm -f $(DESTDIR)$(VMUSR_PLUGIN_INSTALLDIR)/*.la

