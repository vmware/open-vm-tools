/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CRequestConfigDoc_h_
#define CRequestConfigDoc_h_


#include "Doc/CafCoreTypesDoc/CAddInsDoc.h"
#include "Doc/CafCoreTypesDoc/CLoggingLevelCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type RequestConfig
class CAFCORETYPESDOC_LINKAGE CRequestConfigDoc {
public:
	CRequestConfigDoc();
	virtual ~CRequestConfigDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string responseFormatType,
		const SmartPtrCAddInsDoc requestProcessorAddIns = SmartPtrCAddInsDoc(),
		const SmartPtrCAddInsDoc responseProcessorAddIns = SmartPtrCAddInsDoc(),
		const SmartPtrCLoggingLevelCollectionDoc loggingLevelCollection = SmartPtrCLoggingLevelCollectionDoc());

public:
	/// Accessor for the ResponseFormatType
	std::string getResponseFormatType() const;

	/// Accessor for the RequestProcessorAddIns
	SmartPtrCAddInsDoc getRequestProcessorAddIns() const;

	/// Accessor for the ResponseProcessorAddIns
	SmartPtrCAddInsDoc getResponseProcessorAddIns() const;

	/// Accessor for the LoggingLevelCollection
	SmartPtrCLoggingLevelCollectionDoc getLoggingLevelCollection() const;

private:
	bool _isInitialized;

	std::string _responseFormatType;
	SmartPtrCAddInsDoc _requestProcessorAddIns;
	SmartPtrCAddInsDoc _responseProcessorAddIns;
	SmartPtrCLoggingLevelCollectionDoc _loggingLevelCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CRequestConfigDoc);
};

CAF_DECLARE_SMART_POINTER(CRequestConfigDoc);

}

#endif
