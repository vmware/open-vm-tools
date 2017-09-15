/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CAddInCollectionDoc_h_
#define CAddInCollectionDoc_h_

namespace Caf {

/// A simple container for objects of type AddInCollection
class CAFCORETYPESDOC_LINKAGE CAddInCollectionDoc {
public:
	CAddInCollectionDoc();
	virtual ~CAddInCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<std::string> addInCollection = std::deque<std::string>());

public:
	/// Accessor for the AddIn
	std::deque<std::string> getAddInCollection() const;

private:
	bool _isInitialized;

	std::deque<std::string> _addInCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CAddInCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CAddInCollectionDoc);

}

#endif
