################################################################################
### Copyright (C) 2016 VMware, Inc.  All rights reserved.
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

AUTOMAKE_OPTIONS = subdir-objects
cafbindir = @CAF_PME_BIN_DIR@
cafbin_PROGRAMS =

cafbin_PROGRAMS += ConfigProvider

ConfigProvider_SOURCES=
ConfigProvider_SOURCES += Config_Provider/src/CConfigProvider.cpp
ConfigProvider_SOURCES += Config_Provider/src/CIniFileWithoutSection.cpp
ConfigProvider_SOURCES += Config_Provider/src/ConfigProvider.cpp

ConfigProvider_CPPFLAGS =
ConfigProvider_CPPFLAGS += @GLIB2_CPPFLAGS@
ConfigProvider_CPPFLAGS += @LOG4CPP_CPPFLAGS@
ConfigProvider_CPPFLAGS += @SSL_CPPFLAGS@
ConfigProvider_CPPFLAGS += @LIBRABBITMQ_CPPFLAGS@

ConfigProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
ConfigProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ProviderFx/ProviderFx/include
ConfigProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/InternalProviders/Config_Provider/include
ConfigProvider_LDADD =
ConfigProvider_LDADD += @GLIB2_LIBS@
ConfigProvider_LDADD += @LOG4CPP_LIBS@
ConfigProvider_LDADD += -ldl
ConfigProvider_LDADD += ../Framework/libFramework.la
ConfigProvider_LDADD += ../ProviderFx/libProviderFx.la

cafbin_PROGRAMS += InstallProvider

InstallProvider_SOURCES=
InstallProvider_SOURCES += Install_Provider/src/CInstallProvider.cpp
InstallProvider_SOURCES += Install_Provider/src/CInstallUtils.cpp
InstallProvider_SOURCES += Install_Provider/src/CPackageExecutor.cpp
InstallProvider_SOURCES += Install_Provider/src/CPackageInstaller.cpp
InstallProvider_SOURCES += Install_Provider/src/CPathBuilder.cpp
InstallProvider_SOURCES += Install_Provider/src/CProviderInstaller.cpp
InstallProvider_SOURCES += Install_Provider/src/InstallProvider.cpp

InstallProvider_CPPFLAGS =
InstallProvider_CPPFLAGS += @GLIB2_CPPFLAGS@
InstallProvider_CPPFLAGS += @LOG4CPP_CPPFLAGS@
InstallProvider_CPPFLAGS += @SSL_CPPFLAGS@
InstallProvider_CPPFLAGS += @LIBRABBITMQ_CPPFLAGS@

InstallProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
InstallProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ProviderFx/ProviderFx/include
InstallProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/InternalProviders/Install_Provider/include
InstallProvider_LDADD =
InstallProvider_LDADD += @GLIB2_LIBS@
InstallProvider_LDADD += @LOG4CPP_LIBS@
InstallProvider_LDADD += -ldl
InstallProvider_LDADD += ../Framework/libFramework.la
InstallProvider_LDADD += ../ProviderFx/libProviderFx.la

cafbin_PROGRAMS += RemoteCommandProvider

RemoteCommandProvider_SOURCES=
RemoteCommandProvider_SOURCES += RemoteCommand_Provider/src/CRemoteCommandProvider.cpp
RemoteCommandProvider_SOURCES += RemoteCommand_Provider/src/RemoteCommandProvider.cpp

RemoteCommandProvider_CPPFLAGS =
RemoteCommandProvider_CPPFLAGS += @GLIB2_CPPFLAGS@
RemoteCommandProvider_CPPFLAGS += @LOG4CPP_CPPFLAGS@
RemoteCommandProvider_CPPFLAGS += @SSL_CPPFLAGS@
RemoteCommandProvider_CPPFLAGS += @LIBRABBITMQ_CPPFLAGS@

RemoteCommandProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
RemoteCommandProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ProviderFx/ProviderFx/include
RemoteCommandProvider_LDADD =
RemoteCommandProvider_LDADD += @GLIB2_LIBS@
RemoteCommandProvider_LDADD += @LOG4CPP_LIBS@
RemoteCommandProvider_LDADD += -ldl
RemoteCommandProvider_LDADD += ../Framework/libFramework.la
RemoteCommandProvider_LDADD += ../ProviderFx/libProviderFx.la

cafbin_PROGRAMS += TestInfraProvider

TestInfraProvider_SOURCES=
TestInfraProvider_SOURCES += TestInfra_Provider/src/CTestInfraProvider.cpp
TestInfraProvider_SOURCES += TestInfra_Provider/src/TestInfraProvider.cpp

TestInfraProvider_CPPFLAGS =
TestInfraProvider_CPPFLAGS += @GLIB2_CPPFLAGS@
TestInfraProvider_CPPFLAGS += @LOG4CPP_CPPFLAGS@
TestInfraProvider_CPPFLAGS += @SSL_CPPFLAGS@
TestInfraProvider_CPPFLAGS += @LIBRABBITMQ_CPPFLAGS@

TestInfraProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
TestInfraProvider_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ProviderFx/ProviderFx/include
TestInfraProvider_LDADD =
TestInfraProvider_LDADD += @GLIB2_LIBS@
TestInfraProvider_LDADD += @LOG4CPP_LIBS@
TestInfraProvider_LDADD += -ldl
TestInfraProvider_LDADD += ../Framework/libFramework.la
TestInfraProvider_LDADD += ../ProviderFx/libProviderFx.la

