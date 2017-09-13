/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _MaIntegration_CConfigEnvMessageHandler_h_
#define _MaIntegration_CConfigEnvMessageHandler_h_

namespace Caf {

class CConfigEnvMessageHandler :
		public IMessageHandler,
		public IErrorProcessor {
public:
	CConfigEnvMessageHandler();
	virtual ~CConfigEnvMessageHandler();

public:
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IMessageHandler)
		CAF_QI_ENTRY(IErrorProcessor)
	CAF_END_QI()

public:
	void initialize(
		const SmartPtrIDocument& configSection);

public: // IMessageHandler
	void handleMessage(
		const SmartPtrIIntMessage& message);

	SmartPtrIIntMessage getSavedMessage() const;

	void clearSavedMessage();

public: // IErrorProcessor
	SmartPtrIIntMessage processErrorMessage(
		const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	std::string _id;
	bool _deleteSourceEntries;
	SmartPtrIConfigEnv _configEnv;
	SmartPtrIIntMessage _savedMessage;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CConfigEnvMessageHandler);
};

CAF_DECLARE_SMART_QI_POINTER(CConfigEnvMessageHandler);

}

#endif // #ifndef _MaIntegration_CConfigEnvMessageHandler_h_
