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

noinst_PROGRAMS = xferlogs

xferlogs_LDADD =
xferlogs_LDADD += ../lib/rpcVmx/libRpcVmx.a
xferlogs_LDADD += ../lib/string/libString.a
# In the absence of the linker options --start-group and --end-group (which can't
# be put in LDADD), we need to bring the following libraries out of the alphebetical
# order so their symbols are properly resolved.
xferlogs_LDADD += ../lib/rpcOut/libRpcOut.a
xferlogs_LDADD += ../lib/message/libMessage.a
xferlogs_LDADD += ../lib/backdoor/libBackdoor.a
xferlogs_LDADD += ../lib/misc/libMisc.a
xferlogs_LDADD += @ICU_LIBS@

xferlogs_SOURCES =
xferlogs_SOURCES += xferlogs.c
