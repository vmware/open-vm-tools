#!/usr/bin/make -f
##########################################################
# Copyright (C) 2007-2018, 2021, 2023 VMware, Inc. All rights reserved.
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

####
#### vmmemctl Makefile (for FreeBSD guest OS)
####

SRCS	:= os.c
SRCS	+= vmballoon.c
SRCS	+= backdoor_balloon.c
SRCS	+= kernelStubsBSD.c

.if $(MACHINE_ARCH) == "amd64"
SRCS    += backdoorGcc64.c
.elif $(MACHINE_ARCH) == "aarch64"
SRCS    += backdoorGcc64_arm64.c
.else
SRCS    += backdoorGcc32.c
.endif
KMOD	= vmmemctl
PROG    = ../$(KMOD).ko
MK_MAN  = no
KLDMOD	= t

# Don't print a warning that the object dir wasn't specified
NOOBJ   = 1
NO_OBJ  = 1

.ifdef OVT_SOURCE_DIR
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/include
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/backdoor
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/freebsd/shared
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/shared/vmmemctl
   CFLAGS += -isystem /usr/include
   VPATH  := $(OVT_SOURCE_DIR)/lib/backdoor
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/modules/shared/vmmemctl
.else
   CFLAGS += -Ishared
.endif

.include <bsd.kmod.mk>

.ifndef VERBOSE
.SILENT:
.endif
