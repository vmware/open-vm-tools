/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CParameterCollectionDoc_h_
#define CParameterCollectionDoc_h_


#include "Doc/CafCoreTypesDoc/CRequestInstanceParameterDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestParameterDoc.h"

namespace Caf {

/// A simple container for objects of type ParameterCollection
class CAFCORETYPESDOC_LINKAGE CParameterCollectionDoc {
public:
	CParameterCollectionDoc();
	virtual ~CParameterCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCRequestParameterDoc> parameter = std::deque<SmartPtrCRequestParameterDoc>(),
		const std::deque<SmartPtrCRequestInstanceParameterDoc> instanceParameter = std::deque<SmartPtrCRequestInstanceParameterDoc>());

public:
	/// Accessor for the Parameter
	std::deque<SmartPtrCRequestParameterDoc> getParameter() const;

	/// Accessor for the InstanceParameter
	std::deque<SmartPtrCRequestInstanceParameterDoc> getInstanceParameter() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCRequestParameterDoc> _parameter;
	std::deque<SmartPtrCRequestInstanceParameterDoc> _instanceParameter;

private:
	CAF_CM_DECLARE_NOCOPY(CParameterCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CParameterCollectionDoc);

}

#endif
