/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMultiPmeMgmtBatchDoc_h_
#define CMultiPmeMgmtBatchDoc_h_


#include "Doc/MgmtTypesDoc/CMgmtBatchDoc.h"
#include "Doc/MultiPmeMgmtRequestDoc/CPmeIdCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type MultiPmeMgmtBatch
class MULTIPMEMGMTREQUESTDOC_LINKAGE CMultiPmeMgmtBatchDoc {
public:
	CMultiPmeMgmtBatchDoc();
	virtual ~CMultiPmeMgmtBatchDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCPmeIdCollectionDoc pmeIdCollection,
		const SmartPtrCMgmtBatchDoc batch);

public:
	/// Accessor for the PmeIdCollection
	SmartPtrCPmeIdCollectionDoc getPmeIdCollection() const;

	/// Accessor for the Batch
	SmartPtrCMgmtBatchDoc getBatch() const;

private:
	bool _isInitialized;

	SmartPtrCPmeIdCollectionDoc _pmeIdCollection;
	SmartPtrCMgmtBatchDoc _batch;

private:
	CAF_CM_DECLARE_NOCOPY(CMultiPmeMgmtBatchDoc);
};

CAF_DECLARE_SMART_POINTER(CMultiPmeMgmtBatchDoc);

}

#endif
