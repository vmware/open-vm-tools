/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDiagSetValueDoc_h_
#define CDiagSetValueDoc_h_


#include "Doc/CafCoreTypesDoc/CPropertyDoc.h"

namespace Caf {

/// A simple container for objects of type DiagSetValue
class DIAGTYPESDOC_LINKAGE CDiagSetValueDoc {
public:
	CDiagSetValueDoc();
	virtual ~CDiagSetValueDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID jobId,
		const std::string fileAlias,
		const SmartPtrCPropertyDoc value);

public:
	/// Accessor for the JobId
	UUID getJobId() const;

	/// Accessor for the FileAlias
	std::string getFileAlias() const;

	/// Accessor for the Value
	SmartPtrCPropertyDoc getValue() const;

private:
	UUID _jobId;
	std::string _fileAlias;
	SmartPtrCPropertyDoc _value;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CDiagSetValueDoc);
};

CAF_DECLARE_SMART_POINTER(CDiagSetValueDoc);

}

#endif
