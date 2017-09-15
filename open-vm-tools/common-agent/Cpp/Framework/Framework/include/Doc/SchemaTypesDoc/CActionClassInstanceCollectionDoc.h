/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 *
 */

#ifndef CActionClassInstanceCollectionDoc_h_
#define CActionClassInstanceCollectionDoc_h_

namespace Caf {

/// A simple container for objects of type ActionClassInstanceCollection
class SCHEMATYPESDOC_LINKAGE CActionClassInstanceCollectionDoc {
public:
	CActionClassInstanceCollectionDoc();
	virtual ~CActionClassInstanceCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCActionClassInstanceDoc> actionClassInstanceCollection);

public:
	/// Accessor for the ActionClassInstance
	std::deque<SmartPtrCActionClassInstanceDoc> getActionClassInstanceCollection() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCActionClassInstanceDoc> _actionClassInstanceCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CActionClassInstanceCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CActionClassInstanceCollectionDoc);

}

#endif
