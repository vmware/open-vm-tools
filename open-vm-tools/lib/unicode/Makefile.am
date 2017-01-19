################################################################################
### Copyright (C) 2007-2016 VMware, Inc.  All rights reserved.
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

noinst_LTLIBRARIES = libUnicode.la

libUnicode_la_SOURCES =
libUnicode_la_SOURCES += unicodeCommon.c
libUnicode_la_SOURCES += unicodeSimpleBase.c
libUnicode_la_SOURCES += unicodeSimpleCaseFolding.c
libUnicode_la_SOURCES += unicodeSimpleTypes.c
libUnicode_la_SOURCES += unicodeSimpleOperations.c
libUnicode_la_SOURCES += unicodeSimpleTransforms.c
libUnicode_la_SOURCES += unicodeStatic.c

if HAVE_ICU
libUnicode_la_SOURCES += unicodeICU.c
endif
