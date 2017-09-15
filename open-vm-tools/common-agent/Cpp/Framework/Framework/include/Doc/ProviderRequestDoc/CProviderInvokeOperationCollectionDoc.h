/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderInvokeOperationCollectionDoc_h_
#define CProviderInvokeOperationCollectionDoc_h_


#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderInvokeOperationCollection
class PROVIDERREQUESTDOC_LINKAGE CProviderInvokeOperationCollectionDoc {
public:
	CProviderInvokeOperationCollectionDoc();
	virtual ~CProviderInvokeOperationCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCProviderInvokeOperationDoc> invokeOperation);

public:
	/// Accessor for the InvokeOperation
	std::deque<SmartPtrCProviderInvokeOperationDoc> getInvokeOperation() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCProviderInvokeOperationDoc> _invokeOperation;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderInvokeOperationCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderInvokeOperationCollectionDoc);

}

#endif
