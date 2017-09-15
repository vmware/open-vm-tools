/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef COperationDoc_h_
#define COperationDoc_h_


#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type Operation
class CAFCORETYPESDOC_LINKAGE COperationDoc {
public:
	COperationDoc();
	virtual ~COperationDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const SmartPtrCParameterCollectionDoc parameterCollection = SmartPtrCParameterCollectionDoc());

public:
	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the ParameterCollection
	SmartPtrCParameterCollectionDoc getParameterCollection() const;

private:
	bool _isInitialized;

	std::string _name;
	SmartPtrCParameterCollectionDoc _parameterCollection;

private:
	CAF_CM_DECLARE_NOCOPY(COperationDoc);
};

CAF_DECLARE_SMART_POINTER(COperationDoc);

}

#endif
