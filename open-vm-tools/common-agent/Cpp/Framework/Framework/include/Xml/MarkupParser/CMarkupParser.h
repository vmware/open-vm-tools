/*
 *	 Author: mdonahue
 *  Created: Jan 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMARKUPPARSER_H_
#define CMARKUPPARSER_H_

#include <list>
#include <map>

namespace Caf {

/*
 * Wrapper around glib GMarkupParser to store results in std containers
 */
namespace MarkupParser {

// attribute name, value
typedef std::pair<std::string, std::string> Attribute;
typedef std::list<Attribute> Attributes;

struct AttributeName : public std::binary_function<Attribute, std::string, bool> {
	bool operator()(const Attribute& attr, const std::string& name) const {
		return (attr.first.compare(name) == 0);
	}
};

struct Element;
CAF_DECLARE_SMART_POINTER(Element);
struct Element {
	Element() {}
	std::string name;
	std::string value;
	Attributes attributes;
	typedef std::list<SmartPtrElement> Children;
	Children children;

	CAF_CM_DECLARE_NOCOPY(Element);
};

struct ElementName : public std::binary_function<SmartPtrElement, std::string, bool> {
	bool operator()(const SmartPtrElement& element, const std::string& name) const {
		return (element->name.compare(name) == 0);
	}
};

SmartPtrElement MARKUPPARSER_LINKAGE parseString(const std::string& xml);

SmartPtrElement MARKUPPARSER_LINKAGE parseFile(const std::string& file);

typedef Element::Children::iterator ChildIterator;
typedef Attributes::iterator AttributeIterator;

ChildIterator MARKUPPARSER_LINKAGE findChild(SmartPtrElement& element, const std::string& name);

AttributeIterator MARKUPPARSER_LINKAGE findAttribute(Attributes& attributes, const std::string& name);

std::string MARKUPPARSER_LINKAGE getAttributeValue(SmartPtrElement& element, const std::string& name);

}}

#endif /* CMARKUPPARSER_H_ */
