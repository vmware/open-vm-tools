/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CInstanceOperationDoc_h_
#define CInstanceOperationDoc_h_

namespace Caf {

/// A simple container for objects of type InstanceOperation
class SCHEMATYPESDOC_LINKAGE CInstanceOperationDoc {
public:
	CInstanceOperationDoc();
	virtual ~CInstanceOperationDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string operationName,
		const std::string moniker);

public:
	/// Accessor for the OperationName
	std::string getOperationName() const;

	/// Accessor for the Moniker
	std::string getMoniker() const;

private:
	bool _isInitialized;

	std::string _operationName;
	std::string _moniker;

private:
	CAF_CM_DECLARE_NOCOPY(CInstanceOperationDoc);
};

CAF_DECLARE_SMART_POINTER(CInstanceOperationDoc);

}

#endif
