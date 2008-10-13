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
libGuestRpc_a_SOURCES += ghiGetBinaryHandlers_xdr.c
libGuestRpc_a_SOURCES += ghiProtocolHandler_xdr.c
libGuestRpc_a_SOURCES += nicinfo_xdr.c
libGuestRpc_a_SOURCES += unityActive_xdr.c

nodist_libGuestRpc_a_SOURCES =
nodist_libGuestRpc_a_SOURCES += ghiGetBinaryHandlers.h
nodist_libGuestRpc_a_SOURCES += ghiGetBinaryHandlers_xdr.c
nodist_libGuestRpc_a_SOURCES += ghiProtocolHandler.h
nodist_libGuestRpc_a_SOURCES += ghiProtocolHandler_xdr.c
nodist_libGuestRpc_a_SOURCES += nicinfo.h
nodist_libGuestRpc_a_SOURCES += nicinfo_xdr.c
nodist_libGuestRpc_a_SOURCES += unityActive.h
nodist_libGuestRpc_a_SOURCES += unityActive_xdr.c

# XXX: Autoreconf complains about this and recommends using AM_CFLAGS instead.
# Neither that nor libGuestRpc_a_CFLAGS work, though. What gives?
CFLAGS += -Wno-unused

CLEANFILES =
CLEANFILES += ghiGetBinaryHandlers.h
CLEANFILES += ghiGetBinaryHandlers_xdr.c
CLEANFILES += ghiProtocolHandler.h
CLEANFILES += ghiProtocolHandler_xdr.c
CLEANFILES += nicinfo.h
CLEANFILES += nicinfo_xdr.c
CLEANFILES += unityActive.h
CLEANFILES += unityActive_xdr.c

EXTRA_DIST =
EXTRA_DIST += ghiGetBinaryHandlers.x
EXTRA_DIST += ghiProtocolHandler.x
EXTRA_DIST += nicinfo.x
EXTRA_DIST += unityActive.x

## `make clean` should also remove the copied headers.
clean-local:
	$(RM) -rf $(top_builddir)/lib/include/guestrpc

ghiGetBinaryHandlers.h: ghiGetBinaryHandlers.x
	$(RPCGEN) -h -o $@ ghiGetBinaryHandlers.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiGetBinaryHandlers_xdr.c: ghiGetBinaryHandlers.x ghiGetBinaryHandlers.h
	$(RPCGEN) -c -o $@ ghiGetBinaryHandlers.x

ghiProtocolHandler.h: ghiProtocolHandler.x
	$(RPCGEN) -h -o $@ ghiProtocolHandler.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiProtocolHandler_xdr.c: ghiProtocolHandler.x ghiProtocolHandler.h
	$(RPCGEN) -c -o $@ ghiProtocolHandler.x

nicinfo.h: nicinfo.x
	$(RPCGEN) -h -o $@ nicinfo.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

nicinfo_xdr.c: nicinfo.x nicinfo.h
	$(RPCGEN) -c -o $@ nicinfo.x

unityActive.h: unityActive.x
	$(RPCGEN) -h -o $@ unityActive.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

unityActive_xdr.c: unityActive.x unityActive.h
	$(RPCGEN) -c -o $@ unityActive.x

