#!/usr/bin/make -f
##########################################################
# Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
####  VMware kernel module Makefile to be distributed externally
####

####
#### SRCROOT _must_ be a relative path.
####
SRCROOT = .

#
# open-vm-tools doesn't replicate shared source files for different modules;
# instead, files are kept in shared locations. So define a few useful macros
# to be able to handle both cases cleanly.
#
INCLUDE      :=
ifdef OVT_SOURCE_DIR
AUTOCONF_DIR := $(OVT_SOURCE_DIR)/modules/linux/shared/autoconf
VMLIB_PATH   = $(OVT_SOURCE_DIR)/lib/$(1)
INCLUDE      += -I$(OVT_SOURCE_DIR)/modules/linux/shared
INCLUDE      += -I$(OVT_SOURCE_DIR)/lib/include
else
AUTOCONF_DIR := $(SRCROOT)/shared/autoconf
INCLUDE      += -I$(SRCROOT)/shared
endif


VM_UNAME = $(shell uname -r)

# Header directory for the running kernel
ifdef LINUXINCLUDE
HEADER_DIR = $(LINUXINCLUDE)
else
HEADER_DIR = /lib/modules/$(VM_UNAME)/build/include
endif

BUILD_DIR = $(HEADER_DIR)/..

DRIVER := vmhgfs
PRODUCT := tools

# Grep program
GREP = /bin/grep

vm_check_gcc = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null \
        > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi)
vm_check_file = $(shell if test -f $(1); then echo "yes"; else echo "no"; fi)

ifndef VM_KBUILD
VM_KBUILD := no
ifeq ($(call vm_check_file,$(BUILD_DIR)/Makefile), yes)
VM_KBUILD := yes
endif
export VM_KBUILD
endif

ifndef VM_KBUILD_SHOWN
ifeq ($(VM_KBUILD), no)
VM_DUMMY := $(shell echo >&2 "Using standalone build system.")
else
VM_DUMMY := $(shell echo >&2 "Using kernel build system.")
endif
VM_KBUILD_SHOWN := yes
export VM_KBUILD_SHOWN
endif

ifneq ($(VM_KBUILD), no)

VMCCVER := $(shell $(CC) -dumpversion)

# If there is no version defined, we are in toplevel pass, not yet in kernel makefiles...
ifeq ($(VERSION),)

DRIVER_KO := $(DRIVER).ko

.PHONY: $(DRIVER_KO)

auto-build: $(DRIVER_KO)
	cp -f $< $(SRCROOT)/../$(DRIVER).o

# $(DRIVER_KO) is a phony target, so compare file times explicitly
$(DRIVER): $(DRIVER_KO)
	if [ $< -nt $@ ] || [ ! -e $@ ] ; then cp -f $< $@; fi

# Pass gcc version down the chain, so we can detect if kernel attempts to use unapproved compiler
VM_CCVER := $(VMCCVER)
export VM_CCVER
VM_CC := $(CC)
export VM_CC

MAKEOVERRIDES := $(filter-out CC=%,$(MAKEOVERRIDES))

#
# Define a setup target that gets built before the actual driver.
# This target may not be used at all, but if it is then it will be defined
# in Makefile.kernel
#
prebuild:: ;
postbuild:: ;

$(DRIVER_KO): prebuild
	$(MAKE) -C $(BUILD_DIR) SUBDIRS=$$PWD SRCROOT=$$PWD/$(SRCROOT) \
	  MODULEBUILDDIR=$(MODULEBUILDDIR) modules
	$(MAKE) -C $$PWD SRCROOT=$$PWD/$(SRCROOT) \
	  MODULEBUILDDIR=$(MODULEBUILDDIR) postbuild
endif

vm_check_build = $(shell if $(CC) $(KBUILD_CPPFLAGS) $(KBUILD_CFLAGS) \
	$(CPPFLAGS) $(CFLAGS) $(CFLAGS_KERNEL) $(LINUXINCLUDE) \
	$(EXTRA_CFLAGS) -Iinclude2/asm/mach-default \
	-DKBUILD_BASENAME=\"$(DRIVER)\" \
	-Werror -S -o /dev/null -xc $(1) \
	> /dev/null 2>&1; then echo "$(2)"; else echo "$(3)"; fi)

CC_WARNINGS := -Wall -Wstrict-prototypes
CC_OPTS := $(GLOBAL_DEFS) $(CC_WARNINGS) -DVMW_USING_KBUILD
ifdef VMX86_DEVEL
CC_OPTS += -DVMX86_DEVEL
endif
ifdef VMX86_DEBUG
CC_OPTS += -DVMX86_DEBUG
endif

include $(SRCROOT)/Makefile.kernel

else

include $(SRCROOT)/Makefile.normal

endif

#.SILENT:
