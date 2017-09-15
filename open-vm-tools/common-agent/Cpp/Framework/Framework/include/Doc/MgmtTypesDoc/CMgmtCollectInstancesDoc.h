/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMgmtCollectInstancesDoc_h_
#define CMgmtCollectInstancesDoc_h_


#include "Doc/CafCoreTypesDoc/CClassSpecifierDoc.h"
#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type MgmtCollectInstances
class MGMTTYPESDOC_LINKAGE CMgmtCollectInstancesDoc {
public:
	CMgmtCollectInstancesDoc();
	virtual ~CMgmtCollectInstancesDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID jobId,
		const SmartPtrCClassSpecifierDoc classSpecifier,
		const SmartPtrCParameterCollectionDoc parameterCollection);

public:
	/// Accessor for the JobId
	UUID getJobId() const;

	/// Accessor for the ClassSpecifier
	SmartPtrCClassSpecifierDoc getClassSpecifier() const;

	/// Accessor for the ParameterCollection
	SmartPtrCParameterCollectionDoc getParameterCollection() const;

private:
	UUID _jobId;
	SmartPtrCClassSpecifierDoc _classSpecifier;
	SmartPtrCParameterCollectionDoc _parameterCollection;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CMgmtCollectInstancesDoc);
};

CAF_DECLARE_SMART_POINTER(CMgmtCollectInstancesDoc);

}

#endif
