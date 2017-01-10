/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCafMessageHeadersWriter_h_
#define CCafMessageHeadersWriter_h_


#include "Integration/IIntMessage.h"

namespace Caf {

CAF_DECLARE_CLASS_AND_SMART_POINTER(CCafMessageHeadersWriter);

class INTEGRATIONCAF_LINKAGE CCafMessageHeadersWriter {
public:
	static SmartPtrCCafMessageHeadersWriter create();

public:
	CCafMessageHeadersWriter();
	virtual ~CCafMessageHeadersWriter();

public:
	IIntMessage::SmartPtrCHeaders getHeaders() const;

public:
	void setPayloadType(const std::string& payloadType);

	void setVersion(const std::string& version);

	void setPayloadVersion(const std::string& payloadVersion);

	void setClientId(const UUID& clientId);

	void setClientId(const std::string& clientIdStr);

	void setRequestId(const UUID& requestId);

	void setRequestId(const std::string& requestIdStr);

	void setPmeId(const std::string& pmeId);

	void setSessionId(const UUID& sessionId);

	void setSessionId(const std::string& sessionId);

	void setRelDirectory(const std::string& relDirectory);

	void setRelFilename(const std::string& relFilename);

	void setProviderUri(const std::string& providerUri);

	void setIsThrowable(const bool& isThrowable);

	void setIsMultiPart(const bool& isMultiPart);

	void setProtocol(const std::string& protocol);

	void setProtocolAddress(const std::string& protocolAddress);

	void setFlowDirection(const std::string& flowDirection);

public:
	void insertString(
		const std::string& key,
		const std::string& value);

	void insertBool(
		const std::string& key,
		const bool& value);

private:
	void initialize();

private:
	bool _isInitialized;
	IIntMessage::SmartPtrCHeaders _headers;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CCafMessageHeadersWriter);
};

}

#endif // #ifndef CCafMessageHeadersWriter_h_
