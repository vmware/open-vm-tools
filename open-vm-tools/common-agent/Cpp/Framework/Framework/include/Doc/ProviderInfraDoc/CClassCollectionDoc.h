/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CClassCollectionDoc_h_
#define CClassCollectionDoc_h_


#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"

namespace Caf {

/// A simple container for objects of type ClassCollection
class PROVIDERINFRADOC_LINKAGE CClassCollectionDoc {
public:
	CClassCollectionDoc();
	virtual ~CClassCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCFullyQualifiedClassGroupDoc> fullyQualifiedClass);

public:
	/// Accessor for the FullyQualifiedClass
	std::deque<SmartPtrCFullyQualifiedClassGroupDoc> getFullyQualifiedClass() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCFullyQualifiedClassGroupDoc> _fullyQualifiedClass;

private:
	CAF_CM_DECLARE_NOCOPY(CClassCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CClassCollectionDoc);

}

#endif
