/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CClassSpecifierDoc_h_
#define CClassSpecifierDoc_h_


#include "Doc/CafCoreTypesDoc/CClassFiltersDoc.h"
#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"

namespace Caf {

/// A simple container for objects of type ClassSpecifier
class CAFCORETYPESDOC_LINKAGE CClassSpecifierDoc {
public:
	CClassSpecifierDoc();
	virtual ~CClassSpecifierDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCFullyQualifiedClassGroupDoc fullyQualifiedClass = SmartPtrCFullyQualifiedClassGroupDoc(),
		const SmartPtrCClassFiltersDoc classFilters = SmartPtrCClassFiltersDoc());

public:
	/// Accessor for the FullyQualifiedClass
	SmartPtrCFullyQualifiedClassGroupDoc getFullyQualifiedClass() const;

	/// Accessor for the ClassFilters
	SmartPtrCClassFiltersDoc getClassFilters() const;

private:
	bool _isInitialized;

	SmartPtrCFullyQualifiedClassGroupDoc _fullyQualifiedClass;
	SmartPtrCClassFiltersDoc _classFilters;

private:
	CAF_CM_DECLARE_NOCOPY(CClassSpecifierDoc);
};

CAF_DECLARE_SMART_POINTER(CClassSpecifierDoc);

}

#endif
