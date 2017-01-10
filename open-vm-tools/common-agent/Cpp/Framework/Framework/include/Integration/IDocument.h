/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IDocument_h_
#define _IntegrationContracts_IDocument_h_

namespace Caf {

CAF_FORWARD_DECLARE_SMART_INTERFACE(IDocument);

/// TODO - describe interface
struct __declspec(novtable)
	IDocument : public ICafObject
{
	CAF_DECL_UUID("aa95ea11-3ca0-4863-b267-88d38246ff67")

public: // Read operations
	typedef std::map<std::string, std::string> CAttributeCollection;
	typedef std::multimap<std::string, SmartPtrIDocument> CChildCollection;
	typedef std::deque<SmartPtrIDocument> COrderedChildCollection;
	CAF_DECLARE_SMART_POINTER(CAttributeCollection);
	CAF_DECLARE_SMART_POINTER(CChildCollection);
	CAF_DECLARE_SMART_POINTER(COrderedChildCollection);

public: // Read operations
	virtual std::string findRequiredAttribute(const std::string& name) const = 0;
	virtual std::string findOptionalAttribute(const std::string& name) const = 0;
	virtual SmartPtrIDocument findRequiredChild(const std::string& name) const = 0;
	virtual SmartPtrIDocument findOptionalChild(const std::string& name) const = 0;
	virtual SmartPtrCAttributeCollection getAllAttributes() const = 0;
	virtual SmartPtrCChildCollection getAllChildren() const = 0;
	virtual SmartPtrCOrderedChildCollection getAllChildrenInOrder() const = 0;
	virtual std::string getName() const = 0;
	virtual std::string getValue() const = 0;
	virtual std::string getPath() const = 0;

public: // Save operations
	virtual void saveToFile(const std::string& filename) const = 0;
	virtual std::string saveToString() const = 0;
	virtual std::string saveToStringRaw() const = 0;
};

}

#endif // #ifndef _IntegrationContracts_IDocument_h_

