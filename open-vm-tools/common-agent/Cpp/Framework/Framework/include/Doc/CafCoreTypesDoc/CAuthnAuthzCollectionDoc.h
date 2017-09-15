/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CAuthnAuthzCollectionDoc_h_
#define CAuthnAuthzCollectionDoc_h_


#include "Doc/CafCoreTypesDoc/CAuthnAuthzDoc.h"

namespace Caf {

/// Set of logging levels for different components
class CAFCORETYPESDOC_LINKAGE CAuthnAuthzCollectionDoc {
public:
	CAuthnAuthzCollectionDoc();
	virtual ~CAuthnAuthzCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCAuthnAuthzDoc> authnAuthz = std::deque<SmartPtrCAuthnAuthzDoc>());

public:
	/// Used to change the logging level for a specific component
	std::deque<SmartPtrCAuthnAuthzDoc> getAuthnAuthz() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCAuthnAuthzDoc> _authnAuthz;

private:
	CAF_CM_DECLARE_NOCOPY(CAuthnAuthzCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CAuthnAuthzCollectionDoc);

}

#endif
