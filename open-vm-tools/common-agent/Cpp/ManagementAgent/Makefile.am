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
subsysdir = @CAF_SUBSYS_DIR@
subsys_LTLIBRARIES =
cafbindir = @CAF_PME_BIN_DIR@
cafbin_PROGRAMS =
subsys_LTLIBRARIES += libMaIntegrationSubsys.la
libMaIntegrationSubsys_la_LDFLAGS = @CAF_SUBSYS_LDFLAGS@

libMaIntegrationSubsys_la_SOURCES=
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CAttachmentRequestTransformer.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CAttachmentRequestTransformerInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CCollectSchemaExecutor.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CConfigEnv.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CConfigEnvInboundChannelAdapterInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CConfigEnvMerge.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CConfigEnvMessageHandler.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CConfigEnvOutboundChannelAdapterInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CConfigEnvReadingMessageSource.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CDiagToMgmtRequestTransformer.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CDiagToMgmtRequestTransformerInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CInstallToMgmtRequestTransformer.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CInstallToMgmtRequestTransformerInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CMonitorInboundChannelAdapterInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CMonitorReadingMessageSource.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CPersistenceInboundChannelAdapterInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CPersistenceMerge.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CPersistenceMessageHandler.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CPersistenceNamespaceDb.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CPersistenceOutboundChannelAdapterInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CPersistenceReadingMessageSource.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CProviderCollectSchemaExecutor.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CProviderExecutor.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CProviderExecutorRequest.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CProviderExecutorRequestHandler.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CResponseFactory.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CSchemaCacheManager.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CSinglePmeRequestSplitter.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CSinglePmeRequestSplitterInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CVersionTransformer.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/CVersionTransformerInstance.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/IntegrationObjects.cpp
libMaIntegrationSubsys_la_SOURCES += Subsystems/MaIntegration/src/MaIntegration.cpp

libMaIntegrationSubsys_la_CPPFLAGS =
libMaIntegrationSubsys_la_CPPFLAGS += @GLIB2_CPPFLAGS@
libMaIntegrationSubsys_la_CPPFLAGS += @LOG4CPP_CPPFLAGS@

libMaIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
libMaIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Subsystems/CafIntegration/include
libMaIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ManagementAgent/ManagementAgent/include
libMaIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ManagementAgent/Subsystems/MaIntegration/include
libMaIntegrationSubsys_la_LIBADD =
libMaIntegrationSubsys_la_LIBADD += @GLIB2_LIBS@
libMaIntegrationSubsys_la_LIBADD += @LOG4CPP_LIBS@
libMaIntegrationSubsys_la_LIBADD += -ldl
libMaIntegrationSubsys_la_LIBADD += ../Framework/libFramework.la

libMaIntegrationSubsys_la_LDFLAGS += -shared

cafbin_PROGRAMS += ManagementAgentHost

ManagementAgentHost_SOURCES=
ManagementAgentHost_SOURCES += ManagementAgent/src/CManagementAgentHostWork.cpp
ManagementAgentHost_SOURCES += ManagementAgent/src/ManagementAgentHost.cpp
ManagementAgentHost_SOURCES += ManagementAgent/src/ManagementAgentHostMain.cpp

ManagementAgentHost_CPPFLAGS =
ManagementAgentHost_CPPFLAGS += @GLIB2_CPPFLAGS@
ManagementAgentHost_CPPFLAGS += @LOG4CPP_CPPFLAGS@

ManagementAgentHost_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
ManagementAgentHost_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ManagementAgent/ManagementAgent/include
ManagementAgentHost_LDADD =
ManagementAgentHost_LDADD += @GLIB2_LIBS@
ManagementAgentHost_LDADD += @LOG4CPP_LIBS@
ManagementAgentHost_LDADD += -ldl
ManagementAgentHost_LDADD += ../Framework/libFramework.la
ManagementAgentHost_LDADD += $(top_builddir)/vgauth/lib/libvgauth.la

subsys_LTLIBRARIES += libVgAuthIntegrationSubsys.la
libVgAuthIntegrationSubsys_la_LDFLAGS = @CAF_SUBSYS_LDFLAGS@

libVgAuthIntegrationSubsys_la_SOURCES=
libVgAuthIntegrationSubsys_la_SOURCES += Subsystems/VgAuth/src/CGuestAuthenticator.cpp
libVgAuthIntegrationSubsys_la_SOURCES += Subsystems/VgAuth/src/CGuestAuthenticatorInstance.cpp
libVgAuthIntegrationSubsys_la_SOURCES += Subsystems/VgAuth/src/CVgAuthContext.cpp
libVgAuthIntegrationSubsys_la_SOURCES += Subsystems/VgAuth/src/CVgAuthError.cpp
libVgAuthIntegrationSubsys_la_SOURCES += Subsystems/VgAuth/src/CVgAuthImpersonation.cpp
libVgAuthIntegrationSubsys_la_SOURCES += Subsystems/VgAuth/src/CVgAuthInitializer.cpp
libVgAuthIntegrationSubsys_la_SOURCES += Subsystems/VgAuth/src/CVgAuthUserHandle.cpp
libVgAuthIntegrationSubsys_la_SOURCES += Subsystems/VgAuth/src/VgAuthIntegration.cpp

libVgAuthIntegrationSubsys_la_CPPFLAGS =
libVgAuthIntegrationSubsys_la_CPPFLAGS += @GLIB2_CPPFLAGS@
libVgAuthIntegrationSubsys_la_CPPFLAGS += @LOG4CPP_CPPFLAGS@
libVgAuthIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/vgauth/public

libVgAuthIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
libVgAuthIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ManagementAgent/ManagementAgent/include
libVgAuthIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/ManagementAgent/Subsystems/VgAuth/include
libVgAuthIntegrationSubsys_la_LIBADD =
libVgAuthIntegrationSubsys_la_LIBADD += @GLIB2_LIBS@
libVgAuthIntegrationSubsys_la_LIBADD += @LOG4CPP_LIBS@
libVgAuthIntegrationSubsys_la_LIBADD += -ldl
libVgAuthIntegrationSubsys_la_LIBADD += ../Framework/libFramework.la
libVgAuthIntegrationSubsys_la_LIBADD += $(top_builddir)/vgauth/lib/libvgauth.la

libVgAuthIntegrationSubsys_la_LDFLAGS += -shared

