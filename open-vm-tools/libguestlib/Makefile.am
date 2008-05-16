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

lib_LTLIBRARIES = libguestlib.la

libguestlib_la_LIBADD =
libguestlib_la_LIBADD += ../lib/rpcOut/shared/libRpcOut.la
libguestlib_la_LIBADD += ../lib/string/shared/libString.la
# In the absence of the linker options --start-group and --end-group (which can't
# be put in LDADD), we need to bring the following libraries out of the alphebetical
# order so their symbols are properly resolved.
libguestlib_la_LIBADD += ../lib/message/shared/libMessage.la
libguestlib_la_LIBADD += ../lib/backdoor/shared/libBackdoor.la

libguestlib_la_SOURCES =
libguestlib_la_SOURCES += vmGuestLib.c
libguestlib_la_SOURCES += vmGuestLibPanic.c
libguestlib_la_SOURCES += stubs.c

libguestlib_la_LDFLAGS =
# We require GCC, so we're fine passing compiler-specific flags.
# Needed for OS's that don't link shared libraries against libc by default, e.g. FreeBSD
libguestlib_la_LDFLAGS += -Wl,-lc
