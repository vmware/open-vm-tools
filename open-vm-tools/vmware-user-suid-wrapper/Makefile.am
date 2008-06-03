################################################################################
### Copyright 2007 VMware, Inc.  All rights reserved.
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

noinst_PROGRAMS = vmware-user-suid-wrapper

# We shouldn't impose on users' installation preferences, so just provide
# a sane default and let them override it at configure or make time.
VMWARE_USER_PATH = $(bindir)/vmware-user

AM_CPPFLAGS =
AM_CPPFLAGS += -DVMWARE_USER_PATH=\"$(VMWARE_USER_PATH)\"

vmware_user_suid_wrapper_SOURCES =
vmware_user_suid_wrapper_SOURCES += main.c
vmware_user_suid_wrapper_SOURCES += wrapper-@TARGET_OS@.c
