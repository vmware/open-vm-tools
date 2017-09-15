/*
 *	 Author: mdonahue
 *  Created: Dec 3, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Exception/CCafException.h"

using namespace Caf;

const std::string CXmlElement::CDATA_BEG = "<![CDATA[";
const std::string CXmlElement::CDATA_END = "]]>";

CXmlElement::CXmlElement() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CXmlElement") {
}

CXmlElement::~CXmlElement() {
}

void CXmlElement::initialize(
	const MarkupParser::SmartPtrElement& element,
	const std::string& path) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_PTR(element);
	CAF_CM_VALIDATE_STRING(path);

	_element = element;
	_path = path;
	_isInitialized = true;
}

MarkupParser::SmartPtrElement CXmlElement::getInternalElement() {
	return _element;
}

std::string CXmlElement::findRequiredAttribute(const std::string& name) const {
	CAF_CM_FUNCNAME("findRequiredAttribute");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	CAF_CM_VALIDATE_COND_VA3(!_element->attributes.empty(),
		"element (%s) does not contain any attributes (%s) in %s", _element->name.c_str(),
		name.c_str(), _path.c_str());

	MarkupParser::AttributeIterator iter = MarkupParser::findAttribute(
		_element->attributes, name);
	CAF_CM_VALIDATE_COND_VA3(iter != _element->attributes.end(),
		"element (%s) does not contain required attribute (%s) in %s",
		_element->name.c_str(), name.c_str(), _path.c_str());

	std::string rc = iter->second;
	CAF_CM_VALIDATE_STRING(rc);

	return rc;
}

std::string CXmlElement::findOptionalAttribute(const std::string& name) const {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalAttribute");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	std::string rc;
	if (!_element->attributes.empty()) {
		MarkupParser::AttributeIterator iter = MarkupParser::findAttribute(
			_element->attributes, name);
		if (iter != _element->attributes.end()) {
			rc = iter->second;
		}
	}

	return rc;
}

SmartPtrCXmlElement CXmlElement::findRequiredChild(const std::string& name) const {
	CAF_CM_FUNCNAME("findRequiredChild");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	const MarkupParser::ChildIterator iter = MarkupParser::findChild(_element, name);
	if (iter == _element->children.end()) {
		CAF_CM_LOG_INFO_VA1("Child not found: %s", name.c_str());
	}
	CAF_CM_VALIDATE_COND_VA3(iter != _element->children.end(),
		"element (%s) does not contain required child (%s) in %s", _element->name.c_str(),
		name.c_str(), _path.c_str());

	SmartPtrCXmlElement rc;
	rc.CreateInstance();
	rc->initialize(*iter, _path);

	return rc;
}

SmartPtrCXmlElement CXmlElement::findOptionalChild(const std::string& name) const {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalChild");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	SmartPtrCXmlElement rc;
	const MarkupParser::ChildIterator iter = MarkupParser::findChild(_element, name);
	if (iter != _element->children.end()) {
		rc.CreateInstance();
		rc->initialize(*iter, _path);
	}

	return rc;
}

CXmlElement::SmartPtrCAttributeCollection CXmlElement::getAllAttributes() const {
	CAF_CM_FUNCNAME_VALIDATE("getAllAttributes");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrCAttributeCollection rc;
	rc.CreateInstance();

	for (TConstIterator<MarkupParser::Attributes> attribute(_element->attributes);
		attribute; attribute++) {
		rc->insert(std::make_pair((*attribute).first, (*attribute).second));
	}

	return rc;
}

CXmlElement::SmartPtrCElementCollection CXmlElement::getAllChildren() const {
	CAF_CM_FUNCNAME_VALIDATE("getAllChildren");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrCElementCollection rc;
	rc.CreateInstance();

	SmartPtrCXmlElement xmlElement;
	for (TConstIterator<MarkupParser::Element::Children> child(_element->children); child;
		child++) {
		xmlElement.CreateInstance();
		xmlElement->initialize(*child, _path);
		rc->insert(std::make_pair((*child)->name, xmlElement));
	}

	return rc;
}

CXmlElement::SmartPtrCOrderedElementCollection CXmlElement::getAllChildrenInOrder() const {
	CAF_CM_FUNCNAME_VALIDATE("getAllChildrenInOrder");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrCOrderedElementCollection rc;
	rc.CreateInstance();

	SmartPtrCXmlElement xmlElement;
	for (TConstIterator<MarkupParser::Element::Children> child(_element->children); child;
		child++) {
		xmlElement.CreateInstance();
		xmlElement->initialize(*child, _path);
		rc->push_back(xmlElement);
	}

	return rc;
}

CXmlElement::SmartPtrCElementCollection CXmlElement::findRequiredChildren(
	const std::string& name) const {
	CAF_CM_FUNCNAME("findRequiredChildren");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	SmartPtrCElementCollection rc = findOptionalChildren(name);

	if (rc.IsNull() || rc->empty()) {
		CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
			"Children not found: %s", name.c_str());
	}

	return rc;
}

CXmlElement::SmartPtrCElementCollection CXmlElement::findOptionalChildren(
	const std::string& name) const {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalChildren");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	SmartPtrCElementCollection rc;
	rc.CreateInstance();

	SmartPtrCXmlElement xmlElement;
	for (TConstIterator<MarkupParser::Element::Children> child(_element->children); child;
		child++) {
		if (((*child)->name).compare(name) == 0) {
			xmlElement.CreateInstance();
			xmlElement->initialize(*child, _path);
			rc->insert(std::make_pair((*child)->name, xmlElement));
		}
	}

	return rc;
}

std::string CXmlElement::getName() const {
	CAF_CM_FUNCNAME_VALIDATE("getName");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _element->name;
}

std::string CXmlElement::getValue() const {
	CAF_CM_FUNCNAME_VALIDATE("getValue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _element->value;
}

std::string CXmlElement::getCDataValue() const {
	CAF_CM_FUNCNAME_VALIDATE("getCDataValue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rc = _element->value;
	if (! rc.empty()) {
		const std::string::size_type cdataBegPos = rc.find(CDATA_BEG);
		const std::string::size_type cdataEndPos = rc.find(CDATA_END);
		if ((0 == cdataBegPos) && (std::string::npos != cdataEndPos)) {
			rc = rc.substr(0, cdataEndPos);
			rc = rc.substr(cdataBegPos + CDATA_BEG.length());
		}
	}

	return rc;
}

std::string CXmlElement::getPath() const {
	CAF_CM_FUNCNAME_VALIDATE("getPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _path;
}

void CXmlElement::addAttribute(const std::string& name, const std::string& value) {
	CAF_CM_FUNCNAME("addAttribute");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);
	CAF_CM_VALIDATE_STRING(value);

	if (!_element->attributes.empty()) {
		MarkupParser::AttributeIterator iter = MarkupParser::findAttribute(
			_element->attributes, name);
		CAF_CM_VALIDATE_COND_VA3(iter == _element->attributes.end(),
			"element (%s) already contains attribute (%s) in %s", _element->name.c_str(),
			name.c_str(), _path.c_str());
	}

	_element->attributes.push_back(std::make_pair(name, value));
}

void CXmlElement::removeAttribute(const std::string& name) {

	CAF_CM_FUNCNAME_VALIDATE("removeAttribute");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	if (!_element->attributes.empty()) {
		MarkupParser::AttributeIterator iter = MarkupParser::findAttribute(
			_element->attributes, name);
		if (iter != _element->attributes.end()) {
			_element->attributes.erase(iter);
		}
	}
}

void CXmlElement::setAttribute(const std::string& name, const std::string& value) {
	CAF_CM_FUNCNAME("setAttribute");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);
	CAF_CM_VALIDATE_STRING(value);

	CAF_CM_VALIDATE_COND_VA3(!_element->attributes.empty(),
		"element (%s) does not contain any attributes (%s) in %s",
		_element->name.c_str(), name.c_str(), _path.c_str());

	MarkupParser::AttributeIterator iter = MarkupParser::findAttribute(
		_element->attributes, name);
	CAF_CM_VALIDATE_COND_VA3(iter != _element->attributes.end(),
		"element (%s) does not contain required attribute (%s) in %s",
		_element->name.c_str(), name.c_str(), _path.c_str());

	iter->second = value;
}

SmartPtrCXmlElement CXmlElement::createAndAddElement(const std::string& name) {
	CAF_CM_FUNCNAME_VALIDATE("createAndAddElement");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	MarkupParser::SmartPtrElement element;
	element.CreateInstance();
	element->name = name;

	SmartPtrCXmlElement rc;
	rc.CreateInstance();
	rc->initialize(element, _path);

	_element->children.push_back(element);

	return rc;
}

void CXmlElement::addChild(const SmartPtrCXmlElement& xmlElement) {
	CAF_CM_FUNCNAME_VALIDATE("addChild");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(xmlElement);

	_element->children.push_back(xmlElement->getInternalElement());
}

void CXmlElement::removeChild(const std::string& name) {
	CAF_CM_FUNCNAME_VALIDATE("removeChild");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	const MarkupParser::ChildIterator iter = MarkupParser::findChild(_element, name);
	if (iter != _element->children.end()) {
		_element->children.erase(iter);
	}
}

void CXmlElement::setValue(const std::string& value) {
	CAF_CM_FUNCNAME_VALIDATE("setValue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(value);

	_element->value = value;
}

void CXmlElement::setCDataValue(const std::string& value) {
	CAF_CM_FUNCNAME_VALIDATE("setCDataValue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(value);

	if (std::string::npos == value.find(CDATA_BEG)) {
		_element->value = CDATA_BEG + value + CDATA_END;
	} else {
		_element->value = value;
	}
}

void CXmlElement::appendValue(const std::string& value) {
	CAF_CM_FUNCNAME_VALIDATE("appendValue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(value);

	if (_element->value.empty()) {
		_element->value = "";
	}

	_element->value += value;
}

void CXmlElement::saveToFile(const std::string& filename) const {
	CAF_CM_FUNCNAME_VALIDATE("saveToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(filename);

	CAF_CM_LOG_INFO_VA1("Saving XML to file \"%s\"", filename.c_str());

	const std::string xml = saveToString();
	FileSystemUtils::saveTextFile(filename, xml);
}

std::string CXmlElement::saveToString() const {
	CAF_CM_FUNCNAME_VALIDATE("saveToString");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rc = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
	saveToString(_element, rc);

	return rc;
}

std::string CXmlElement::saveToStringRaw() const {
	CAF_CM_FUNCNAME_VALIDATE("saveToStringRaw");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rc;
	saveToString(_element, rc);

	return rc;
}

void CXmlElement::saveToString(
	const MarkupParser::SmartPtrElement& element,
	std::string& xml) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CXmlElement", "saveToString");
	CAF_CM_VALIDATE_PTR(element);

	xml += "<" + element->name;
	for (TConstIterator<MarkupParser::Attributes> attribute(element->attributes);
		attribute; attribute++) {
		xml += " " + attribute->first + "=\"" + attribute->second + "\"";
	}

	if (element->value.empty() && element->children.empty()) {
		xml += "/>";
	} else {
		xml += ">" + element->value;
		for (TConstIterator<MarkupParser::Element::Children> child(element->children);
			child; child++) {
			saveToString(*child, xml);
		}
		xml += "</" + element->name + ">";
	}
}
