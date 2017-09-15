################################################################################
### Copyright (C) 2008-2016 VMware, Inc.  All rights reserved.
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

noinst_LTLIBRARIES = libStubs.la

libStubs_la_SOURCES =
libStubs_la_SOURCES += stub-config.c
libStubs_la_SOURCES += stub-log.c
libStubs_la_SOURCES += stub-debug.c
libStubs_la_SOURCES += stub-warning.c
libStubs_la_SOURCES += stub-panic.c
libStubs_la_SOURCES += stub-user-msg.c
libStubs_la_SOURCES += stub-user-panic.c
libStubs_la_SOURCES += stub-user-util.c

noinst_LTLIBRARIES += libStubsCS.la

libStubsCS_la_SOURCES =
libStubsCS_la_SOURCES += stub-config.c
libStubsCS_la_SOURCES += stub-user-msg.c
libStubsCS_la_SOURCES += stub-user-panic.c

if !LINUX
   libStubsCS_la_SOURCES += stub-msgfmt-fbsd.c
endif

