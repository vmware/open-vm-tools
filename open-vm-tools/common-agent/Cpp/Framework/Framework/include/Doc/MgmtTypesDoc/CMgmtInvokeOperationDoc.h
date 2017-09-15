/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMgmtInvokeOperationDoc_h_
#define CMgmtInvokeOperationDoc_h_


#include "Doc/CafCoreTypesDoc/CClassSpecifierDoc.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"

namespace Caf {

/// A simple container for objects of type MgmtInvokeOperation
class MGMTTYPESDOC_LINKAGE CMgmtInvokeOperationDoc {
public:
	CMgmtInvokeOperationDoc();
	virtual ~CMgmtInvokeOperationDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID jobId,
		const SmartPtrCClassSpecifierDoc classSpecifier,
		const SmartPtrCOperationDoc operation);

public:
	/// Accessor for the JobId
	UUID getJobId() const;

	/// Accessor for the ClassSpecifier
	SmartPtrCClassSpecifierDoc getClassSpecifier() const;

	/// Accessor for the Operation
	SmartPtrCOperationDoc getOperation() const;

private:
	UUID _jobId;
	SmartPtrCClassSpecifierDoc _classSpecifier;
	SmartPtrCOperationDoc _operation;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CMgmtInvokeOperationDoc);
};

CAF_DECLARE_SMART_POINTER(CMgmtInvokeOperationDoc);

}

#endif
