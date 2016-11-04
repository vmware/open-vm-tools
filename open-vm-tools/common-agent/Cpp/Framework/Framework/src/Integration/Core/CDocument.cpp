/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Integration/Core/CDocument.h"
#include "Common/IAppConfig.h"

using namespace Caf;

CDocument::CDocument() :
	_isInitialized(false),
	CAF_CM_INIT("CDocument") {
}

CDocument::~CDocument() {
}

void CDocument::initialize(
	const SmartPtrCXmlElement& xmlElement) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(xmlElement);

		_xmlElement = xmlElement;

		// iterate over all attributes and resolve the attribute values
		// for environment and appConfig value references
		SmartPtrIAppConfig appConfig = getAppConfig();
		std::deque<SmartPtrCXmlElement> stack;
		stack.push_back(_xmlElement);
		while (stack.size()) {
			SmartPtrCXmlElement element = stack.front();
			stack.pop_front();
			CXmlElement::SmartPtrCAttributeCollection attributes = element->getAllAttributes();
			for (TConstMapIterator<CAttributeCollection> attribute(*attributes);
					attribute;
					attribute++) {
				element->setAttribute(
						attribute.getKey(),
						appConfig->resolveValue(*attribute));
			}
			CXmlElement::SmartPtrCElementCollection children = element->getAllChildren();
			std::transform(
					children->begin(),
					children->end(),
					std::insert_iterator<std::deque<SmartPtrCXmlElement> >(stack, stack.end()),
					select2nd<CXmlElement::CElementCollection::value_type>());
		}

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

SmartPtrCXmlElement CDocument::getXmlElement() const {
	CAF_CM_FUNCNAME_VALIDATE("getXmlElement");

	SmartPtrCXmlElement rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _xmlElement;
	}
	CAF_CM_EXIT;

	return rc;
}

std::string CDocument::findRequiredAttribute(const std::string& name) const {
	CAF_CM_FUNCNAME_VALIDATE("findRequiredAttribute");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _xmlElement->findRequiredAttribute(name);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string CDocument::findOptionalAttribute(const std::string& name) const {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalAttribute");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _xmlElement->findOptionalAttribute(name);
	}
	CAF_CM_EXIT;

	return rc;
}

SmartPtrIDocument CDocument::findRequiredChild(const std::string& name) const {
	CAF_CM_FUNCNAME_VALIDATE("findRequiredChild");

	SmartPtrIDocument rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		const SmartPtrCXmlElement child = _xmlElement->findRequiredChild(name);

		SmartPtrCDocument document;
		document.CreateInstance();
		document->initialize(child);

		rc = document;
	}
	CAF_CM_EXIT;

	return rc;
}

SmartPtrIDocument CDocument::findOptionalChild(const std::string& name) const {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalChild");

	SmartPtrIDocument rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		const SmartPtrCXmlElement child = _xmlElement->findOptionalChild(name);
		if (! child.IsNull()) {
			SmartPtrCDocument document;
			document.CreateInstance();
			document->initialize(child);

			rc = document;
		}
	}
	CAF_CM_EXIT;

	return rc;
}

IDocument::SmartPtrCAttributeCollection CDocument::getAllAttributes() const {
	CAF_CM_FUNCNAME_VALIDATE("getAllAttributes");

	IDocument::SmartPtrCAttributeCollection rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		rc.CreateInstance();

		const CXmlElement::SmartPtrCAttributeCollection attributesXml =
			_xmlElement->getAllAttributes();
		for (TConstIterator<CXmlElement::CAttributeCollection> attributeXmlIter(*attributesXml);
			attributeXmlIter; attributeXmlIter++) {
			rc->insert(std::make_pair(attributeXmlIter->first, attributeXmlIter->second));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

IDocument::SmartPtrCChildCollection CDocument::getAllChildren() const {
	CAF_CM_FUNCNAME_VALIDATE("getAllChildren");

	IDocument::SmartPtrCChildCollection rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		rc.CreateInstance();

		const CXmlElement::SmartPtrCElementCollection childrenXml =
			_xmlElement->getAllChildren();
		for (TConstIterator<CXmlElement::CElementCollection> childXmlIter(*childrenXml);
			childXmlIter; childXmlIter++) {
			SmartPtrCDocument document;
			document.CreateInstance();
			document->initialize(childXmlIter->second);

			rc->insert(std::make_pair(childXmlIter->first, document));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

IDocument::SmartPtrCOrderedChildCollection CDocument::getAllChildrenInOrder() const {
	CAF_CM_FUNCNAME_VALIDATE("getAllChildrenInOrder");

	IDocument::SmartPtrCOrderedChildCollection rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		rc.CreateInstance();

		const CXmlElement::SmartPtrCOrderedElementCollection childrenXml =
			_xmlElement->getAllChildrenInOrder();
		for (TConstIterator<CXmlElement::COrderedElementCollection> childXmlIter(*childrenXml);
			childXmlIter; childXmlIter++) {
			SmartPtrCDocument document;
			document.CreateInstance();
			document->initialize(*childXmlIter);
			rc->push_back(document);
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string CDocument::getName() const {
	CAF_CM_FUNCNAME_VALIDATE("getName");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _xmlElement->getName();
	}
	CAF_CM_EXIT;

	return rc;
}

std::string CDocument::getValue() const {
	CAF_CM_FUNCNAME_VALIDATE("getValue");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _xmlElement->getValue();
	}
	CAF_CM_EXIT;

	return rc;
}

std::string CDocument::getPath() const {
	CAF_CM_FUNCNAME_VALIDATE("getPath");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _xmlElement->getPath();
	}
	CAF_CM_EXIT;

	return rc;
}

void CDocument::saveToFile(const std::string& filename) const {
	CAF_CM_FUNCNAME_VALIDATE("saveToFile");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		_xmlElement->saveToFile(filename);
	}
	CAF_CM_EXIT;
}

std::string CDocument::saveToString() const {
	CAF_CM_FUNCNAME_VALIDATE("saveToString");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _xmlElement->saveToString();
	}
	CAF_CM_EXIT;

	return rc;
}

std::string CDocument::saveToStringRaw() const {
	CAF_CM_FUNCNAME_VALIDATE("saveToStringRaw");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _xmlElement->saveToStringRaw();
	}
	CAF_CM_EXIT;

	return rc;
}
