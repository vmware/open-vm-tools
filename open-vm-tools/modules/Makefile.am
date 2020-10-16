################################################################################
### Copyright (C) 2008-2018,2020 VMware, Inc.  All rights reserved.
###
### This program is free software; you can redistribute it and/or modify
### it under the terms of version 2 of the GNU General Public License as
### published by the Free Software Foundation.
###
### This program is distributed in the hope that it will be useful,
### but WITHOUT ANY WARRANTY; without even the implied warranty of
### MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
### GNU General Public License for more details.
###
### You should have received a copy of the GNU General Public License
### along with this program; if not, write to the Free Software
### Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
################################################################################

all: modules

.PHONY: modules $(MODULES)
modules: $(MODULES)

modulesrc = $(abs_top_srcdir)/modules

$(MODULES):
	$(MAKE) VM_UNAME=$(KERNEL_RELEASE) MV=mv RM=rm      \
	   OVT_SOURCE_DIR=$(abs_top_srcdir)                 \
	   MODULEBUILDDIR=$(modulesrc)/$(MODULES_OS)        \
	   $(EXTRA_ARGS) -C "$(modulesrc)/$(MODULES_OS)/$@"

if FREEBSD_CUSTOM_SYSDIR
EXTRA_ARGS =
EXTRA_ARGS += "SYSDIR=@SYSDIR@"
endif

if SOLARIS
export vmhgfsdir := $(MODULES_DIR)
# Solaris does not have Kbuild-like system so we need to supply
# compiler and linker and other items discovered by configure
# script.
EXTRA_ARGS =
EXTRA_ARGS += "CC=$(CC)"
EXTRA_ARGS += "CC_WARNINGS=$(CC_WARNINGS)"
EXTRA_ARGS += "GLOBAL_DEFS=$(GLOBAL_DEFS)"
EXTRA_ARGS += "LD=$(LD)"
EXTRA_ARGS += "HAVE_GNU_LD=$(HAVE_GNU_LD)"
endif

## Automake will supplement its own "clean" targets with this.
clean-local:
	for MOD in $(MODULES); do                                              \
	   if [ -d "$(modulesrc)/$(MODULES_OS)/$$MOD" ]; then                  \
	      $(MAKE) VM_UNAME=$(KERNEL_RELEASE) MV=mv RM=rm                   \
	             -C "$(modulesrc)/$(MODULES_OS)/$$MOD" clean || exit 1;    \
	   fi                                                                  \
	done
	rm -f $(modulesrc)/$(MODULES_OS)/*.o $(modulesrc)/$(MODULES_OS)/*.ko
	rm -f $(modulesrc)/$(MODULES_OS)/VMwareVMCIModule.symvers

install-exec-hook:
if SOLARIS
	for MOD in $(MODULES); do                                              \
	   $(MAKE) VM_UNAME=$(KERNEL_RELEASE) MV=mv RM=rm                      \
	      -C "$(modulesrc)/$(MODULES_OS)/$$MOD" install || exit 1;         \
	done
endif
if FREEBSD
	for MOD in $(MODULES); do                                              \
	   $(INSTALL) -d $(DESTDIR)$(MODULES_DIR);                             \
	   $(INSTALL) -m 644 $(modulesrc)/$(MODULES_OS)/$$MOD.ko               \
	      $(DESTDIR)$(MODULES_DIR);                                        \
	done
endif

uninstall-hook:
	for MOD in $(MODULES); do                                              \
	   rm -f $(DESTDIR)`eval echo '$$'$${MOD}dir`/$$MOD.ko &> /dev/null;\
	done
	eval "$$DEPMOD"
