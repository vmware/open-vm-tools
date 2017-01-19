#!/usr/bin/make -f
##########################################################
# Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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

MODULE   := vmhgfs
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
CFLAGS   += $(GLOBAL_DEFS)
CFLAGS   += $(CC_WARNINGS)
CFLAGS   += -DVMX86_TOOLS
CFLAGS   += -I.

ifdef VMX86_DEVEL
CFLAGS   += -DVMX86_DEVEL
endif
ifdef VMX86_DEBUG               # For assertions
CFLAGS   += -DVMX86_DEBUG
endif
ifdef VMX86_LOG                 # For debug logging
CFLAGS   += -DVM_DEBUGGING_ON
endif

##
## Objects needed to build the HGFS kernel module
##
SOLHGFS_KRN_OBJS := debug.o
SOLHGFS_KRN_OBJS += filesystem.o
SOLHGFS_KRN_OBJS += hgfsBdGlue.o
SOLHGFS_KRN_OBJS += hgfsState.o
SOLHGFS_KRN_OBJS += kernelStubsSolaris.o
SOLHGFS_KRN_OBJS += module.o
SOLHGFS_KRN_OBJS += request.o
SOLHGFS_KRN_OBJS += vnode.o

SOLHGFS_32_KRN_OBJS := $(addprefix i386/, $(SOLHGFS_KRN_OBJS))
SOLHGFS_64_KRN_OBJS := $(addprefix amd64/, $(SOLHGFS_KRN_OBJS))

SOLHGFS_LIB_OBJS := backdoor.o
SOLHGFS_LIB_OBJS += cpName.o
SOLHGFS_LIB_OBJS += cpNameLinux.o
SOLHGFS_LIB_OBJS += hgfsBd.o
SOLHGFS_LIB_OBJS += hgfsEscape.o
SOLHGFS_LIB_OBJS += hgfsUtil.o
SOLHGFS_LIB_OBJS += message.o
SOLHGFS_LIB_OBJS += rpcout.o
SOLHGFS_LIB_OBJS += sha1.o

SOLHGFS_32_LIB_OBJS := $(addprefix i386/, $(SOLHGFS_LIB_OBJS))
SOLHGFS_32_LIB_OBJS += i386/backdoorGcc32.o

SOLHGFS_64_LIB_OBJS := $(addprefix amd64/, $(SOLHGFS_LIB_OBJS))
SOLHGFS_64_LIB_OBJS += amd64/backdoorGcc64.o

SOLHGFS_32_OBJS := $(SOLHGFS_32_KRN_OBJS) $(SOLHGFS_32_LIB_OBJS)
SOLHGFS_64_OBJS := $(SOLHGFS_64_KRN_OBJS) $(SOLHGFS_64_LIB_OBJS)

MODULE_32 := i386/$(MODULE)
MODULE_64 := amd64/$(MODULE)

ifdef OVT_SOURCE_DIR
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/include
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/backdoor
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/hgfs

   VPATH  := $(OVT_SOURCE_DIR)/lib/backdoor
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/hgfs
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/hgfsBd
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/message
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/misc
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/lib/rpcOut
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
$(MODULE_32): $(SOLHGFS_32_OBJS)
	@echo "Linking          $(MODULE_32)"
	$(LD) $(LDFLAGS_32) -r -o $(MODULE_32) $(SOLHGFS_32_OBJS)

$(SOLHGFS_32_KRN_OBJS): i386/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_32) -D_KERNEL -c $< -o $@

$(SOLHGFS_32_LIB_OBJS): i386/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_32) -c $< -o $@

# Now the same for 64 bits kernel
$(MODULE_64): $(SOLHGFS_64_OBJS)
	@echo "Linking          $(MODULE_64)"
	$(LD) $(LDFLAGS_64) -r -o $(MODULE_64) $(SOLHGFS_64_OBJS)

$(SOLHGFS_64_KRN_OBJS): amd64/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_64) -D_KERNEL -c $< -o $@

$(SOLHGFS_64_LIB_OBJS): amd64/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_64) -c $< -o $@

clean:
	@echo "Cleaning directories"
	@rm -rf $(MODULE_32) $(SOLHGFS_32_OBJS) i386
	@rm -rf $(MODULE_64) $(SOLHGFS_32_OBJS) amd64

install:
	@echo "Installing modules"
	cp $(MODULE_32) /kernel/drv/
	chmod 755 /kernel/drv/$(MODULE)
	cp $(MODULE_64) /kernel/drv/amd64
	chmod 755 /kernel/drv/amd64/$(MODULE)
