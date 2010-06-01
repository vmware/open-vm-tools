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

PROG      := vmware-memctld
PROG_32   := i386/$(PROG)
PROG_64   := amd64/$(PROG)

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

VMMCTL_32_OBJS := $(addprefix i386/, $(VMMCTL_OBJS))
VMMCTL_32_OBJS += i386/backdoorGcc32.o

VMMCTL_64_OBJS := $(addprefix amd64/, $(VMMCTL_OBJS))
VMMCTL_64_OBJS += amd64/backdoorGcc64.o

##
## Objects needed to build the vmware-memctld program
##
VMMCTLD_OBJS := vmmemctld.o

VMMCTLD_32_OBJS := $(addprefix i386/, $(VMMCTLD_OBJS))
VMMCTLD_64_OBJS := $(addprefix amd64/, $(VMMCTLD_OBJS))


ifeq ($(shell echo "$(VM_UNAME)" | cut -c-4),5.9) # {

# Solaris 9
MODULES := $(MODULE_32)
PROGS   := $(PROG_32)
INSTALL := install32
CFLAGS  += -D__STDC__=0 -DSOL9

else # } {
ifeq ($(shell echo "$(VM_UNAME)" | cut -c-4),5.10) # {

# Solaris 10
MODULES := $(MODULE_32) $(MODULE_64)
PROGS   := $(PROG_32) $(PROG_64)
INSTALL := install32 install64
CFLAGS  += -DSOL10
KFLAGS  += -ffreestanding
KFLAGS  += -nodefaultlibs

else # } {

$(error "This makefile is only used for Solaris 9 and 10")

endif # }
endif # }

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

all: prepare_dirs $(MODULES) $(PROGS)

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

$(PROG_32): $(VMMCTLD_32_OBJS)
	@echo "Linking          $@"
	$(CC) -m32 $(VMMCTLD_32_OBJS) -o $@

$(VMMCTLD_32_OBJS): i386/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_32) -c $< -o $@

$(PROG_64): $(VMMCTLD_64_OBJS)
	@echo "Linking          $@"
	$(CC) -m64 $(VMMCTLD_64_OBJS) $< -o $@

$(VMMCTLD_64_OBJS): amd64/%.o: %.c
	@echo "Compiling        $(<F)"
	$(CC) $(CFLAGS_64) -c $< -o $@

clean:
	@echo "Cleaning directories"
	@rm -rf $(MODULE_32) $(PROG_32) $(VMMCTL_32_OBJS) $(VMMCTLD_32_OBJS) i386
	@rm -rf $(MODULE_64) $(PROG_64) $(VMMCTL_64_OBJS) $(VMMCTLD_64_OBJS) amd64

install32:
	cp $(MODULE_32) /kernel/drv/
	chmod 755 /kernel/drv/$(MODULE)
	mkdir -p /usr/sbin/i86
	cp $(PROG_32) /usr/sbin/i86/$(PROG)
	chmod 755 /usr/sbin/i86/$(PROG)
	ln -f /usr/lib/isaexec /usr/sbin/$(PROG)

install64:
	chmod 755 /kernel/drv/$(MODULE)
	cp $(MODULE_64) /kernel/drv/amd64
	mkdir -p /usr/sbin/amd64
	cp $(PROG_64) /usr/sbin/amd64/$(PROG)
	chmod 755 /usr/sbin/amd64/$(PROG)

install: $(INSTALL)
	cp vmmemctl.conf /kernel/drv/
	chmod 755 /kernel/drv/vmmemctl.conf
	grep $(MODULE) /etc/name_to_major > /dev/null 2>&1; \
	if [ ! $$? = 0 ]; then \
		/usr/sbin/add_drv $(MODULE); \
	fi
