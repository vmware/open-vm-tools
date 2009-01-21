################################################################################
### Copyright 2008 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmware-vmblock-fuse

# _XOPEN_SOURCE needs to be defined to at least 500 when compiling
# vmblock-fuse's os.h to get pthread_rwlock_t.
# -D_FILE_OFFSET_BITS=64 required for fuse.

AM_CFLAGS =
AM_CFLAGS += -Dvmblock_fuse
AM_CFLAGS += -D_XOPEN_SOURCE=500
AM_CFLAGS += -DUSERLEVEL
AM_CFLAGS += -D_FILE_OFFSET_BITS=64
AM_CFLAGS += @FUSE_CPPFLAGS@
AM_CFLAGS += @VMWARE_USER_CPPFLAGS@

vmware_vmblock_fuse_LDADD =
vmware_vmblock_fuse_LDADD += ../lib/misc/libMisc.la
vmware_vmblock_fuse_LDADD += @FUSE_LIBS@

vmware_vmblock_fuse_SOURCES =
vmware_vmblock_fuse_SOURCES += stubs.c
vmware_vmblock_fuse_SOURCES += block.c
vmware_vmblock_fuse_SOURCES += util.c
vmware_vmblock_fuse_SOURCES += fsops.c
vmware_vmblock_fuse_SOURCES += main.c

if HAVE_ICU
   vmware_vmblock_fuse_LDADD += @ICU_LIBS@
   vmware_vmblock_fuse_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                      $(LIBTOOLFLAGS) --mode=link $(CXX) $(AM_CXXFLAGS) \
                      $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
else
   vmware_vmblock_fuse_LINK = $(LINK)
endif
