/*
 *  Created on: Aug 13, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CMessageHandlerChainInstance.h"
#include "CObjectFactoryTables.h"
#include "CServiceActivatorInstance.h"
#include "CWireTapInstance.h"
#include "CPublishSubscribeChannelInstance.h"
#include "CFileInboundChannelAdapterInstance.h"
#include "CFileOutboundChannelAdapterInstance.h"
#include "CFileToStringTransformerInstance.h"
#include "CHeaderEnricherTransformerInstance.h"
#include "CHeaderValueRouterInstance.h"
#include "CPayloadContentRouterInstance.h"
#include "CLoggingChannelAdapterInstance.h"
#include "CRecipientListRouterInstance.h"
#include "CRouterInstance.h"
#include "CXPathHeaderEnricherTransformerInstance.h"

using namespace Caf;

const ObjectCreatorMap::value_type CObjectFactoryTables::objectCreatorEntries[] = {
		ObjectCreatorMap::value_type(
				"file-to-string-transformer",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"header-enricher",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"transformer",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"header-value-router",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"payload-content-router",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"splitter",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"service-activator",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"logging-channel-adapter",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"wire-tap",
				CreateIntegrationObject<SmartPtrCWireTapInstance>),
		ObjectCreatorMap::value_type(
				"publish-subscribe-channel",
				CreateIntegrationObject<SmartPtrCPublishSubscribeChannelInstance>),
		ObjectCreatorMap::value_type(
				"file-inbound-channel-adapter",
				CreateIntegrationObject<SmartPtrCFileInboundChannelAdapterInstance>),
		ObjectCreatorMap::value_type(
				"file-outbound-channel-adapter",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"channel",
				NULL),
		ObjectCreatorMap::value_type(
				"recipient-list-router",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"chain",
				CreateIntegrationObject<SmartPtrCMessageHandlerChainInstance>),
		ObjectCreatorMap::value_type(
				"router",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
		ObjectCreatorMap::value_type(
				"xpath-header-enricher",
				CreateIntegrationObject<SmartPtrCServiceActivatorInstance>),
	};

const ObjectCreatorMap CObjectFactoryTables::objectCreatorMap(
		objectCreatorEntries,
		objectCreatorEntries +
			(sizeof(objectCreatorEntries)/sizeof(objectCreatorEntries[0])));


const MessageHandlerObjectCreatorMap::value_type
	CObjectFactoryTables::messageHandlerObjectCreatorEntries[] = {
		MessageHandlerObjectCreatorMap::value_type(
				"service-activator",
				MessageHandlerObjectCreatorMap::mapped_type(
						NULL,
						true)),
		MessageHandlerObjectCreatorMap::value_type(
				"file-to-string-transformer",
				MessageHandlerObjectCreatorMap::mapped_type(
						CreateIntegrationObject<SmartPtrCFileToStringTransformerInstance>,
						true)),
		MessageHandlerObjectCreatorMap::value_type(
				"header-enricher",
				MessageHandlerObjectCreatorMap::mapped_type(
						CreateIntegrationObject<SmartPtrCHeaderEnricherTransformerInstance>,
						true)),
		MessageHandlerObjectCreatorMap::value_type(
				"header-value-router",
				MessageHandlerObjectCreatorMap::mapped_type(
						CreateIntegrationObject<SmartPtrCHeaderValueRouterInstance>,
						false)),
		MessageHandlerObjectCreatorMap::value_type(
				"payload-content-router",
				MessageHandlerObjectCreatorMap::mapped_type(
						CreateIntegrationObject<SmartPtrCPayloadContentRouterInstance>,
						false)),
		MessageHandlerObjectCreatorMap::value_type(
				"logging-channel-adapter",
				MessageHandlerObjectCreatorMap::mapped_type(
						CreateIntegrationObject<SmartPtrCLoggingChannelAdapterInstance>,
						false)),
		MessageHandlerObjectCreatorMap::value_type(
				"splitter",
				MessageHandlerObjectCreatorMap::mapped_type(
						NULL,
						true)),
		MessageHandlerObjectCreatorMap::value_type(
				"transformer",
				MessageHandlerObjectCreatorMap::mapped_type(
						NULL,
						true)),
		MessageHandlerObjectCreatorMap::value_type(
				"recipient-list-router",
				MessageHandlerObjectCreatorMap::mapped_type(
						CreateIntegrationObject<SmartPtrCRecipientListRouterInstance>,
						false)),
		MessageHandlerObjectCreatorMap::value_type(
				"router",
				MessageHandlerObjectCreatorMap::mapped_type(
						CreateIntegrationObject<SmartPtrCRouterInstance>,
						false)),
		MessageHandlerObjectCreatorMap::value_type(
				"file-outbound-channel-adapter",
				MessageHandlerObjectCreatorMap::mapped_type(
						CreateIntegrationObject<SmartPtrCFileOutboundChannelAdapterInstance>,
						false)),
		MessageHandlerObjectCreatorMap::value_type(
				"xpath-header-enricher",
				MessageHandlerObjectCreatorMap::mapped_type(
						CreateIntegrationObject<SmartPtrCXPathHeaderEnricherTransformerInstance>,
						true))
	};

const MessageHandlerObjectCreatorMap CObjectFactoryTables::messageHandlerObjectCreatorMap(
		messageHandlerObjectCreatorEntries,
		messageHandlerObjectCreatorEntries +
			(sizeof(messageHandlerObjectCreatorEntries)/
			sizeof(messageHandlerObjectCreatorEntries[0])));
