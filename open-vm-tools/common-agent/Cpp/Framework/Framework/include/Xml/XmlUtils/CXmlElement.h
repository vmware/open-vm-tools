/*
 *	 Author: mdonahue
 *  Created: Dec 3, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _CXmlElement_h
#define _CXmlElement_h


#include "Integration/IDocument.h"
#include "Xml/MarkupParser/CMarkupParser.h"

namespace Caf {

CAF_DECLARE_CLASS_AND_SMART_POINTER(CXmlElement);

class XMLUTILS_LINKAGE CXmlElement {
public:
	typedef std::map<std::string, std::string> CAttributeCollection;
	typedef std::multimap<std::string, SmartPtrCXmlElement> CElementCollection;
	typedef std::deque<SmartPtrCXmlElement> COrderedElementCollection;
	CAF_DECLARE_SMART_POINTER(CAttributeCollection);
	CAF_DECLARE_SMART_POINTER(CElementCollection);
	CAF_DECLARE_SMART_POINTER(COrderedElementCollection);

public:
	CXmlElement();
	virtual ~CXmlElement();

public:
	void initialize(const MarkupParser::SmartPtrElement& element, const std::string& path);
	MarkupParser::SmartPtrElement getInternalElement();

public: // Read operations

	std::string findRequiredAttribute(const std::string& name) const;
	std::string findOptionalAttribute(const std::string& name) const;
	SmartPtrCXmlElement findRequiredChild(const std::string& name) const;
	SmartPtrCXmlElement findOptionalChild(const std::string& name) const;
	SmartPtrCElementCollection findRequiredChildren(const std::string& name) const;
	SmartPtrCElementCollection findOptionalChildren(const std::string& name) const;
	SmartPtrCAttributeCollection getAllAttributes() const;
	SmartPtrCElementCollection getAllChildren() const;
	SmartPtrCOrderedElementCollection getAllChildrenInOrder() const;
	std::string getName() const;
	std::string getValue() const;
	std::string getCDataValue() const;
	std::string getPath() const;

public: // Write operations
	void addAttribute(const std::string& name, const std::string& value);
	void removeAttribute(const std::string& name);
	void setAttribute(const std::string& name, const std::string& value);
	SmartPtrCXmlElement createAndAddElement(const std::string& name);
	void addChild(const SmartPtrCXmlElement& xmlElement);
	void removeChild(const std::string& name);
	void setValue(const std::string& value);
	void setCDataValue(const std::string& value);
	void appendValue(const std::string& value);
	void saveToFile(const std::string& filename) const;
	std::string saveToString() const;
	std::string saveToStringRaw() const;

private:
	static const std::string CDATA_BEG;
	static const std::string CDATA_END;

private:
	bool _isInitialized;
	mutable MarkupParser::SmartPtrElement _element;
	std::string _path;

private:
	static void saveToString(const MarkupParser::SmartPtrElement& element, std::string& xml);

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CXmlElement);
};

CAF_DECLARE_SMART_POINTER(CXmlElement);

}

#endif /* _CXmlElement_h */
