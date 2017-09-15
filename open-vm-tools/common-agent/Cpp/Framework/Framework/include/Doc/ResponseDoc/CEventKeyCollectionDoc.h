/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CEventKeyCollectionDoc_h_
#define CEventKeyCollectionDoc_h_


#include "Doc/ResponseDoc/CEventKeyDoc.h"

namespace Caf {

/// A simple container for objects of type EventKeyCollection
class RESPONSEDOC_LINKAGE CEventKeyCollectionDoc {
public:
	CEventKeyCollectionDoc();
	virtual ~CEventKeyCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCEventKeyDoc> eventKey);

public:
	/// Accessor for the EventKey
	std::deque<SmartPtrCEventKeyDoc> getEventKey() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCEventKeyDoc> _eventKey;

private:
	CAF_CM_DECLARE_NOCOPY(CEventKeyCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CEventKeyCollectionDoc);

}

#endif
