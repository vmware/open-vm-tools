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

MODULE    := vmmemctl
MODULE_32 := i386/$(MODULE)
MODULE_64 := amd64/$(MODULE)

CFLAGS    :=
KFLAGS    :=
LDFLAGS   :=

CFLAGS    += -O
CFLAGS    += -Wall -Wno-unknown-pragmas -Werror
CFLAGS    += -I../../../lib/include    # for buildNumber.h

KFLAGS    += -D_KERNEL

##
## Objects needed to build the vmmemctl kernel module
##
VMMCTL_OBJS := vmballoon.o
VMMCTL_OBJS += vmballoon_kstats.o
VMMCTL_OBJS += os.o
VMMCTL_OBJS += backdoor_balloon.o
VMMCTL_OBJS += kernelStubsSolaris.o

VMMCTL_32_OBJS := $(addprefix i386/, $(VMMCTL_OBJS))
VMMCTL_32_OBJS += i386/backdoorGcc32.o

VMMCTL_64_OBJS := $(addprefix amd64/, $(VMMCTL_OBJS))
VMMCTL_64_OBJS += amd64/backdoorGcc64.o

ifeq ($(shell echo "$(VM_UNAME)" | cut -c-4),5.10)

# Solaris 10
SUPPORTED := 1
MODULES := $(MODULE_32) $(MODULE_64)
INSTALL := install32 install64
CFLAGS  += -DSOL10
KFLAGS  += -ffreestanding
KFLAGS  += -nodefaultlibs

endif

ifeq ($(shell echo "$(VM_UNAME)" | cut -c-4),5.11)

# Solaris 11
SUPPORTED := 1
MODULES := $(MODULE_32) $(MODULE_64)
INSTALL := install32 install64
CFLAGS  += -DSOL11
KFLAGS  += -ffreestanding
KFLAGS  += -nodefaultlibs

endif

$(if $(SUPPORTED),,$(error "Unsupported Solaris version: $(VM_UNAME)"))


ifdef OVT_SOURCE_DIR
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/include
   CFLAGS += -I$(OVT_SOURCE_DIR)/lib/backdoor
   CFLAGS += -I$(OVT_SOURCE_DIR)/modules/shared/vmmemctl

   VPATH  := $(OVT_SOURCE_DIR)/lib/backdoor
   VPATH  := $(VPATH):$(OVT_SOURCE_DIR)/modules/shared/vmmemctl
endif

CFLAGS_32  := $(CFLAGS)
CFLAGS_32  += -m32
LDFLAGS_32 := $(LDFLAGS)

CFLAGS_64  := $(CFLAGS)
CFLAGS_64  += -m64

KFLAGS_32  := $(KFLAGS)

KFLAGS_64  := $(KFLAGS)
KFLAGS_64  += -mcmodel=kernel
KFLAGS_64  += -mno-red-zone

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

$(MODULE_32): $(VMMCTL_32_OBJS)
	@echo "Linking          $@"
	$(LD) $(LDFLAGS_32) -r $(VMMCTL_32_OBJS) -o $@

$(VMMCTL_32_OBJS): i386/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_32) $(KFLAGS_32) -c $< -o $@

$(MODULE_64): $(VMMCTL_64_OBJS)
	@echo "Linking          $@"
	$(LD) $(LDFLAGS_64) -r $(VMMCTL_64_OBJS) -o $@

$(VMMCTL_64_OBJS): amd64/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_64) $(KFLAGS_64) -c $< -o $@

clean:
	@echo "Cleaning directories"
	@rm -rf $(MODULE_32) $(VMMCTL_32_OBJS) i386
	@rm -rf $(MODULE_64) $(VMMCTL_64_OBJS) amd64

install32:
	cp $(MODULE_32) /kernel/drv/
	chmod 755 /kernel/drv/$(MODULE)

install64:
	chmod 755 /kernel/drv/$(MODULE)
	cp $(MODULE_64) /kernel/drv/amd64

install: $(INSTALL)
