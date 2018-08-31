################################################################################
### Copyright (C) 2009-2018 VMware, Inc.  All rights reserved.
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

noinst_LTLIBRARIES = libRpcChannel.la

libRpcChannel_la_SOURCES =
libRpcChannel_la_SOURCES += bdoorChannel.c
libRpcChannel_la_SOURCES += rpcChannel.c
if HAVE_VSOCK
libRpcChannel_la_SOURCES += vsockChannel.c
libRpcChannel_la_SOURCES += simpleSocket.c
endif

libRpcChannel_la_CPPFLAGS =
libRpcChannel_la_CPPFLAGS += @VMTOOLS_CPPFLAGS@
libRpcChannel_la_CPPFLAGS += @XDR_CPPFLAGS@

libRpcChannel_la_LIBADD =
libRpcChannel_la_LIBADD += @XDR_LIBS@

