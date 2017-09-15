/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CPropertyDoc_h_
#define CPropertyDoc_h_

#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"

namespace Caf {

/// A simple container for objects of type Property
class CAFCORETYPESDOC_LINKAGE CPropertyDoc {
public:
	CPropertyDoc();
	virtual ~CPropertyDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const PROPERTY_TYPE type,
		const std::deque<std::string> value = std::deque<std::string>());

public:
	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Type
	PROPERTY_TYPE getType() const;

	/// Accessor for the Value
	std::deque<std::string> getValue() const;

private:
	PROPERTY_TYPE _type;

	bool _isInitialized;
	std::string _name;
	std::deque<std::string> _value;

private:
	CAF_CM_DECLARE_NOCOPY(CPropertyDoc);
};

CAF_DECLARE_SMART_POINTER(CPropertyDoc);

}

#endif
