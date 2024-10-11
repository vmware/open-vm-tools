################################################################################
### Copyright (c) 2020, 2023 VMware, Inc.  All rights reserved.
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

lib_LTLIBRARIES = libguestStoreClient.la

libguestStoreClient_la_LIBADD =
libguestStoreClient_la_LIBADD += ../lib/err/libErr.la
libguestStoreClient_la_LIBADD += ../lib/string/libString.la
libguestStoreClient_la_LIBADD += ../lib/unicode/libUnicode.la
libguestStoreClient_la_LIBADD += ../lib/misc/libMisc.la
libguestStoreClient_la_LIBADD += -lpthread
libguestStoreClient_la_LIBADD += -ldl
libguestStoreClient_la_LIBADD += @ICU_LIBS@

libguestStoreClient_la_SOURCES =
libguestStoreClient_la_SOURCES += guestStoreClientLib.c

libguestStoreClient_la_LDFLAGS =
# We require GCC, so we're fine passing compiler-specific flags.
libguestStoreClient_la_LDFLAGS += -Wl,-z,defs
# Needed for OS's that don't link shared libraries against libc by
#default, e.g. FreeBSD
libguestStoreClient_la_LIBADD += -lc
