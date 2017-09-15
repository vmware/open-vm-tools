/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDiagDeleteValueCollectionDoc_h_
#define CDiagDeleteValueCollectionDoc_h_


#include "Doc/DiagTypesDoc/CDiagDeleteValueDoc.h"

namespace Caf {

/// A simple container for objects of type DiagDeleteValueCollection
class DIAGTYPESDOC_LINKAGE CDiagDeleteValueCollectionDoc {
public:
	CDiagDeleteValueCollectionDoc();
	virtual ~CDiagDeleteValueCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCDiagDeleteValueDoc> deleteValueCollection);

public:
	/// Accessor for the DeleteValue
	std::deque<SmartPtrCDiagDeleteValueDoc> getDeleteValueCollection() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCDiagDeleteValueDoc> _deleteValueCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CDiagDeleteValueCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CDiagDeleteValueCollectionDoc);

}

#endif
