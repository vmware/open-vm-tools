defaultscripts  = poweron-vm-default
defaultscripts += poweroff-vm-default
defaultscripts += suspend-vm-default
defaultscripts += resume-vm-default

confdir = /etc/vmware-tools

conf_SCRIPTS = ./common/vm-support
conf_SCRIPTS += ./common/statechange.subr
conf_SCRIPTS += $(defaultscripts)

vmwsrcdir = $(confdir)/scripts/vmware

vmwsrc_SCRIPTS = $(MODULES_OS)/network

$(defaultscripts): $(top_srcdir)/scripts/common/statechange.sh
	cp $(top_srcdir)/scripts/common/statechange.sh $@

