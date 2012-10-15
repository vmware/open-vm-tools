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

if LINUX
export LINUXINCLUDE := @LINUXINCLUDE@
export vmblockdir := $(MODULES_DIR)/fs/vmblock
export vmcidir := $(MODULES_DIR)/drivers/misc
export vmhgfsdir := $(MODULES_DIR)/fs/vmhgfs
export vmsyncdir := $(MODULES_DIR)/drivers/misc
export vmxnetdir := $(MODULES_DIR)/drivers/net
export vsockdir := $(MODULES_DIR)/net/vsock
if WITH_ROOT_PRIVILEGES
export DEPMOD := depmod -a $(KERNEL_RELEASE)
endif WITH_ROOT_PRIVILEGES

include_HEADERS = $(top_srcdir)/lib/include/vmci_sockets.h
endif LINUX

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
	   $(INSTALL) -m644 $(modulesrc)/$(MODULES_OS)/$$MOD.ko                \
	      $(DESTDIR)$(MODULES_DIR);                                        \
	done
endif
if LINUX
	for MOD in $(MODULES); do                                              \
	   $(INSTALL) -d $(DESTDIR)`eval echo '$$'$${MOD}dir`;                 \
	   $(INSTALL) -m644 $(modulesrc)/$(MODULES_OS)/$$MOD/$$MOD.ko          \
	      $(DESTDIR)`eval echo '$$'$${MOD}dir`;                            \
	done
	eval "$$DEPMOD"
endif

uninstall-hook:
	for MOD in $(MODULES); do                                              \
	   rm -f $(DESTDIR)`eval echo '$$'$${MOD}dir`/$$MOD.ko &> /dev/null;\
	done
	eval "$$DEPMOD"
