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

####
#### vmxnet Makefile (for FreeBSD guest OS)
####

CSRCS	= if_vxn.c
HEADERS := opt_bdg.h
HEADERS += device_if.h
HEADERS += bus_if.h
HEADERS += pci_if.h
SRCS    = $(CSRCS) $(HEADERS)
KMOD	= vmxnet
PROG    = ../$(KMOD).ko
NOMAN	= t
NO_MAN  = t
KLDMOD	= t

# Don't print a warning that the object dir wasn't specified
NOOBJ   = 1
NO_OBJ  = 1

.ifdef OVT_SOURCE_DIR
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/include
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/shared/vmxnet
.else
   CFLAGS += -Ishared
.endif

.include <bsd.kmod.mk>

