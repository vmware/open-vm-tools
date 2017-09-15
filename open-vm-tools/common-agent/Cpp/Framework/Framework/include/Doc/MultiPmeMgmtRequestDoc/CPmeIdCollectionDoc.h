/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CPmeIdCollectionDoc_h_
#define CPmeIdCollectionDoc_h_

namespace Caf {

/// A simple container for objects of type PmeIdCollection
class MULTIPMEMGMTREQUESTDOC_LINKAGE CPmeIdCollectionDoc {
public:
	CPmeIdCollectionDoc();
	virtual ~CPmeIdCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<std::string> pmeIdCollection);

public:
	/// Accessor for the PmeId
	std::deque<std::string> getPmeIdCollection() const;

private:
	bool _isInitialized;

	std::deque<std::string> _pmeIdCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CPmeIdCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CPmeIdCollectionDoc);

}

#endif
