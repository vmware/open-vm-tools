all: modules

.PHONY: modules $(MODULES)
modules: $(MODULES)

modulesrc = $(top_srcdir)/modules

$(MODULES):
	$(MAKE) VM_UNAME=$(KERNEL_RELEASE) -C "$(modulesrc)/$(MODULES_OS)/$@"

if LINUX
export pvscsidir := $(MODULES_DIR)/drivers/scsi
export vmblockdir := $(MODULES_DIR)/fs/vmblock
export vmcidir := $(MODULES_DIR)/drivers/misc
export vmhgfsdir := $(MODULES_DIR)/fs/vmhgfs
export vmmemctldir := $(MODULES_DIR)/drivers/misc
export vmsyncdir := $(MODULES_DIR)/drivers/misc
export vmxnetdir := $(MODULES_DIR)/drivers/net
export vmxnet3dir := $(MODULES_DIR)/drivers/net
export vsockdir := $(MODULES_DIR)/net/vsock
if WITH_ROOT_PRIVILEGES
export DEPMOD := depmod -a
endif WITH_ROOT_PRIVILEGES
endif LINUX

if FREEBSD
export vmblockdir := $(MODULES_DIR)
export vmhgfsdir := $(MODULES_DIR)
export vmmemctldir := $(MODULES_DIR)
export vmsyncdir := $(MODULES_DIR)
export vmxnetdir := $(MODULES_DIR)
endif FREEBSD

## Automake will supplement its own "clean" targets with this.
clean-local:
	for MOD in $(MODULES); do 															\
	   if [ -d "$(modulesrc)/$(MODULES_OS)/$$MOD" ]; then						\
	      $(MAKE) -C "$(modulesrc)/$(MODULES_OS)/$$MOD" clean || exit 1; \
	   fi																						\
	done
	$(RM) -f $(modulesrc)/$(MODULES_OS)/*.o $(modulesrc)/$(MODULES_OS)/*.ko

install-exec-hook:
	for MOD in $(MODULES); do 													\
	   $(INSTALL) -d $(DESTDIR)`eval echo '$$'$${MOD}dir`; 			\
	   $(INSTALL) -m644 $(modulesrc)/$(MODULES_OS)/$$MOD/$$MOD.ko	\
	      $(DESTDIR)`eval echo '$$'$${MOD}dir`; 							\
	done
	eval "$$DEPMOD"
uninstall-hook:
	for MOD in $(MODULES); do \
	   $(RM) -f $(DESTDIR)`eval echo '$$'$${MOD}dir`/$$MOD.ko &> /dev/null; \
	done
	eval "$$DEPMOD"
