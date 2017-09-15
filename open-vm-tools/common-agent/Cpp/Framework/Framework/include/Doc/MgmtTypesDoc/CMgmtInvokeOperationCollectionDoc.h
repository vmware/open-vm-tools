/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMgmtInvokeOperationCollectionDoc_h_
#define CMgmtInvokeOperationCollectionDoc_h_


#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationDoc.h"

namespace Caf {

/// A simple container for objects of type MgmtInvokeOperationCollection
class MGMTTYPESDOC_LINKAGE CMgmtInvokeOperationCollectionDoc {
public:
	CMgmtInvokeOperationCollectionDoc();
	virtual ~CMgmtInvokeOperationCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCMgmtInvokeOperationDoc> invokeOperationCollection);

public:
	/// Accessor for the InvokeOperation
	std::deque<SmartPtrCMgmtInvokeOperationDoc> getInvokeOperationCollection() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCMgmtInvokeOperationDoc> _invokeOperationCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CMgmtInvokeOperationCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CMgmtInvokeOperationCollectionDoc);

}

#endif
