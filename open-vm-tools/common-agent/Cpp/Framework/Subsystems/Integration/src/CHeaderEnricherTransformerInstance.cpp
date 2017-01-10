/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IBean.h"
#include "IVariant.h"
#include "Integration/Core/CExpressionHandler.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/ITransformer.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#include "CHeaderEnricherTransformerInstance.h"
#include "Integration/Core/MessageHeaders.h"

using namespace Caf;

CHeaderEnricherTransformerInstance::CHeaderEnricherTransformerInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CHeaderEnricherTransformerInstance") {
}

CHeaderEnricherTransformerInstance::~CHeaderEnricherTransformerInstance() {
}

void CHeaderEnricherTransformerInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");
	_configSection = configSection;

	if (_configSection->getAllChildren()->empty()) {
		CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_INVALID_DATA,
			"Configuration section is empty - %s", _id.c_str());
	}

	_isInitialized = true;
}

std::string CHeaderEnricherTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CHeaderEnricherTransformerInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	SmartPtrIAppConfig appConfig = getAppConfig();

	const IDocument::SmartPtrCChildCollection configChildren =
		_configSection->getAllChildren();
	for (TSmartConstMapIterator<IDocument::CChildCollection> configIter(*configChildren);
		configIter; configIter++) {
		const SmartPtrIDocument config = *configIter;

		if (config->getName().compare("header") == 0) {
			const std::string headerName = config->findRequiredAttribute("name");
			const std::string headerRef = config->findOptionalAttribute("ref");
			const std::string headerValue = config->findOptionalAttribute("value");
			const std::string expressionValue = config->findOptionalAttribute("expression");

			if (! headerRef.empty()) {
				CAF_CM_LOG_DEBUG_VA2("Creating the header enricher bean - %s = %s",
					headerName.c_str(),
					headerRef.c_str());
				SmartPtrIBean bean = appContext->getBean(headerRef);
				SmartPtrITransformer transformer;
				transformer.QueryInterface(bean, false);
				if (transformer.IsNull()) {
					CAF_CM_EXCEPTIONEX_VA1(
							NoSuchInterfaceException,
							0,
							"Bean is not a transformer - %s",
							headerRef.c_str());
				}
				_headerWithRef.insert(std::make_pair(headerName, transformer));
			} else if (! headerValue.empty()) {
				CAF_CM_LOG_DEBUG_VA2("Creating the header enricher value - %s = %s",
						headerName.c_str(), headerValue.c_str());
				_headerWithValue.insert(std::make_pair(headerName, headerValue));
			} else if (! expressionValue.empty()) {
				CAF_CM_LOG_DEBUG_VA2("Creating the header enricher expression - %s = %s",
						headerName.c_str(), expressionValue.c_str());
				SmartPtrCExpressionHandler expressionHandler;
				expressionHandler.CreateInstance();
				expressionHandler->init(
						appConfig,
						appContext,
						expressionValue);
				_headerWithExpression.insert(std::make_pair(headerName, expressionHandler));
			} else {
				CAF_CM_EXCEPTIONEX_VA2(
						InvalidArgumentException,
						ERROR_INVALID_DATA,
						"Configuration error: unrecognized header value type attribute: [id='%s'][header='%s']",
						_id.c_str(),
						headerName.c_str());
			}
		} else if (config->getName().compare("error-channel") == 0) {
			_errorChannelRef = config->findRequiredAttribute("ref");
		} else {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_INVALID_DATA,
				"Configuration section contains unrecognized entry - %s", _id.c_str());
		}
	}
}

SmartPtrIIntMessage CHeaderEnricherTransformerInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initialize(message->getPayload(),
		message->getHeaders(), IIntMessage::SmartPtrCHeaders());
	SmartPtrIIntMessage newMessage = messageImpl;

	for (TSmartMapIterator<Transformers> headerWithRefIter(_headerWithRef);
		headerWithRefIter; headerWithRefIter++) {
		const SmartPtrIIntMessage transformMessage =
				headerWithRefIter->transformMessage(newMessage);
		SmartPtrCIntMessage tmpMessageImpl;
		tmpMessageImpl.CreateInstance();
		tmpMessageImpl->initialize(message->getPayload(),
			transformMessage->getHeaders(), newMessage->getHeaders());
		newMessage = tmpMessageImpl;
	}

	IIntMessage::SmartPtrCHeaders newHeaders = newMessage->getHeaders();

	for (TSmartMapIterator<Expressions> expressionIter(_headerWithExpression);
			expressionIter;
			expressionIter++) {
		const SmartPtrIVariant value = expressionIter->evaluate(newMessage);
		CAF_CM_LOG_DEBUG_VA2(
				"Inserting/updating a header value - %s = %s",
				expressionIter.getKey().c_str(),
				value->toString().c_str());
		(*newHeaders)[expressionIter.getKey()] =
			std::make_pair(value, SmartPtrICafObject());
	}

	if (! _errorChannelRef.empty()) {
		CAF_CM_LOG_DEBUG_VA1("Inserting/updating a new error channel - %s",
			_errorChannelRef.c_str());
		(*newHeaders)[MessageHeaders::_sERROR_CHANNEL] =
			std::make_pair(CVariant::createString(_errorChannelRef),
				SmartPtrICafObject());
	}

	for (TConstMapIterator<Cmapstrstr> headerWithValueIter(_headerWithValue);
		headerWithValueIter; headerWithValueIter++) {
		const std::string name = headerWithValueIter.getKey();
		const std::string value = *headerWithValueIter;

		CAF_CM_LOG_DEBUG_VA2("Inserting/updating a header value - %s = %s",
			name.c_str(), value.c_str());
		(*newHeaders)[name] =
			std::make_pair(CVariant::createString(value), SmartPtrICafObject());
	}

	return newMessage;
}
