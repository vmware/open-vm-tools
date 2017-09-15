/*
 *	 Author: brets
 *  Created: Nov 20, 2015
 *
 *	Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CProviderExecutorRequest_h_
#define CProviderExecutorRequest_h_


#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"
#include "Integration/IIntMessage.h"

namespace Caf {

/// TODO - describe class
class CProviderExecutorRequest {
public:
	CProviderExecutorRequest();
	virtual ~CProviderExecutorRequest();

public:
	void initialize(const SmartPtrIIntMessage& request);

	const SmartPtrIIntMessage getInternalRequest() const;
	const SmartPtrCProviderRequestDoc getRequest() const;
	const std::string& getOutputDirectory() const;
	const std::string& getProviderUri() const;

private:
	bool _isInitialized;
	SmartPtrIIntMessage _internalRequest;
	SmartPtrCProviderRequestDoc _request;
	std::string _outputDir;
	std::string _providerUri;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CProviderExecutorRequest);
};

CAF_DECLARE_SMART_POINTER(CProviderExecutorRequest);

}

#endif // #ifndef CProviderExecutorRequest_h_
