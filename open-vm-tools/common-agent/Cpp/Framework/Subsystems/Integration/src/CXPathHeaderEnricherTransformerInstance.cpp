/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CXPathHeaderEnricherItem.h"
#include "Common/IAppContext.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"

#include "CXPathHeaderEnricherTransformerInstance.h"

using namespace Caf;

CXPathHeaderEnricherTransformerInstance::CXPathHeaderEnricherTransformerInstance() :
	_isInitialized(false),
	_defaultOverwrite(true),
	_shouldSkipNulls(true),
	CAF_CM_INIT_LOG("CXPathHeaderEnricherTransformerInstance") {
}

CXPathHeaderEnricherTransformerInstance::~CXPathHeaderEnricherTransformerInstance() {
}

void CXPathHeaderEnricherTransformerInstance::initialize(
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

std::string CXPathHeaderEnricherTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CXPathHeaderEnricherTransformerInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	SmartPtrIAppConfig appConfig = getAppConfig();

	const std::string defaultOverwriteStr =
		_configSection->findOptionalAttribute("default-overwrite");
	const std::string shouldSkipNullsStr =
		_configSection->findOptionalAttribute("should-skip-nulls");

	_defaultOverwrite =
		(defaultOverwriteStr.empty() || defaultOverwriteStr.compare("true") == 0) ? true : false;
	_shouldSkipNulls =
		(shouldSkipNullsStr.empty() || shouldSkipNullsStr.compare("true") == 0) ? true : false;

	const IDocument::SmartPtrCChildCollection configChildren =
		_configSection->getAllChildren();
	for (TSmartConstMapIterator<IDocument::CChildCollection> configIter(*configChildren);
		configIter; configIter++) {
		const SmartPtrIDocument config = *configIter;

		if (config->getName().compare("header") == 0) {
			SmartPtrCXPathHeaderEnricherItem item;
			item.CreateInstance();
			item->initialize(config, _defaultOverwrite);

			_headerItems.insert(std::make_pair(item->getName(), item));
		} else {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_INVALID_DATA,
				"Configuration section contains unrecognized entry - %s", _id.c_str());
		}
	}
}

SmartPtrIIntMessage CXPathHeaderEnricherTransformerInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initialize(message->getPayload(),
		message->getHeaders(), IIntMessage::SmartPtrCHeaders());
	SmartPtrIIntMessage newMessage = messageImpl;

	IIntMessage::SmartPtrCHeaders newHeaders = newMessage->getHeaders();
	const std::string payloadXmlStr = newMessage->getPayloadStr();

	for (TConstMapIterator<CXPathHeaderEnricherTransformerInstance::Items> headerItemIter(_headerItems);
		headerItemIter; headerItemIter++) {
		const std::string name = headerItemIter.getKey();
		const SmartPtrCXPathHeaderEnricherItem value = *headerItemIter;

		if (isInsertable(name, value, newHeaders)) {
			const std::string xpathRc = evaluateXPathExpression(name, value, payloadXmlStr);
			if (xpathRc.empty()) {
				if (! _shouldSkipNulls) {
					CAF_CM_LOG_INFO_VA1("Removing header from unresolvable expression - %s",
						name.c_str());
					newHeaders->erase(name);
				}
			} else {
				CAF_CM_LOG_DEBUG_VA2("Inserting/updating a header value - %s = %s",
					name.c_str(), xpathRc.c_str());
				(*newHeaders)[name] =
					std::make_pair(CVariant::createString(xpathRc), SmartPtrICafObject());
			}
		}
	}

	return newMessage;
}

bool CXPathHeaderEnricherTransformerInstance::isInsertable(
	const std::string& name,
	const SmartPtrCXPathHeaderEnricherItem& value,
	const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_FUNCNAME_VALIDATE("isInsertable");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);
	CAF_CM_VALIDATE_SMARTPTR(value);
	CAF_CM_VALIDATE_SMARTPTR(headers);

	bool rc = false;
	if (value->getEvaluationType().compare("STRING_RESULT") == 0) {
		if (headers->find(name) == headers->end()) {
			rc = true;
		} else {
			if (value->getOverwrite()) {
				CAF_CM_LOG_DEBUG_VA1("Existing header will be overwritten - name: %s",
					name.c_str());
				rc = true;
			} else {
				CAF_CM_LOG_WARN_VA1("Existing header will not be overwritten - name: %s",
					name.c_str());
			}
		}
	} else {
		CAF_CM_LOG_ERROR_VA2("Evaluation type not supported - name: %s, type: %s",
			name.c_str(), value->getEvaluationType().c_str());
	}

	return rc;
}

std::string CXPathHeaderEnricherTransformerInstance::evaluateXPathExpression(
	const std::string& name,
	const SmartPtrCXPathHeaderEnricherItem& value,
	const std::string& payloadXmlStr) {
	CAF_CM_FUNCNAME_VALIDATE("isInsertable");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);
	CAF_CM_VALIDATE_SMARTPTR(value);
	CAF_CM_VALIDATE_STRING(payloadXmlStr);

	std::string rc;
	const std::string expr = value->getXpathExpression();
	if (expr.empty()) {
		CAF_CM_LOG_ERROR_VA1(
			"xpath-expression is required until xpath-expression-ref is supported - name: %s",
			name.c_str());
	} else {
		if (! value->getXpathExpressionRef().empty()) {
			CAF_CM_LOG_WARN_VA1(
				"Both xpath-expression and xpath-expression-ref cannot be specified... Using xpath-expression - name: %s",
				name.c_str());
		}

		if (expr.find_first_of('@') != 0) {
			CAF_CM_LOG_ERROR_VA2(
				"Currently, only root-level attributes are supported - name: %s, xpath-expression: %s",
				name.c_str(), expr.c_str());
		} else {
			const std::string attr = expr.substr(1);

			const SmartPtrCXmlElement rootXml =
				CXmlUtils::parseString(payloadXmlStr, std::string());

			const std::string payloadType = rootXml->getName();
			const std::string attrVal = rootXml->findOptionalAttribute(attr);
			if (attrVal.empty()) {
				CAF_CM_LOG_WARN_VA2(
					"Attribute not found at root level: %s",
					name.c_str(), expr.c_str());
			} else {
				rc = attrVal;
			}
		}
	}

	return rc;
}
