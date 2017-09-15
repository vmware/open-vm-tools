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
caflibdir = @CAF_PME_LIB_DIR@
caflib_LTLIBRARIES =
caflib_LTLIBRARIES += libCommAmqpIntegration.la

libCommAmqpIntegration_la_SOURCES=
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AMQChannel.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AMQChannelManager.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AMQCommand.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AMQConnection.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AMQUtil.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/Address.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AmqpAuthPlain.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AmqpChannel.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AmqpCommon.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AmqpConnection.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/AmqpUtil.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/BlockingRpcContinuation.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/CAmqpAuthMechanism.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/CAmqpChannel.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/CAmqpConnection.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/CAmqpFrame.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/CertInfo.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/CommandAssembler.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/ConnectionFactoryImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/ConnectionWeakReference.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/ConsumerDispatcher.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/ConsumerWorkService.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpClient.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/AMQExceptions.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/AMQPImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/AmqpClientImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/AmqpContentHeadersImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicAckMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicCancelMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicCancelOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicConsumeMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicConsumeOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicDeliverMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicGetEmptyMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicGetMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicGetOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicProperties.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicPublishMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicQosMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicQosOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicRecoverMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicRecoverOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicRejectMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/BasicReturnMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/ChannelCloseMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/ChannelCloseOkFromServerMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/ChannelCloseOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/ChannelOpenOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/EnvelopeImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/ExchangeDeclareMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/ExchangeDeclareOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/ExchangeDeleteMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/ExchangeDeleteOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/FieldImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/GetResponseImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueueBindMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueueBindOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueueDeclareMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueueDeclareOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueueDeleteMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueueDeleteOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueuePurgeMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueuePurgeOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueueUnbindMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpClient/amqpImpl/QueueUnbindOkMethod.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/AbstractConnectionFactory.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/AmqpHeaderMapper.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/AmqpIntegrationExceptions.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/AmqpMessageListenerSource.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/AmqpOutboundEndpoint.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/AutoChannelClose.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/BindingImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/BlockingQueueConsumer.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/CachedChannelHandler.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/CachingConnectionFactory.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/ChannelCachingConnectionProxy.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/CompositeConnectionListener.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/DefaultAmqpHeaderMapper.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/ExchangeImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/HeaderUtils.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/QueueImpl.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/RabbitAdmin.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/RabbitTemplate.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/SimpleConnection.cpp
libCommAmqpIntegration_la_SOURCES += amqpCore/src/amqpCore/SimpleMessageListenerContainer.cpp

libCommAmqpIntegration_la_CPPFLAGS =
libCommAmqpIntegration_la_CPPFLAGS += @GLIB2_CPPFLAGS@
libCommAmqpIntegration_la_CPPFLAGS += @LOG4CPP_CPPFLAGS@
libCommAmqpIntegration_la_CPPFLAGS += @SSL_CPPFLAGS@
libCommAmqpIntegration_la_CPPFLAGS += @LIBRABBITMQ_CPPFLAGS@

libCommAmqpIntegration_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
libCommAmqpIntegration_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Communication/amqpListener/include
libCommAmqpIntegration_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Communication/amqpCore/include
libCommAmqpIntegration_la_LIBADD =
libCommAmqpIntegration_la_LIBADD += @GLIB2_LIBS@
libCommAmqpIntegration_la_LIBADD += @LOG4CPP_LIBS@
libCommAmqpIntegration_la_LIBADD += @SSL_LIBS@
libCommAmqpIntegration_la_LIBADD += -ldl
libCommAmqpIntegration_la_LIBADD += @LIBRABBITMQ_LIBS@
libCommAmqpIntegration_la_LIBADD += ../Framework/libFramework.la

libCommAmqpIntegration_la_LDFLAGS = -shared

subsys_LTLIBRARIES += libCommAmqpIntegrationSubsys.la
libCommAmqpIntegrationSubsys_la_LDFLAGS = @CAF_SUBSYS_LDFLAGS@

libCommAmqpIntegrationSubsys_la_SOURCES=
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/AmqpInboundChannelAdapterInstance.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/AmqpOutboundEndpointInstance.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/BindingInstance.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/CachingConnectionFactoryObj.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/ExchangeInstance.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/IntegrationObjects.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/QueueInstance.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/RabbitAdminInstance.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/RabbitTemplateInstance.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/SecureCachingConnectionFactoryObj.cpp
libCommAmqpIntegrationSubsys_la_SOURCES += Subsystems/amqpIntegration/src/amqpIntegration.cpp

libCommAmqpIntegrationSubsys_la_CPPFLAGS =
libCommAmqpIntegrationSubsys_la_CPPFLAGS += @GLIB2_CPPFLAGS@
libCommAmqpIntegrationSubsys_la_CPPFLAGS += @LOG4CPP_CPPFLAGS@
libCommAmqpIntegrationSubsys_la_CPPFLAGS += @SSL_CPPFLAGS@
libCommAmqpIntegrationSubsys_la_CPPFLAGS += @LIBRABBITMQ_CPPFLAGS@

libCommAmqpIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
libCommAmqpIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Communication/amqpListener/include
libCommAmqpIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Communication/amqpCore/include
libCommAmqpIntegrationSubsys_la_LIBADD =
libCommAmqpIntegrationSubsys_la_LIBADD += @GLIB2_LIBS@
libCommAmqpIntegrationSubsys_la_LIBADD += @LOG4CPP_LIBS@
libCommAmqpIntegrationSubsys_la_LIBADD += @SSL_LIBS@
libCommAmqpIntegrationSubsys_la_LIBADD += -ldl
libCommAmqpIntegrationSubsys_la_LIBADD += @LIBRABBITMQ_LIBS@
libCommAmqpIntegrationSubsys_la_LIBADD += ../Framework/libFramework.la
libCommAmqpIntegrationSubsys_la_LIBADD += ../Communication/libCommAmqpIntegration.la

libCommAmqpIntegrationSubsys_la_LDFLAGS += -shared

cafbin_PROGRAMS += CommAmqpListener

CommAmqpListener_SOURCES=
CommAmqpListener_SOURCES += amqpListener/src/AmqpListenerWorker.cpp
CommAmqpListener_SOURCES += amqpListener/src/amqpListenerMain.cpp

CommAmqpListener_CPPFLAGS =
CommAmqpListener_CPPFLAGS += @GLIB2_CPPFLAGS@
CommAmqpListener_CPPFLAGS += @LOG4CPP_CPPFLAGS@
CommAmqpListener_CPPFLAGS += @SSL_CPPFLAGS@
CommAmqpListener_CPPFLAGS += @LIBRABBITMQ_CPPFLAGS@

CommAmqpListener_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
CommAmqpListener_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Communication/amqpListener/include
CommAmqpListener_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Communication/amqpCore/include
CommAmqpListener_LDADD =
CommAmqpListener_LDADD += @GLIB2_LIBS@
CommAmqpListener_LDADD += @LOG4CPP_LIBS@
CommAmqpListener_LDADD += @SSL_LIBS@
CommAmqpListener_LDADD += -ldl
CommAmqpListener_LDADD += @LIBRABBITMQ_LIBS@
CommAmqpListener_LDADD += ../Framework/libFramework.la
CommAmqpListener_LDADD += ../Communication/libCommAmqpIntegration.la

subsys_LTLIBRARIES += libCommIntegrationSubsys.la
libCommIntegrationSubsys_la_LDFLAGS = @CAF_SUBSYS_LDFLAGS@

libCommIntegrationSubsys_la_SOURCES=
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CCmsMessage.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CCmsMessageAttachments.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CCmsMessageTransformer.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CCmsMessageTransformerInstance.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CCmsMessageUtils.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CEventTopicCalculatorInstance.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CIncomingMessageHandler.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CIncomingMessageHandlerInstance.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CMessageDeliveryRecord.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CMessagePartDescriptor.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CMessagePartDescriptorCalculator.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CMessagePartDescriptorSourceRecord.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CMessagePartRecord.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CMessagePartsBuilder.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CMessagePartsHeader.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CMessagePartsParser.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/COutgoingMessageHandler.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CProtocolHeaderEnricher.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CProtocolHeaderEnricherInstance.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CReplyToCacher.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CReplyToCacherInstance.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/CReplyToResolverInstance.cpp
libCommIntegrationSubsys_la_SOURCES += Subsystems/commIntegration/src/commIntegration.cpp

libCommIntegrationSubsys_la_CPPFLAGS =
libCommIntegrationSubsys_la_CPPFLAGS += @GLIB2_CPPFLAGS@
libCommIntegrationSubsys_la_CPPFLAGS += @LOG4CPP_CPPFLAGS@
libCommIntegrationSubsys_la_CPPFLAGS += @SSL_CPPFLAGS@
libCommIntegrationSubsys_la_CPPFLAGS += @LIBRABBITMQ_CPPFLAGS@

libCommIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Framework/Framework/include
libCommIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Communication/amqpListener/include
libCommIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Communication/amqpCore/include
libCommIntegrationSubsys_la_CPPFLAGS += -I$(top_srcdir)/common-agent/Cpp/Communication/Subsystems/commIntegration/include
libCommIntegrationSubsys_la_LIBADD =
libCommIntegrationSubsys_la_LIBADD += @GLIB2_LIBS@
libCommIntegrationSubsys_la_LIBADD += @LOG4CPP_LIBS@
libCommIntegrationSubsys_la_LIBADD += @SSL_LIBS@
libCommIntegrationSubsys_la_LIBADD += -ldl
libCommIntegrationSubsys_la_LIBADD += @LIBRABBITMQ_LIBS@
libCommIntegrationSubsys_la_LIBADD += ../Framework/libFramework.la
libCommIntegrationSubsys_la_LIBADD += ../Communication/libCommAmqpIntegration.la

libCommIntegrationSubsys_la_LDFLAGS += -shared

