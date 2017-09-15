/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMgmtCollectInstancesCollectionDoc_h_
#define CMgmtCollectInstancesCollectionDoc_h_


#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesDoc.h"

namespace Caf {

/// A simple container for objects of type MgmtCollectInstancesCollection
class MGMTTYPESDOC_LINKAGE CMgmtCollectInstancesCollectionDoc {
public:
	CMgmtCollectInstancesCollectionDoc();
	virtual ~CMgmtCollectInstancesCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCMgmtCollectInstancesDoc> collectInstancesCollection);

public:
	/// Accessor for the CollectInstances
	std::deque<SmartPtrCMgmtCollectInstancesDoc> getCollectInstancesCollection() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCMgmtCollectInstancesDoc> _collectInstancesCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CMgmtCollectInstancesCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CMgmtCollectInstancesCollectionDoc);

}

#endif
