/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CIntMessage_h_
#define CIntMessage_h_


#include "Integration/IIntMessage.h"

#include "ICafObject.h"
#include "IVariant.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CIntMessage :
	public IIntMessage {
public:
	static SmartPtrCHeaders mergeHeaders(
		const SmartPtrCHeaders& newHeaders,
		const SmartPtrCHeaders& origHeaders);

public:
	CIntMessage();
	virtual ~CIntMessage();

public:
	void initializeStr(
		const std::string& payloadStr,
		const SmartPtrCHeaders& newHeaders,
		const SmartPtrCHeaders& origHeaders);

	void initialize(
		const SmartPtrCDynamicByteArray& payload,
		const SmartPtrCHeaders& newHeaders,
		const SmartPtrCHeaders& origHeaders);

public: // IIntMessage
	SmartPtrCDynamicByteArray getPayload() const;

	std::string getPayloadStr() const;

	UUID getMessageId() const;

	std::string getMessageIdStr() const;

	SmartPtrCHeaders getHeaders() const;

	SmartPtrIVariant findOptionalHeader(
		const std::string& key) const;

	SmartPtrIVariant findRequiredHeader(
		const std::string& key) const;

	std::string findOptionalHeaderAsString(
		const std::string& key) const;

	std::string findRequiredHeaderAsString(
		const std::string& key) const;

	SmartPtrICafObject findOptionalObjectHeader(
		const std::string& key) const;

	SmartPtrICafObject findRequiredObjectHeader(
		const std::string& key) const;

private:
	bool _isInitialized;
	UUID _messageId;
	SmartPtrCDynamicByteArray _payload;
	SmartPtrCHeaders _headers;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CIntMessage);
};

CAF_DECLARE_SMART_POINTER(CIntMessage);

}

#endif // #ifndef _IntegrationContracts_CIntMessage_h_
