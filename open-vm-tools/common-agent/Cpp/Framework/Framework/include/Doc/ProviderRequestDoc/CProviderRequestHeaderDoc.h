/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderRequestHeaderDoc_h_
#define CProviderRequestHeaderDoc_h_


#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestConfigDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderRequestHeader
class PROVIDERREQUESTDOC_LINKAGE CProviderRequestHeaderDoc {
public:
	CProviderRequestHeaderDoc();
	virtual ~CProviderRequestHeaderDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCProviderRequestConfigDoc requestConfig,
		const SmartPtrCPropertyCollectionDoc echoPropertyBag);

public:
	/// Accessor for the RequestConfig
	SmartPtrCProviderRequestConfigDoc getRequestConfig() const;

	/// Accessor for the EchoPropertyBag
	SmartPtrCPropertyCollectionDoc getEchoPropertyBag() const;

private:
	bool _isInitialized;

	SmartPtrCProviderRequestConfigDoc _requestConfig;
	SmartPtrCPropertyCollectionDoc _echoPropertyBag;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderRequestHeaderDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderRequestHeaderDoc);

}

#endif
