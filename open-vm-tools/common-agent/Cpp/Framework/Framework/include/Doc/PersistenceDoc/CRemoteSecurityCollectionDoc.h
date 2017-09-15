/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CRemoteSecurityCollectionDoc_h_
#define CRemoteSecurityCollectionDoc_h_


#include "Doc/PersistenceDoc/CRemoteSecurityDoc.h"

namespace Caf {

/// A simple container for objects of type RemoteSecurityCollection
class PERSISTENCEDOC_LINKAGE CRemoteSecurityCollectionDoc {
public:
	CRemoteSecurityCollectionDoc();
	virtual ~CRemoteSecurityCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCRemoteSecurityDoc> remoteSecurity = std::deque<SmartPtrCRemoteSecurityDoc>());

public:
	/// Accessor for the RemoteSecurity
	std::deque<SmartPtrCRemoteSecurityDoc> getRemoteSecurity() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCRemoteSecurityDoc> _remoteSecurity;

private:
	CAF_CM_DECLARE_NOCOPY(CRemoteSecurityCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CRemoteSecurityCollectionDoc);

}

#endif
