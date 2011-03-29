#!/usr/bin/make -f
##########################################################
# Copyright (C) 2009 VMware, Inc. All rights reserved.
#
# The contents of this file are subject to the terms of the Common
# Development and Distribution License (the "License") version 1.0
# and no later version.  You may not use this file except in
# compliance with the License.
#
# You can obtain a copy of the License at
#         http://www.opensource.org/licenses/cddl1.php
#
# See the License for the specific language governing permissions
# and limitations under the License.
#
##########################################################

##
## General build locations and variables
##

MODULE   := vmblock
CFLAGS   :=
LDFLAGS  :=

# Solaris version defines for in-code #ifdefs (default to Solaris 9)
ifeq ($(shell echo "$(VM_UNAME)" | cut -c-4),5.11)
CFLAGS   += -DSOL11
else
ifeq ($(shell echo "$(VM_UNAME)" | cut -c-4),5.10)
CFLAGS   += -DSOL10
else
CFLAGS   += -DSOL9
# we don't support Solaris 9 (we could but we use memcpy/memset
# and we'd need stubs to get the module to load into the kernel).
$(error "This makefile is only used for Solaris 10 and 11")
endif
endif

CFLAGS   += -ffreestanding
CFLAGS   += -nodefaultlibs

CFLAGS   += -O2
CFLAGS   += -Wall -Werror
CFLAGS   += -I.

##
## Objects needed to build vmblock kernel module
##
VMBLOCK_OBJS := block.o
VMBLOCK_OBJS += module.o
VMBLOCK_OBJS += stubs.o
VMBLOCK_OBJS += vfsops.o
VMBLOCK_OBJS += vnops.o

VMBLOCK_32_OBJS := $(addprefix i386/, $(VMBLOCK_OBJS))
VMBLOCK_64_OBJS := $(addprefix amd64/, $(VMBLOCK_OBJS))

MODULE_32 := i386/$(MODULE)
MODULE_64 := amd64/$(MODULE)

ifdef OVT_SOURCE_DIR
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/include
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/shared/vmblock
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/solaris/vmblock
   VPATH  := $(OVT_SOURCE_DIR)/modules/shared/vmblock
endif

CFLAGS_32  := $(CFLAGS)
CFLAGS_32  += -m32
LDFLAGS_32 := $(LDFLAGS)

CFLAGS_64  := $(CFLAGS)
CFLAGS_64  += -m64
CFLAGS_64  += -mcmodel=kernel
CFLAGS_64  += -mno-red-zone
LDFLAGS_64 := $(LDFLAGS)
ifdef HAVE_GNU_LD
LDFLAGS_64 += -m elf_x86_64
else
LDFLAGS_64 += -64
endif

##
## Building targets
##
.PHONY: all prepare_dirs clean install

all: prepare_dirs $(MODULE_32) $(MODULE_64)

prepare_dirs:
	@echo "Creating build directories"
	mkdir -p i386
	mkdir -p amd64

# Build just the module for 32 bits kernel
$(MODULE_32): $(VMBLOCK_32_OBJS)
	@echo "Linking          $(MODULE_32)"
	$(LD) $(LDFLAGS_32) -r -o $(MODULE_32) $(VMBLOCK_32_OBJS)

$(VMBLOCK_32_OBJS): i386/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_32) -D_KERNEL -c $< -o $@

# Now the same for 64 bits kernel
$(MODULE_64): $(VMBLOCK_64_OBJS)
	@echo "Linking          $(MODULE_64)"
	$(LD) $(LDFLAGS_64) -r -o $(MODULE_64) $(VMBLOCK_64_OBJS)

$(VMBLOCK_64_OBJS): amd64/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_64) -D_KERNEL -c $< -o $@

clean:
	@echo "Cleaning directories"
	@rm -rf $(MODULE_32) $(VMBLOCK_32_OBJS) i386
	@rm -rf $(MODULE_64) $(VMBLOCK_32_OBJS) amd64

install:
	@echo "Installing modules"
	cp $(MODULE_32) /kernel/drv/
	chmod 755 /kernel/drv/$(MODULE)
	cp $(MODULE_64) /kernel/drv/amd64
	chmod 755 /kernel/drv/amd64/$(MODULE)
