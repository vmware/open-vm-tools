################################################################################
### Copyright (C) 2007-2019 VMware, Inc.  All rights reserved.
###
### Top-level Makefile for building the VMware OSS Tools.
###
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

# These flags get passed to aclocal when autoreconf calls it, and tell aclocal
# that all of our macros are in the 'm4' subdirectory.
ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
SUBDIRS += lib
SUBDIRS += libvmtools
SUBDIRS += libhgfs
SUBDIRS += hgfsclient
if BUILD_HGFSMOUNTER
   SUBDIRS += hgfsmounter
endif
if ENABLE_VGAUTH
   SUBDIRS += vgauth
endif
SUBDIRS += checkvm
SUBDIRS += libguestlib
if ENABLE_DEPLOYPKG
   SUBDIRS += libDeployPkg
endif
SUBDIRS += rpctool
SUBDIRS += namespacetool
SUBDIRS += scripts
SUBDIRS += services
SUBDIRS += toolbox
if HAVE_X11
   SUBDIRS += vmware-user-suid-wrapper
endif
if HAVE_FUSE
   SUBDIRS += vmblock-fuse
   SUBDIRS += vmhgfs-fuse
endif
if !LINUX
   SUBDIRS += vmblockmounter
endif
SUBDIRS += xferlogs
if ENABLE_TESTS
   SUBDIRS += tests
endif
if WITH_KERNEL_MODULES
   SUBDIRS += modules
endif
SUBDIRS += docs
if ENABLE_LIBAPPMONITOR
   SUBDIRS += libappmonitor
endif

if HAVE_UDEV
   SUBDIRS += udev
endif

install-data-local:
	$(INSTALL) -d $(DESTDIR)/etc/vmware-tools/
	$(INSTALL) $(srcdir)/tools.conf $(DESTDIR)/etc/vmware-tools/tools.conf.example

