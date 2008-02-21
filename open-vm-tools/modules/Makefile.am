all: modules

.PHONY: modules $(MODULES)
modules: $(MODULES)

$(MODULES):
	$(MAKE) -C "$(MODULES_OS)/$@"

## Automake will supplement its own "distclean" and "clean" targets with these.
distclean-local:
clean-local:
	for MOD in $(MODULES); do \
	   $(MAKE) -C "$(MODULES_OS)/$$MOD" clean || exit 1; \
	done
	rm -f $(MODULES_OS)/*.o $(MODULES_OS)/*.ko
