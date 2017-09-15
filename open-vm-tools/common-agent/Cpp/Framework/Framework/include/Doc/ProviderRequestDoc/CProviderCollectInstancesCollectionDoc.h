/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderCollectInstancesCollectionDoc_h_
#define CProviderCollectInstancesCollectionDoc_h_


#include "Doc/ProviderRequestDoc/CProviderCollectInstancesDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderCollectInstancesCollection
class PROVIDERREQUESTDOC_LINKAGE CProviderCollectInstancesCollectionDoc {
public:
	CProviderCollectInstancesCollectionDoc();
	virtual ~CProviderCollectInstancesCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCProviderCollectInstancesDoc> collectInstances);

public:
	/// Accessor for the CollectInstances
	std::deque<SmartPtrCProviderCollectInstancesDoc> getCollectInstances() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCProviderCollectInstancesDoc> _collectInstances;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderCollectInstancesCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderCollectInstancesCollectionDoc);

}

#endif
