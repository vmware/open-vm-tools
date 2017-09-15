/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProtocolCollectionDoc_h_
#define CProtocolCollectionDoc_h_


#include "Doc/CafCoreTypesDoc/CProtocolDoc.h"

namespace Caf {

/// Set of protocol
class CAFCORETYPESDOC_LINKAGE CProtocolCollectionDoc {
public:
	CProtocolCollectionDoc();
	virtual ~CProtocolCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCProtocolDoc> protocol = std::deque<SmartPtrCProtocolDoc>());

public:
	/// Used to change the logging level for a specific component
	std::deque<SmartPtrCProtocolDoc> getProtocol() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCProtocolDoc> _protocol;

private:
	CAF_CM_DECLARE_NOCOPY(CProtocolCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CProtocolCollectionDoc);

}

#endif
