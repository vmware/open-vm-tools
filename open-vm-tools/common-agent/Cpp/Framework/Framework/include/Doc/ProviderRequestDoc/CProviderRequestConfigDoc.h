/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderRequestConfigDoc_h_
#define CProviderRequestConfigDoc_h_


#include "Doc/CafCoreTypesDoc/CLoggingLevelCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderRequestConfig
class PROVIDERREQUESTDOC_LINKAGE CProviderRequestConfigDoc {
public:
	CProviderRequestConfigDoc();
	virtual ~CProviderRequestConfigDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string responseFormatType,
		const SmartPtrCLoggingLevelCollectionDoc loggingLevelCollection);

public:
	/// Accessor for the ResponseFormatType
	std::string getResponseFormatType() const;

	/// Accessor for the LoggingLevelCollection
	SmartPtrCLoggingLevelCollectionDoc getLoggingLevelCollection() const;

private:
	bool _isInitialized;

	std::string _responseFormatType;
	SmartPtrCLoggingLevelCollectionDoc _loggingLevelCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderRequestConfigDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderRequestConfigDoc);

}

#endif
