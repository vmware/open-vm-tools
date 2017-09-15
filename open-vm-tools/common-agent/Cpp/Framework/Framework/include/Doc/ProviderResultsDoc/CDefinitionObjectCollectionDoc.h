/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDefinitionObjectCollectionDoc_h_
#define CDefinitionObjectCollectionDoc_h_

namespace Caf {

/// Set of elements containing data returned as a result of a provider collection or action
class PROVIDERRESULTSDOC_LINKAGE CDefinitionObjectCollectionDoc {
public:
	CDefinitionObjectCollectionDoc();
	virtual ~CDefinitionObjectCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<std::string> value);

public:
	/// Accessor for the Value
	std::deque<std::string> getValue() const;

private:
	bool _isInitialized;

	std::deque<std::string> _value;

private:
	CAF_CM_DECLARE_NOCOPY(CDefinitionObjectCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CDefinitionObjectCollectionDoc);

}

#endif
