/*
 *  Created on: Aug 9, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IVariant.h"
#include "Integration/Core/CExpressionHandler.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#include "CRecipientListRouterInstance.h"

using namespace Caf;

CRecipientListRouterInstance::CRecipientListRouterInstance() :
	_isInitialized(false),
	_ignoreSendFailures(false),
	_timeout(-1),
	CAF_CM_INIT_LOG("CRecipientListRouterInstance") {
}

CRecipientListRouterInstance::~CRecipientListRouterInstance() {
}

void CRecipientListRouterInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_id = configSection->findRequiredAttribute("id");

		std::string val = configSection->findOptionalAttribute("timeout");
		if (val.length()) {
			_timeout = CStringConv::fromString<int32>(val);
		}

		val = configSection->findOptionalAttribute("ignore-send-failures");
		_ignoreSendFailures = (val == "true");

		Csetstr channelIds;
		const IDocument::SmartPtrCChildCollection childCollection = configSection->getAllChildren();
		for(TSmartConstMultimapIterator<IDocument::CChildCollection> childIter(*childCollection);
				childIter;
				childIter++) {
			const std::string sectionName = childIter.getKey();
			if (sectionName == "recipient") {
				const SmartPtrIDocument document = *childIter;
				const std::string channelId = document->findRequiredAttribute("channel");
				const std::string selectorExpression
					= document->findOptionalAttribute("selector-expression");

				if (!channelIds.insert(channelId).second) {
					CAF_CM_EXCEPTIONEX_VA2(
							DuplicateElementException,
							0,
							"Duplicate channelId '%s' in "
							"recipient-list-router definition '%s'",
							channelId.c_str(),
							_id.c_str());
				}

				if (selectorExpression.length()) {
					_selectorDefinitions.insert(
							Cmapstrstr::value_type(channelId, selectorExpression));
				} else {
					_staticChannelIds.push_back(channelId);
				}
			}
		}

		if (!_staticChannelIds.size() && !_selectorDefinitions.size()) {
			CAF_CM_EXCEPTIONEX_VA1(
					NoSuchElementException,
					0,
					"No recipients were listed in the definition of "
					"recipient-list-router '%s'",
					_id.c_str());
		}
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CRecipientListRouterInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CRecipientListRouterInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(appContext);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);

		SmartPtrIAppConfig appConfig = getAppConfig();

		for (TConstIterator<Cdeqstr> channelId(_staticChannelIds);
				channelId;
				channelId++) {
			_staticChannels.push_back(
					channelResolver->resolveChannelName(*channelId));
		}

		for (TConstMapIterator<Cmapstrstr> selectorPair(_selectorDefinitions);
				selectorPair;
				selectorPair++) {
			SmartPtrIMessageChannel channel =
					channelResolver->resolveChannelName(selectorPair.getKey());
			SmartPtrCExpressionHandler handler;
			handler.CreateInstance();
			handler->init(
					appConfig,
					appContext,
					*selectorPair);
			_selectorChannels.push_back(std::make_pair(handler, channel));
		}

		CAbstractMessageRouter::init(
				SmartPtrIMessageChannel(),
				_ignoreSendFailures,
				_timeout);
	}
	CAF_CM_EXIT;
}

CRecipientListRouterInstance::ChannelCollection
CRecipientListRouterInstance::getTargetChannels(
		const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("getTargetChannels");
	ChannelCollection channels;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		// Static channels always get the message
		std::copy(
				_staticChannels.begin(),
				_staticChannels.end(),
				std::back_inserter(channels));

		// Execute the selector expression(s) on the message
		// and add the channels for the expressions that return 'true'
		for (TConstIterator<SelectorChannelCollection> selector(_selectorChannels);
				selector;
				selector++) {
			SmartPtrIVariant evalResult = selector->first->evaluate(message);
			if (evalResult->isBool()) {
				if (CAF_CM_IS_LOG_DEBUG_ENABLED) {
					CAF_CM_LOG_DEBUG_VA3(
							"recipient-list-router [%s] selector-expression [%s] returned '%s'",
							_id.c_str(),
							selector->first->toString().c_str(),
							evalResult->toString().c_str());
				}
				if (g_variant_get_boolean(evalResult->get())) {
					channels.push_back(selector->second);
				}
			} else {
				CAF_CM_EXCEPTIONEX_VA2(
						InvalidArgumentException,
						0,
						"recipient-list-router [%s] illegal selector-expression [%s] : "
						"selector-expression results must return boolean values.",
						_id.c_str(),
						selector->first->toString().c_str());
			}
		}
	}
	CAF_CM_EXIT;

	return channels;
}
