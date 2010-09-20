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
libGuestRpc_la_SOURCES += nicinfo_xdr.c

# XXX: Autoreconf complains about this and recommends using AM_CFLAGS instead.
# Problem is, $(CFLAGS) is appended to the compiler command line after AM_CFLAGS
# and after libGuestRpc_la_CFLAGS, so "-Wall -Werror" will override this flag.
CFLAGS += -Wno-unused

CLEANFILES =
CLEANFILES += nicinfo.h
CLEANFILES += nicinfo_xdr.c

EXTRA_DIST =
EXTRA_DIST += nicinfo.x


# Rules to invoke rpcgen. rpcgen will generate funny paths in the generated
# files if not invoked in the same directory as the source file, so we need
# to copy the sources to the build dir before compiling them.

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

