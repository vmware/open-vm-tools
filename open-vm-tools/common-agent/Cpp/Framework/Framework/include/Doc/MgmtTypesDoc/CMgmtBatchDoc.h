/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMgmtBatchDoc_h_
#define CMgmtBatchDoc_h_


#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectSchemaDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type MgmtBatch
class MGMTTYPESDOC_LINKAGE CMgmtBatchDoc {
public:
	CMgmtBatchDoc();
	virtual ~CMgmtBatchDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCMgmtCollectSchemaDoc collectSchema,
		const SmartPtrCMgmtCollectInstancesCollectionDoc collectInstancesCollection,
		const SmartPtrCMgmtInvokeOperationCollectionDoc invokeOperationCollection);

public:
	/// Accessor for the CollectSchema
	SmartPtrCMgmtCollectSchemaDoc getCollectSchema() const;

	/// Accessor for the CollectInstancesCollection
	SmartPtrCMgmtCollectInstancesCollectionDoc getCollectInstancesCollection() const;

	/// Accessor for the InvokeOperationCollection
	SmartPtrCMgmtInvokeOperationCollectionDoc getInvokeOperationCollection() const;

private:
	bool _isInitialized;

	SmartPtrCMgmtCollectSchemaDoc _collectSchema;
	SmartPtrCMgmtCollectInstancesCollectionDoc _collectInstancesCollection;
	SmartPtrCMgmtInvokeOperationCollectionDoc _invokeOperationCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CMgmtBatchDoc);
};

CAF_DECLARE_SMART_POINTER(CMgmtBatchDoc);

}

#endif
