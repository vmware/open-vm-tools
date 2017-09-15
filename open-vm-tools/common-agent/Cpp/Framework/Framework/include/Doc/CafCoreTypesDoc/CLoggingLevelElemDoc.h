/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CLoggingLevelElemDoc_h_
#define CLoggingLevelElemDoc_h_

#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"

namespace Caf {

/// Logging level for a component
class CAFCORETYPESDOC_LINKAGE CLoggingLevelElemDoc {
public:
	CLoggingLevelElemDoc();
	virtual ~CLoggingLevelElemDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const LOGGINGCOMPONENT_TYPE component = LOGGINGCOMPONENT_NONE,
		const LOGGINGLEVEL_TYPE level = LOGGINGLEVEL_NONE);

public:
	/// The logging level applies to this component
	LOGGINGCOMPONENT_TYPE getComponent() const;

	/// Set the logging level to this value
	LOGGINGLEVEL_TYPE getLevel() const;

private:
	LOGGINGCOMPONENT_TYPE _component;
	LOGGINGLEVEL_TYPE _level;

	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CLoggingLevelElemDoc);
};

CAF_DECLARE_SMART_POINTER(CLoggingLevelElemDoc);

}

#endif
