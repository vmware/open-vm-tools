#!/usr/bin/make -f
##########################################################
# Copyright (C) 2006-2018, 2023 VMware, Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
##########################################################

####
####  VMware vmblock Makefile to be distributed externally
####

HEADERS := block.h
HEADERS += vnode_if.h
HEADERS += vmblock_k.h
HEADERS += os.h

CSRCS  := block.c
CSRCS  += os_panic.c
CSRCS  += vfsops.c
CSRCS  += vnops.c
CSRCS  += subr.c
CSRCS  += stubs.c

SRCS   := $(HEADERS) $(CSRCS)

KMOD    = vmblock
PROG    = ../$(KMOD).ko
MK_MAN  = no
KLDMOD  = t

VMBLOCK := $(MAINSRCROOT)/modules/vmblock

CFLAGS  += $(INCLUDE) -Wall -Werror

.ifdef OVT_SOURCE_DIR
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/include
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/freebsd/shared
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/freebsd/vmblock
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/shared/vmblock
   CFLAGS  += -isystem /usr/include
   VPATH  := $(OVT_SOURCE_DIR)/modules/shared/vmblock
.else
   CFLAGS += -Ishared
.endif

EXPORT_SYMS = NO

.include <bsd.kmod.mk>

