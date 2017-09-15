/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CClassInstancePropertyDoc_h_
#define CClassInstancePropertyDoc_h_


#include "Doc/SchemaTypesDoc/CClassIdentifierDoc.h"

namespace Caf {

/// Definition of an attribute (field) of a class
class SCHEMATYPESDOC_LINKAGE CClassInstancePropertyDoc {
public:
	CClassInstancePropertyDoc();
	virtual ~CClassInstancePropertyDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const std::deque<SmartPtrCClassIdentifierDoc> type,
		const bool required = false,
		const bool transientVal = false,
		const bool list = false,
		const std::string displayName = std::string(),
		const std::string description = std::string());

public:
	/// Property name
	std::string getName() const;

	/// Accessor for the Type
	std::deque<SmartPtrCClassIdentifierDoc> getType() const;

	/// Whether this is a required property, i.e. this property must always be non-empty
	bool getRequired() const;

	/// Accessor for the TransientVal
	bool getTransientVal() const;

	/// Indicates whether to expect a list of properties in the provider response
	bool getList() const;

	/// A hint as to what this property should be called when displaying it to a human
	std::string getDisplayName() const;

	/// A phrase to describe the property for mouse-over text, etc
	std::string getDescription() const;

private:
	std::string _name;
	std::deque<SmartPtrCClassIdentifierDoc> _type;
	bool _required;
	bool _transientVal;
	bool _list;
	std::string _displayName;
	std::string _description;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CClassInstancePropertyDoc);
};

CAF_DECLARE_SMART_POINTER(CClassInstancePropertyDoc);

}

#endif
