/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDiagBatchDoc_h_
#define CDiagBatchDoc_h_


#include "Doc/DiagTypesDoc/CDiagCollectInstancesDoc.h"
#include "Doc/DiagTypesDoc/CDiagDeleteValueCollectionDoc.h"
#include "Doc/DiagTypesDoc/CDiagSetValueCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type DiagBatch
class DIAGTYPESDOC_LINKAGE CDiagBatchDoc {
public:
	CDiagBatchDoc();
	virtual ~CDiagBatchDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCDiagCollectInstancesDoc collectInstances,
		const SmartPtrCDiagSetValueCollectionDoc setValueCollection,
		const SmartPtrCDiagDeleteValueCollectionDoc deleteValueCollection);

public:
	/// Accessor for the CollectInstances
	SmartPtrCDiagCollectInstancesDoc getCollectInstances() const;

	/// Accessor for the SetValueCollection
	SmartPtrCDiagSetValueCollectionDoc getSetValueCollection() const;

	/// Accessor for the DeleteValueCollection
	SmartPtrCDiagDeleteValueCollectionDoc getDeleteValueCollection() const;

private:
	bool _isInitialized;

	SmartPtrCDiagCollectInstancesDoc _collectInstances;
	SmartPtrCDiagSetValueCollectionDoc _setValueCollection;
	SmartPtrCDiagDeleteValueCollectionDoc _deleteValueCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CDiagBatchDoc);
};

CAF_DECLARE_SMART_POINTER(CDiagBatchDoc);

}

#endif
