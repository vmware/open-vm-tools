/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CPersistenceProtocolCollectionDoc_h_
#define CPersistenceProtocolCollectionDoc_h_


#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"

namespace Caf {

/// A simple container for objects of type PersistenceProtocolCollection
class PERSISTENCEDOC_LINKAGE CPersistenceProtocolCollectionDoc {
public:
	CPersistenceProtocolCollectionDoc();
	virtual ~CPersistenceProtocolCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocol = std::deque<SmartPtrCPersistenceProtocolDoc>());

public:
	/// Accessor for the PersistenceProtocol
	std::deque<SmartPtrCPersistenceProtocolDoc> getPersistenceProtocol() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCPersistenceProtocolDoc> _persistenceProtocol;

private:
	CAF_CM_DECLARE_NOCOPY(CPersistenceProtocolCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CPersistenceProtocolCollectionDoc);

}

#endif
