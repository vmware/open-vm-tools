/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CLoggingLevelCollectionDoc_h_
#define CLoggingLevelCollectionDoc_h_


#include "Doc/CafCoreTypesDoc/CLoggingLevelElemDoc.h"

namespace Caf {

/// Set of logging levels for different components
class CAFCORETYPESDOC_LINKAGE CLoggingLevelCollectionDoc {
public:
	CLoggingLevelCollectionDoc();
	virtual ~CLoggingLevelCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCLoggingLevelElemDoc> loggingLevel = std::deque<SmartPtrCLoggingLevelElemDoc>());

public:
	/// Used to change the logging level for a specific component
	std::deque<SmartPtrCLoggingLevelElemDoc> getLoggingLevel() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCLoggingLevelElemDoc> _loggingLevel;

private:
	CAF_CM_DECLARE_NOCOPY(CLoggingLevelCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CLoggingLevelCollectionDoc);

}

#endif
