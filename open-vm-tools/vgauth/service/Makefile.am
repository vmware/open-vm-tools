################################################################################
### Copyright (C) 2014-2019 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = VGAuthService

VGAuthService_SOURCES =
VGAuthService_SOURCES += main.c
VGAuthService_SOURCES += fileLogger.c
VGAuthService_SOURCES += gio.c
VGAuthService_SOURCES += log.c
VGAuthService_SOURCES += servicePosix.c
VGAuthService_SOURCES += signalSource.c
VGAuthService_SOURCES += ../serviceImpl/alias.c
VGAuthService_SOURCES += ../serviceImpl/file.c
VGAuthService_SOURCES += ../serviceImpl/filePosix.c
VGAuthService_SOURCES += ../serviceImpl/netPosix.c
VGAuthService_SOURCES += ../serviceImpl/proto.c
VGAuthService_SOURCES += ../serviceImpl/random.c
if USE_XMLSEC1
VGAuthService_SOURCES += ../serviceImpl/saml-xmlsec1.c
else
VGAuthService_SOURCES += ../serviceImpl/saml-xml-security-c.cpp
endif
VGAuthService_SOURCES += ../serviceImpl/service.c
VGAuthService_SOURCES += ../serviceImpl/ticket.c
VGAuthService_SOURCES += ../serviceImpl/verify.c

VGAuthService_SOURCES += ../common/audit.c
VGAuthService_SOURCES += ../common/certverify.c
VGAuthService_SOURCES += ../common/i18n.c
VGAuthService_SOURCES += ../common/prefs.c
VGAuthService_SOURCES += ../common/usercheck.c
VGAuthService_SOURCES += ../common/VGAuthLog.c
VGAuthService_SOURCES += ../common/VGAuthUtil.c
VGAuthService_SOURCES += ../common/vmxlog.c

VGAuthServicedir = /etc/vmware-tools/vgauth/schemas
VGAuthService_SCRIPTS =
VGAuthService_SCRIPTS += ../serviceImpl/schemas/datatypes.dtd
VGAuthService_SCRIPTS += ../serviceImpl/schemas/saml-schema-assertion-2.0.xsd
VGAuthService_SCRIPTS += ../serviceImpl/schemas/xenc-schema.xsd
VGAuthService_SCRIPTS += ../serviceImpl/schemas/xmldsig-core-schema.xsd
VGAuthService_SCRIPTS += ../serviceImpl/schemas/XMLSchema.dtd
VGAuthService_SCRIPTS += ../serviceImpl/schemas/XMLSchema-hasFacetAndProperty.xsd
VGAuthService_SCRIPTS += ../serviceImpl/schemas/XMLSchema-instance.xsd
VGAuthService_SCRIPTS += ../serviceImpl/schemas/XMLSchema.xsd
VGAuthService_SCRIPTS += ../serviceImpl/schemas/xml.xsd
VGAuthService_SCRIPTS += ../serviceImpl/schemas/catalog.xml

VGAuthService_CPPFLAGS =
VGAuthService_CPPFLAGS += @GLIB2_CPPFLAGS@
if USE_XMLSEC1
VGAuthService_CPPFLAGS += @XMLSEC1_CPPFLAGS@
else
VGAuthService_CPPFLAGS += @XERCES_CPPFLAGS@
VGAuthService_CPPFLAGS += @XMLSECURITY_CPPFLAGS@
endif
VGAuthService_CPPFLAGS += @SSL_CPPFLAGS@
VGAuthService_CPPFLAGS += -I$(top_srcdir)/vgauth/public
VGAuthService_CPPFLAGS += -I$(top_srcdir)/vgauth/common
VGAuthService_CPPFLAGS += -I$(top_srcdir)/vgauth/serviceImpl

VGAuthService_LDADD =
VGAuthService_LDADD += @GLIB2_LIBS@
VGAuthService_LDADD += @GTHREAD_LIBS@
if USE_XMLSEC1
VGAuthService_LDADD += @XMLSEC1_LIBS@
else
VGAuthService_LDADD += @XERCES_LIBS@
VGAuthService_LDADD += @XMLSECURITY_LIBS@
VGAuthService_LDADD += -lxerces-c
VGAuthService_LDADD += -lxml-security-c
endif
VGAuthService_LDADD += @SSL_LIBS@
VGAuthService_LDADD += -lssl
VGAuthService_LDADD += -lcrypto

if HAVE_ICU
   VGAuthService_LDADD += @ICU_LIBS@
endif

if VGAUTH_USE_CXX
  VGAuthService_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS)     \
                            $(LIBTOOLFLAGS) --mode=link $(CXX)       \
                            $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                            $(LDFLAGS) -o $@
else
   VGAuthService_LINK = $(LINK)
endif

# Message catalogs.
install-data-hook:
	@INSTVMSG@ VGAuthService $(srcdir)/l10n $(DESTDIR)$(datadir)
	cat vgauth.conf | sed -e"s!@@VGAUTHSCHEMADIR@@!$(VGAuthServicedir)!" > $(DESTDIR)/etc/vmware-tools/vgauth.conf
