/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMultiPmeMgmtBatchCollectionDoc_h_
#define CMultiPmeMgmtBatchCollectionDoc_h_


#include "Doc/MultiPmeMgmtRequestDoc/CMultiPmeMgmtBatchDoc.h"

namespace Caf {

/// A simple container for objects of type MultiPmeMgmtBatchCollection
class MULTIPMEMGMTREQUESTDOC_LINKAGE CMultiPmeMgmtBatchCollectionDoc {
public:
	CMultiPmeMgmtBatchCollectionDoc();
	virtual ~CMultiPmeMgmtBatchCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCMultiPmeMgmtBatchDoc> multiPmeBatch);

public:
	/// Accessor for the MultiPmeBatch
	std::deque<SmartPtrCMultiPmeMgmtBatchDoc> getMultiPmeBatch() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCMultiPmeMgmtBatchDoc> _multiPmeBatch;

private:
	CAF_CM_DECLARE_NOCOPY(CMultiPmeMgmtBatchCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CMultiPmeMgmtBatchCollectionDoc);

}

#endif
