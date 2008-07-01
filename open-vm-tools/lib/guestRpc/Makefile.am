################################################################################
### Copyright 2008 VMware, Inc.  All rights reserved.
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

noinst_LIBRARIES = libGuestRpc.a

libGuestRpc_a_SOURCES =
libGuestRpc_a_SOURCES += nicinfo_xdr.c

nodist_libGuestRpc_a_SOURCES =
nodist_libGuestRpc_a_SOURCES += nicinfo.h
nodist_libGuestRpc_a_SOURCES += nicinfo_xdr.c

# XXX: Autoreconf complains about this and recommends using AM_CFLAGS instead.
# Neither that nor libGuestRpc_a_CFLAGS work, though. What gives?
CFLAGS += -Wno-unused

CLEANFILES =
CLEANFILES += nicinfo.h
CLEANFILES += nicinfo_xdr.c

EXTRA_DIST =
EXTRA_DIST += nicinfo.x

nicinfo.h: nicinfo.x
	$(RPCGEN) -h -o $@ nicinfo.x
	sed 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	mkdir -p $(top_builddir)/include/guestrpc
	cp $@ $(top_builddir)/include/guestrpc

nicinfo_xdr.c: nicinfo.x nicinfo.h
	$(RPCGEN) -c -o $@ nicinfo.x

