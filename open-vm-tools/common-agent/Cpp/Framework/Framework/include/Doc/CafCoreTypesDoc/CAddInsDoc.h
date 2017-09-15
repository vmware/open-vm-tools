/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CAddInsDoc_h_
#define CAddInsDoc_h_


#include "Doc/CafCoreTypesDoc/CAddInCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type AddIns
class CAFCORETYPESDOC_LINKAGE CAddInsDoc {
public:
	CAddInsDoc();
	virtual ~CAddInsDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCAddInCollectionDoc> addInCollection = std::deque<SmartPtrCAddInCollectionDoc>());

public:
	/// Accessor for the AddInCollection
	std::deque<SmartPtrCAddInCollectionDoc> getAddInCollection() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCAddInCollectionDoc> _addInCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CAddInsDoc);
};

CAF_DECLARE_SMART_POINTER(CAddInsDoc);

}

#endif
