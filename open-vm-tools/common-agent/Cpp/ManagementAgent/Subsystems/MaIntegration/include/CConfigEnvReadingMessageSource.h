/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _MaIntegration_CConfigEnvReadingMessageSource_h_
#define _MaIntegration_CConfigEnvReadingMessageSource_h_

#include "IConfigEnv.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/Core/CAbstractPollableChannel.h"

namespace Caf {

class CConfigEnvReadingMessageSource :
	public CAbstractPollableChannel {
private:
	typedef std::map<std::string, bool> CFileCollection;
	CAF_DECLARE_SMART_POINTER(CFileCollection);

public:
	CConfigEnvReadingMessageSource();
	virtual ~CConfigEnvReadingMessageSource();

public:
	void initialize(
		const SmartPtrIDocument& configSection,
		const SmartPtrIConfigEnv& configEnv);

protected: // CAbstractPollableChannel
	bool doSend(
			const SmartPtrIIntMessage& message,
			int32 timeout);

	SmartPtrIIntMessage doReceive(const int32 timeout);

private:
	bool _isInitialized;
	std::string _id;

	SmartPtrIConfigEnv _configEnv;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CConfigEnvReadingMessageSource);
};

CAF_DECLARE_SMART_POINTER(CConfigEnvReadingMessageSource);

}

#endif // #ifndef _MaIntegration_CConfigEnvReadingMessageSource_h_
