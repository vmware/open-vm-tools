################################################################################
### Copyright (c) 2009-2016,2020 VMware, Inc.  All rights reserved.
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

if HAVE_FUSE
  noinst_PROGRAMS = vmware-testvmblock-fuse
  noinst_PROGRAMS += vmware-testvmblock-manual-fuse
else
  noinst_PROGRAMS = vmware-testvmblock-legacy
  noinst_PROGRAMS += vmware-testvmblock-manual-legacy
endif

AM_CFLAGS =
AM_CFLAGS += -DVMX86_DEVEL
AM_CFLAGS += -DVMX86_DEBUG

AM_LDFLAGS =
AM_LDFLAGS += -lpthread

vmware_testvmblock_legacy_SOURCES = vmblocktest.c

vmware_testvmblock_manual_legacy_SOURCES = manual-blocker.c

vmware_testvmblock_fuse_CFLAGS = $(AM_CFLAGS) -Dvmblock_fuse
vmware_testvmblock_fuse_SOURCES = vmblocktest.c

vmware_testvmblock_manual_fuse_CFLAGS = $(AM_CFLAGS) -Dvmblock_fuse
vmware_testvmblock_manual_fuse_SOURCES = manual-blocker.c
