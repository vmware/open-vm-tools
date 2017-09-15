/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CPropertyCollectionDoc_h_
#define CPropertyCollectionDoc_h_


#include "Doc/CafCoreTypesDoc/CPropertyDoc.h"

namespace Caf {

/// A simple container for objects of type PropertyCollection
class CAFCORETYPESDOC_LINKAGE CPropertyCollectionDoc {
public:
	CPropertyCollectionDoc();
	virtual ~CPropertyCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCPropertyDoc> property = std::deque<SmartPtrCPropertyDoc>());

public:
	/// Accessor for the Property
	std::deque<SmartPtrCPropertyDoc> getProperty() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCPropertyDoc> _property;

private:
	CAF_CM_DECLARE_NOCOPY(CPropertyCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CPropertyCollectionDoc);

}

#endif
