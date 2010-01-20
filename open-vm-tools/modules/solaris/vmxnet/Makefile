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

MODULE    := vmxnet
MODULE_32 := i386/$(MODULE)
MODULE_64 := amd64/$(MODULE)

CFLAGS    :=
LDFLAGS   :=

CFLAGS    += -O2
CFLAGS    += -Wall -Werror
CFLAGS    += -U_NO_LONGLONG
CFLAGS    += -D_KERNEL
CFLAGS    += -I../../../lib/include    # for buildNumber.h

##
## Objects needed to build the HGFS kernel module
##
VMXNET_OBJS := vmxnet.o

VMXNET_32_OBJS := $(addprefix i386/, $(VMXNET_OBJS))
VMXNET_64_OBJS := $(addprefix amd64/, $(VMXNET_OBJS))

ifeq ($(shell echo "$(VM_UNAME)" | cut -c-4),5.9) # {

# Solaris 9
MODULES := $(MODULE_32)

else # } {

# Assume Solaris 10 or 11.
MODULES := $(MODULE_32) $(MODULE_64)
CFLAGS  += -ffreestanding
CFLAGS  += -nodefaultlibs

endif # }

ifdef OVT_SOURCE_DIR
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/include
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/shared/vmxnet
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

all: prepare_dirs $(MODULES)

prepare_dirs:
	@echo "Creating build directories"
	mkdir -p i386
	mkdir -p amd64

$(MODULE_32): $(VMXNET_32_OBJS)
	@echo "Linking          $@"
	$(LD) $(LDFLAGS_32) -r $(VMXNET_32_OBJS) -o $@

$(VMXNET_32_OBJS): i386/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_32) -c $< -o $@

$(MODULE_64): $(VMXNET_64_OBJS)
	@echo "Linking          $@"
	$(LD) $(LDFLAGS_64) -r $(VMXNET_64_OBJS) -o $@

$(VMXNET_64_OBJS): amd64/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_64) -c $< -o $@

clean:
	@echo "Cleaning directories"
	@rm -rf $(MODULE_32) $(VMXNET_32_OBJS) i386
	@rm -rf $(MODULE_64) $(VMXNET_64_OBJS) amd64

install:
	@echo "Installing modules"
	cp $(MODULE_32) /kernel/drv/
	chmod 755 /kernel/drv/$(MODULE)
	cp $(MODULE_64) /kernel/drv/amd64
	chmod 755 /kernel/drv/amd64/$(MODULE)
