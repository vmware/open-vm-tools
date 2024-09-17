################################################################################
### Copyright (C) 2019 VMware, Inc.  All rights reserved.
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

defaultscripts  = poweron-vm-default
defaultscripts += poweroff-vm-default
defaultscripts += suspend-vm-default
defaultscripts += resume-vm-default

confdir = ${prefix}/share/vmware-tools

conf_SCRIPTS = ./common/statechange.subr
conf_SCRIPTS += $(defaultscripts)

vmsupportdir = /usr/bin

vmsupport_SCRIPTS = ./common/vm-support

vmwsrcdir = $(confdir)/scripts/vmware

vmwsrc_SCRIPTS = $(MODULES_OS)/network

$(defaultscripts): $(top_srcdir)/scripts/common/statechange.sh
	cp $(top_srcdir)/scripts/common/statechange.sh $@

