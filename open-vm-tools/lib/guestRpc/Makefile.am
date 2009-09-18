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

noinst_LTLIBRARIES = libGuestRpc.la

libGuestRpc_la_SOURCES =
libGuestRpc_la_SOURCES += cpFileContents_xdr.c
libGuestRpc_la_SOURCES += ghiGetBinaryHandlers_xdr.c
libGuestRpc_la_SOURCES += ghiGetExecInfoHash_xdr.c
libGuestRpc_la_SOURCES += ghiProtocolHandler_xdr.c
libGuestRpc_la_SOURCES += ghiStartMenu_xdr.c
libGuestRpc_la_SOURCES += guestlibV3_xdr.c
libGuestRpc_la_SOURCES += nicinfo_xdr.c
libGuestRpc_la_SOURCES += unityActive_xdr.c

# XXX: Autoreconf complains about this and recommends using AM_CFLAGS instead.
# Problem is, $(CFLAGS) is appended to the compiler command line after AM_CFLAGS
# and after libGuestRpc_la_CFLAGS, so "-Wall -Werror" will override this flag.
CFLAGS += -Wno-unused

CLEANFILES =
CLEANFILES += cpFileContents.h
CLEANFILES += cpFileContents_xdr.c
CLEANFILES += ghiGetBinaryHandlers.h
CLEANFILES += ghiGetBinaryHandlers_xdr.c
CLEANFILES += ghiGetExecInfoHash.h
CLEANFILES += ghiGetExecInfoHash_xdr.c
CLEANFILES += ghiProtocolHandler.h
CLEANFILES += ghiProtocolHandler_xdr.c
CLEANFILES += ghiStartMenu.h
CLEANFILES += ghiStartMenu_xdr.c
CLEANFILES += guestlibV3.h
CLEANFILES += guestlibV3_xdr.c
CLEANFILES += nicinfo.h
CLEANFILES += nicinfo_xdr.c
CLEANFILES += unityActive.h
CLEANFILES += unityActive_xdr.c

EXTRA_DIST =
EXTRA_DIST += cpFileContents.x
EXTRA_DIST += ghiGetBinaryHandlers.x
EXTRA_DIST += ghiGetExecInfoHash.x
EXTRA_DIST += ghiProtocolHandler.x
EXTRA_DIST += ghiStartMenu.x
EXTRA_DIST += guestlibV3.x
EXTRA_DIST += nicinfo.x
EXTRA_DIST += unityActive.x

## `make clean` should also remove the copied headers.
clean-local:
	rm -rf $(top_builddir)/lib/include/guestrpc
	if [ $(top_builddir) != $(top_srcdir) ]; then	\
	   rm -f *.x;                                   \
	   rm -f ghiCommonDefines.h;                    \
	fi

# Rules to invoke rpcgen. rpcgen will generate funny paths in the generated
# files if not invoked in the same directory as the source file, so we need
# to copy the sources to the build dir before compiling them.

cpFileContents.h: cpFileContents.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                           \
	   cp -f $(top_srcdir)/lib/guestRpc/cpFileContents.x $(builddir); \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/cpFileContents.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

cpFileContents_xdr.c: cpFileContents.x cpFileContents.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/cpFileContents.x

ghiGetBinaryHandlers.h: ghiGetBinaryHandlers.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                           \
	   cp -f $(top_srcdir)/lib/guestRpc/ghiCommonDefines.h $(builddir);     \
	   cp -f $(top_srcdir)/lib/guestRpc/ghiGetBinaryHandlers.x $(builddir); \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiGetBinaryHandlers.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiGetBinaryHandlers_xdr.c: ghiGetBinaryHandlers.x ghiGetBinaryHandlers.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiGetBinaryHandlers.x

ghiGetExecInfoHash.h: ghiGetExecInfoHash.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                           \
	   cp -f $(top_srcdir)/lib/guestRpc/ghiCommonDefines.h $(builddir);     \
	   cp -f $(top_srcdir)/lib/guestRpc/ghiGetExecInfoHash.x $(builddir);   \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiGetExecInfoHash.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiGetExecInfoHash_xdr.c: ghiGetExecInfoHash.x ghiGetExecInfoHash.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiGetExecInfoHash.x

ghiProtocolHandler.h: ghiProtocolHandler.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                           \
	   cp -f $(top_srcdir)/lib/guestRpc/ghiCommonDefines.h $(builddir);     \
	   cp -f $(top_srcdir)/lib/guestRpc/ghiProtocolHandler.x $(builddir);   \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiProtocolHandler.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiProtocolHandler_xdr.c: ghiProtocolHandler.x ghiProtocolHandler.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiProtocolHandler.x

ghiStartMenu.h: ghiStartMenu.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                           \
	   cp -f $(top_srcdir)/lib/guestRpc/ghiCommonDefines.h $(builddir);     \
	   cp -f $(top_srcdir)/lib/guestRpc/ghiStartMenu.x $(builddir);         \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/ghiStartMenu.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

ghiStartMenu_xdr.c: ghiStartMenu.x ghiStartMenu.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/ghiStartMenu.x

nicinfo.h: nicinfo.x
	if [ $(top_builddir) != $(top_srcdir) ]; then               \
	   cp -f $(top_srcdir)/lib/guestRpc/nicinfo.x $(builddir);  \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/nicinfo.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

nicinfo_xdr.c: nicinfo.x nicinfo.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/nicinfo.x

guestlibV3.h: guestlibV3.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                  \
	   cp -f $(top_srcdir)/lib/guestRpc/guestlibV3.x $(builddir);  \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/guestlibV3.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

guestlibV3_xdr.c: guestlibV3.x guestlibV3.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/guestlibV3.x

unityActive.h: unityActive.x
	if [ $(top_builddir) != $(top_srcdir) ]; then                  \
	   cp -f $(top_srcdir)/lib/guestRpc/unityActive.x $(builddir); \
	fi
	$(RPCGEN) $(RPCGENFLAGS) -h -o $@ $(builddir)/unityActive.x
	$(SED) 's,rpc/rpc\.h,vmxrpc.h,' $@ > $@.tmp
	mv $@.tmp $@
	$(MKDIR_P) $(top_builddir)/lib/include/guestrpc
	cp $@ $(top_builddir)/lib/include/guestrpc

unityActive_xdr.c: unityActive.x unityActive.h
	$(RPCGEN) $(RPCGENFLAGS) -c -o $@ $(builddir)/unityActive.x

