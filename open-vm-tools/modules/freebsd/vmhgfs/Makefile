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
HEADERS += compat_freebsd.h
HEADERS += compat_vop.h

COMMON_SRCS := cpName.c
COMMON_SRCS += cpNameLinux.c
COMMON_SRCS += dbllnklst.c
COMMON_SRCS += sha1.c
COMMON_SRCS += staticEscape.c
COMMON_SRCS += hgfsBd.c
COMMON_SRCS += rpcout.c
COMMON_SRCS += message.c
COMMON_SRCS += messageBackdoor.c
COMMON_SRCS += backdoor.c
.if $(MACHINE_ARCH) == "amd64"
COMMON_SRCS += backdoorGcc64.c
.else
COMMON_SRCS += backdoorGcc32.c
.endif

COMMON_HGFS_SRCS := debug.c
COMMON_HGFS_SRCS += request.c
COMMON_HGFS_SRCS += worker.c
COMMON_HGFS_SRCS += fsutil.c
COMMON_HGFS_SRCS += vnopscommon.c
COMMON_HGFS_SRCS += state.c

MODULE_SRCS := vnops.c
MODULE_SRCS += vfsops.c
MODULE_SRCS += os.c
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
KLDMOD  = t

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
