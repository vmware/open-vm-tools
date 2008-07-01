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

noinst_LTLIBRARIES = libMisc.la

libMisc_la_SOURCES =
libMisc_la_SOURCES += ../atomic.c
libMisc_la_SOURCES += ../base64.c
libMisc_la_SOURCES += ../codesetOld.c
libMisc_la_SOURCES += ../dbllnklst.c
libMisc_la_SOURCES += ../dynarray.c
libMisc_la_SOURCES += ../dynbuf.c
libMisc_la_SOURCES += ../escape.c
libMisc_la_SOURCES += ../hashTable.c
libMisc_la_SOURCES += ../hostinfo_misc.c
libMisc_la_SOURCES += ../hostname.c
libMisc_la_SOURCES += ../idLinux.c
libMisc_la_SOURCES += ../iovector.c
libMisc_la_SOURCES += ../machineID.c
libMisc_la_SOURCES += ../miscSolaris.c
libMisc_la_SOURCES += ../posixDlopen.c
libMisc_la_SOURCES += ../posixPosix.c
libMisc_la_SOURCES += ../random.c
libMisc_la_SOURCES += ../timeutil.c
libMisc_la_SOURCES += ../util_misc.c
libMisc_la_SOURCES += ../vmstdio.c

if NO_ICU
libMisc_la_SOURCES += ../codeset.c
endif

AM_CFLAGS = @LIB_MISC_CPPFLAGS@
