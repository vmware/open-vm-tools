################################################################################
### Copyright (c) 2009-2024 Broadcom. All Rights Reserved.
### Broadcom Confidential. The term "Broadcom" refers to Broadcom Inc.
### and/or its subsidiaries.
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
SUBDIRS += vmrpcdbg
SUBDIRS += testDebug
SUBDIRS += testPlugin
SUBDIRS += testVmblock





STATICLIBS = *.a
LTLIBS     = *.la

install-exec-local:
	rm -f $(DESTDIR)$(TEST_PLUGIN_INSTALLDIR)/$(STATICLIBS)
	rm -f $(DESTDIR)$(TEST_PLUGIN_INSTALLDIR)/$(LTLIBS)
