################################################################################
### Copyright (C) 2016 VMware, Inc.  All rights reserved.
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

confdir=/etc/vmware-caf/pme/config

conf_DATA = \
	cafenv-appconfig \
	CommAmqpListener-appconfig \
	CommAmqpListener-context-amqp.xml \
	CommAmqpListener-context-common.xml \
	CommAmqpListener-context-tunnel.xml \
	CommAmqpListener-log4cpp_config \
	IntBeanConfigFile.xml \
	ma-appconfig \
	ma-context.xml \
	ma-log4cpp_config \
	providerFx-appconfig \
	providerFx-log4cpp_config \
	vgauth.conf

install-data-local:
	D=$(DESTDIR) /bin/bash $(top_srcdir)/common-agent/etc/install/install.sh -L -l $(libdir)/open-vm-tools -B $(libdir)/open-vm-tools -i /var -o /var
