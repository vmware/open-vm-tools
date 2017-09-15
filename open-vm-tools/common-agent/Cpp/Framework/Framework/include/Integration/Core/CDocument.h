/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CDocument_h_
#define CDocument_h_


#include "Integration/IDocument.h"

#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CDocument :
	public IDocument {
public:
	CDocument();
	virtual ~CDocument();

public:
	void initialize(const SmartPtrCXmlElement& xmlElement);
	SmartPtrCXmlElement getXmlElement() const;

public: // IDocument
	std::string findRequiredAttribute(const std::string& name) const;
	std::string findOptionalAttribute(const std::string& name) const;
	SmartPtrIDocument findRequiredChild(const std::string& name) const;
	SmartPtrIDocument findOptionalChild(const std::string& name) const;
	SmartPtrCAttributeCollection getAllAttributes() const;
	SmartPtrCChildCollection getAllChildren() const;
	SmartPtrCOrderedChildCollection getAllChildrenInOrder() const;
	std::string getName() const;
	std::string getValue() const;
	std::string getPath() const;

public: // IDocument Save operations
	void saveToFile(const std::string& filename) const;
	std::string saveToString() const;
	std::string saveToStringRaw() const;

private:
	bool _isInitialized;
	SmartPtrCXmlElement _xmlElement;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CDocument);
};

CAF_DECLARE_SMART_POINTER(CDocument);
}

#endif // #ifndef CDocument_h_
