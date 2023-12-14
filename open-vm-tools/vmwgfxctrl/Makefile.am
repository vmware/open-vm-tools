################################################################################
### Copyright (C) 2021 VMware, Inc.  All rights reserved.
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

AUTOMAKE_OPTIONS = subdir-objects

if ENABLE_VMWGFXCTRL

bin_PROGRAMS = vmwgfxctrl

vmwgfxctrl_LDADD =
vmwgfxctrl_LDADD += @VMTOOLS_LIBS@
vmwgfxctrl_LDADD += @LIBUDEV_LIBS@

vmwgfxctrl_CFLAGS = -std=gnu89

vmwgfxctrl_CPPFLAGS =
vmwgfxctrl_CPPFLAGS += @VMTOOLS_CPPFLAGS@
vmwgfxctrl_CPPFLAGS += @LIBUDEV_CFLAGS@

vmwgfxctrl_SOURCES =
vmwgfxctrl_SOURCES += main.c
vmwgfxctrl_LINK = $(LINK)

endif
