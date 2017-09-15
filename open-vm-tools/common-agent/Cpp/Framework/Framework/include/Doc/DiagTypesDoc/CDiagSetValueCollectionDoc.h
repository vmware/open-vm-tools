/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDiagSetValueCollectionDoc_h_
#define CDiagSetValueCollectionDoc_h_


#include "Doc/DiagTypesDoc/CDiagSetValueDoc.h"

namespace Caf {

/// A simple container for objects of type DiagSetValueCollection
class DIAGTYPESDOC_LINKAGE CDiagSetValueCollectionDoc {
public:
	CDiagSetValueCollectionDoc();
	virtual ~CDiagSetValueCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCDiagSetValueDoc> setValueCollection);

public:
	/// Accessor for the SetValue
	std::deque<SmartPtrCDiagSetValueDoc> getSetValueCollection() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCDiagSetValueDoc> _setValueCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CDiagSetValueCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CDiagSetValueCollectionDoc);

}

#endif
