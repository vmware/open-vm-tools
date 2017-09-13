/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CErrorHandler_h_
#define CErrorHandler_h_

namespace Caf {

class INTEGRATIONCORE_LINKAGE CErrorHandler :
	public IErrorHandler {
public:
	CErrorHandler();
	virtual ~CErrorHandler();

public:
	void initialize(
		const SmartPtrIChannelResolver& channelResolver,
		const SmartPtrIMessageChannel& errorMessageChannel);

public: // IErrorHandler
	void handleError(
		const SmartPtrIThrowable& throwable,
		const SmartPtrIIntMessage& message) const;

private:
	bool _isInitialized;
	SmartPtrIChannelResolver _channelResolver;
	SmartPtrIMessageChannel _errorMessageChannel;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CErrorHandler);
};

CAF_DECLARE_SMART_POINTER(CErrorHandler);
}

#endif // #ifndef CErrorHandler_h_
