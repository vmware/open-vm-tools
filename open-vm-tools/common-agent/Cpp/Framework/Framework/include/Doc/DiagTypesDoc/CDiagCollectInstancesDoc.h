/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDiagCollectInstancesDoc_h_
#define CDiagCollectInstancesDoc_h_

namespace Caf {

/// A simple container for objects of type DiagCollectInstances
class DIAGTYPESDOC_LINKAGE CDiagCollectInstancesDoc {
public:
	CDiagCollectInstancesDoc();
	virtual ~CDiagCollectInstancesDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID jobId);

public:
	/// Accessor for the JobId
	UUID getJobId() const;

private:
	UUID _jobId;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CDiagCollectInstancesDoc);
};

CAF_DECLARE_SMART_POINTER(CDiagCollectInstancesDoc);

}

#endif
