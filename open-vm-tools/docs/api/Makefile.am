################################################################################
### Copyright (C) 2009-2016 VMware, Inc.  All rights reserved.
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

DOXYGEN_INPUT =
DOXYGEN_INPUT += $(top_srcdir)/docs/api
DOXYGEN_INPUT += $(top_srcdir)/docs/api/services
DOXYGEN_INPUT += $(top_srcdir)/lib/include/vmware/tools
DOXYGEN_INPUT += $(top_srcdir)/libvmtools
DOXYGEN_INPUT += $(top_srcdir)/tests/vmrpcdbg

DOXYGEN_CPP_PREDEFINED =
DOXYGEN_CPP_PREDEFINED += G_LOCK_DEFINE_STATIC(x)
DOXYGEN_CPP_PREDEFINED += G_LOG_DOMAIN=

DOXYGEN_CPP_EXPAND =

DOXYGEN_EXTRA_PATTERNS =
DOXYGEN_EXTRA_PATTERNS += *.c
DOXYGEN_EXTRA_PATTERNS += *.doxy
DOXYGEN_EXTRA_PATTERNS += *.h
DOXYGEN_EXTRA_PATTERNS += *.txt

# This rule has no dependencies, so it's executed every time. Not optimal,
# but generation of the docs is pretty quick, and having the right dependencies
# here is very non-trivial.
build/index.html:
	mkdir -p build
	sed \
	   -e 's,##{BUILD_DIR}##,$(top_builddir),'                \
	   -e 's,##{BUILD_OUTPUT}##,build,'                       \
	   -e 's,##{CLIENT_ROOT}##,$(top_srcdir),'                \
	   -e 's,##{CPP_EXPAND}##,$(DOXYGEN_CPP_EXPAND),'         \
	   -e 's,##{CPP_PREDEFINED}##,$(DOXYGEN_CPP_PREDEFINED),' \
	   -e 's,##{DOT}##,@DOT@,'                                \
	   -e 's,##{FILE_PATTERNS}##,$(DOXYGEN_EXTRA_PATTERNS),'  \
	   -e 's,##{HAVE_DOT}##,@HAVE_DOT@,'                      \
	   -e 's,##{INCLUDE_PATHS}##,$(top_srcdir)/lib/include,'  \
	   -e 's,##{INPUT_PATHS}##,$(DOXYGEN_INPUT),'             \
	   -e 's,##{MSCGEN_DIR}##,@MSCGEN_DIR@,'                  \
	   -e 's,##{PERL}##,,'                                    \
	   -e 's,##{PROJECT_NAME}##,@PACKAGE_STRING@,'            \
	   -e 's,##{PROJECT_VERSION}##,@TOOLS_VERSION@,'          \
	   -e 's,##{WARNING_LOG}##,warnings.log,'                 \
	   $(top_srcdir)/docs/api/doxygen.conf > doxygen.parsed.conf
	doxygen doxygen.parsed.conf > /dev/null

all-local: build/index.html

install-data-local:
	mkdir -p $(DESTDIR)$(docdir)/api
	cp -r build/* $(DESTDIR)$(docdir)/api
