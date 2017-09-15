/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderBatchDoc_h_
#define CProviderBatchDoc_h_


#include "Doc/ProviderRequestDoc/CProviderCollectInstancesCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderBatch
class PROVIDERREQUESTDOC_LINKAGE CProviderBatchDoc {
public:
	CProviderBatchDoc();
	virtual ~CProviderBatchDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string outputDir,
		const SmartPtrCProviderCollectInstancesCollectionDoc collectInstancesCollection,
		const SmartPtrCProviderInvokeOperationCollectionDoc invokeOperationCollection);

public:
	/// Accessor for the OutputDir
	std::string getOutputDir() const;

	/// Accessor for the CollectInstancesCollection
	SmartPtrCProviderCollectInstancesCollectionDoc getCollectInstancesCollection() const;

	/// Accessor for the InvokeOperationCollection
	SmartPtrCProviderInvokeOperationCollectionDoc getInvokeOperationCollection() const;

private:
	bool _isInitialized;

	std::string _outputDir;
	SmartPtrCProviderCollectInstancesCollectionDoc _collectInstancesCollection;
	SmartPtrCProviderInvokeOperationCollectionDoc _invokeOperationCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderBatchDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderBatchDoc);

}

#endif
