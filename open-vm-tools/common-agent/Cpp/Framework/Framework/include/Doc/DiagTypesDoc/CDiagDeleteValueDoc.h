/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDiagDeleteValueDoc_h_
#define CDiagDeleteValueDoc_h_

namespace Caf {

/// A simple container for objects of type DiagDeleteValue
class DIAGTYPESDOC_LINKAGE CDiagDeleteValueDoc {
public:
	CDiagDeleteValueDoc();
	virtual ~CDiagDeleteValueDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID jobId,
		const std::string fileAlias,
		const std::string valueName);

public:
	/// Accessor for the JobId
	UUID getJobId() const;

	/// Accessor for the FileAlias
	std::string getFileAlias() const;

	/// Accessor for the ValueName
	std::string getValueName() const;

private:
	UUID _jobId;
	std::string _fileAlias;
	std::string _valueName;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CDiagDeleteValueDoc);
};

CAF_DECLARE_SMART_POINTER(CDiagDeleteValueDoc);

}

#endif
