/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CRequestParameterDoc_h_
#define CRequestParameterDoc_h_

#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"

namespace Caf {

/// A simple container for objects of type RequestParameter
class CAFCORETYPESDOC_LINKAGE CRequestParameterDoc {
public:
	CRequestParameterDoc();
	virtual ~CRequestParameterDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const PARAMETER_TYPE type,
		const std::deque<std::string> value);

public:
	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Type
	PARAMETER_TYPE getType() const;

	/// Accessor for the Value
	std::deque<std::string> getValue() const;

private:
	PARAMETER_TYPE _type;

	bool _isInitialized;
	std::string _name;
	std::deque<std::string> _value;

private:
	CAF_CM_DECLARE_NOCOPY(CRequestParameterDoc);
};

CAF_DECLARE_SMART_POINTER(CRequestParameterDoc);

}

#endif
