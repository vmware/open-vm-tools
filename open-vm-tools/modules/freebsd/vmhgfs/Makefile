#!/usr/bin/make -f
##########################################################
# Copyright (C) 2007 VMware, Inc. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation version 2 and no later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#
##########################################################

HEADERS	:= vnode_if.h
HEADERS += hgfs_kernel.h
HEADERS += request.h
HEADERS += requestInt.h
HEADERS += debug.h
HEADERS += state.h
HEADERS += kernelStubs.h
HEADERS += fsutil.h
HEADERS += vnopscommon.h
HEADERS += os.h
HEADERS += vfsopscommon.h
HEADERS += transport.h
HEADERS += channel.h

COMMON_SRCS := cpName.c
COMMON_SRCS += cpNameLinux.c
COMMON_SRCS += cpNameLite.c
COMMON_SRCS += sha1.c
COMMON_SRCS += hgfsEscape.c
COMMON_SRCS += hgfsBd.c
COMMON_SRCS += rpcout.c
COMMON_SRCS += message.c
COMMON_SRCS += backdoor.c
.if $(MACHINE_ARCH) == "amd64"
COMMON_SRCS += backdoorGcc64.c
.else
COMMON_SRCS += backdoorGcc32.c
.endif

COMMON_HGFS_SRCS := debug.c
COMMON_HGFS_SRCS := bdhandler.c
COMMON_HGFS_SRCS += request.c
COMMON_HGFS_SRCS += worker.c
COMMON_HGFS_SRCS += fsutil.c
COMMON_HGFS_SRCS += vnopscommon.c
COMMON_HGFS_SRCS += state.c
COMMON_HGFS_SRCS += vfsopscommon.c
COMMON_HGFS_SRCS += transport.c

MODULE_SRCS := vnops.c
MODULE_SRCS += vfsops.c
MODULE_SRCS += os.c
MODULE_SRCS += vmci.c
MODULE_SRCS += hgfsUtil.c
MODULE_SRCS += kernelStubsBSD.c

#
# The FreeBSD kernel module build tree iterates over the following variable
# for build targets.  C files will be compiled, and headers will just be
# tested for presence.
#
SRCS = $(MODULE_SRCS) $(COMMON_SRCS) $(COMMON_HGFS_SRCS) $(HEADERS)

KMOD    = vmhgfs
PROG    = ../$(KMOD).ko
NOMAN   = t
NO_MAN  = t
KLDMOD  = t
NOOBJ   = 1
NO_OBJ  = 1

.ifdef OVT_SOURCE_DIR
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/include
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/backdoor
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/hgfs
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/freebsd/shared
   VPATH  := $(OVT_SOURCE_DIR)/lib/backdoor
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/hgfs
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/hgfsBd
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/message
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/misc
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/rpcOut
.else
   CFLAGS += -Ishared
.endif

#
# FreeBSD's kernel module build system defines a bunch of additional warning
# flags for the compiler in addition to -Wall -Werror.  However, some of these,
# like -Wredundant-decls, are overkill.  To get around this, I copied their list
# of warning flags, but explicitly disabled a few.
#
CWARNFLAGS := -Wall
CWARNFLAGS += -Werror
CWARNFLAGS += -Wno-redundant-decls
CWARNFLAGS += -Wnested-externs
CWARNFLAGS += -Wstrict-prototypes
CWARNFLAGS += -Wno-missing-prototypes
CWARNFLAGS += -Wpointer-arith
CWARNFLAGS += -Winline
CWARNFLAGS += -Wcast-qual

EXPORT_SYMS = NO

.include <bsd.kmod.mk>
