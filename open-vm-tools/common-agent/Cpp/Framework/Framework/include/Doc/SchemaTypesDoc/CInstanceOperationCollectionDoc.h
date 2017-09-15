/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CInstanceOperationCollectionDoc_h_
#define CInstanceOperationCollectionDoc_h_


#include "Doc/SchemaTypesDoc/CInstanceOperationDoc.h"

namespace Caf {

/// A simple container for objects of type InstanceOperationCollection
class SCHEMATYPESDOC_LINKAGE CInstanceOperationCollectionDoc {
public:
	CInstanceOperationCollectionDoc();
	virtual ~CInstanceOperationCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCInstanceOperationDoc> instanceOperationCollection);

public:
	/// Accessor for the InstanceOperation
	std::deque<SmartPtrCInstanceOperationDoc> getInstanceOperationCollection() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCInstanceOperationDoc> _instanceOperationCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CInstanceOperationCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CInstanceOperationCollectionDoc);

}

#endif
