/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCafMessageHeaders_h_
#define CCafMessageHeaders_h_


#include "IVariant.h"
#include "Integration/IIntMessage.h"

namespace Caf {

CAF_DECLARE_CLASS_AND_SMART_POINTER(CCafMessageHeaders);

class INTEGRATIONCAF_LINKAGE CCafMessageHeaders {
public:
	static SmartPtrCCafMessageHeaders create(
			const IIntMessage::SmartPtrCHeaders& headers);

public:
	CCafMessageHeaders();
	virtual ~CCafMessageHeaders();

public:
	IIntMessage::SmartPtrCHeaders getHeaders() const;

public:
	std::string getPayloadType() const;

	std::string getPayloadTypeOpt(
			const std::string& defaultVal = std::string()) const;

public:
	std::string getVersion() const;

	std::string getVersionOpt(
			const std::string& defaultVal = "1.0") const;

public:
	UUID getClientId() const;
	std::string getClientIdStr() const;

	UUID getClientIdOpt(
			const UUID defaultVal = CAFCOMMON_GUID_NULL) const;
	std::string getClientIdStrOpt(
			const std::string& defaultVal = std::string()) const;

public:
	UUID getRequestId() const;
	std::string getRequestIdStr() const;

	UUID getRequestIdOpt(
			const UUID defaultVal = CAFCOMMON_GUID_NULL) const;
	std::string getRequestIdStrOpt(
			const std::string& defaultVal = std::string()) const;

public:
	std::string getPmeId() const;

	std::string getPmeIdOpt(
			const std::string& defaultVal = std::string()) const;

public:
	UUID getSessionId() const;
	std::string getSessionIdStr() const;

	UUID getSessionIdOpt(
			const UUID defaultVal = CAFCOMMON_GUID_NULL) const;
	std::string getSessionIdStrOpt(
			const std::string& defaultVal = std::string()) const;

public:
	std::string getRelDirectory() const;

	std::string getRelDirectoryOpt(
			const std::string& defaultVal = std::string()) const;

public:
	std::string getRelFilename() const;

	std::string getRelFilenameOpt(
			const std::string& defaultVal) const;

public:
	std::string getProviderUri() const;

	std::string getProviderUriOpt(
			const std::string& defaultVal) const;

public:
	std::string getFlowDirection() const;

	std::string getFlowDirectionOpt(
			const std::string& defaultVal) const;

public:
	std::string getRequiredStr(
			const std::string& key) const;

	std::string getOptionalStr(
			const std::string& key,
			const std::string& defaultVal = std::string()) const;

	bool getRequiredBool(
			const std::string& key) const;

	bool getOptionalBool(
			const std::string& key,
			const bool defaultVal = false) const;

private:
	void initialize(
			const IIntMessage::SmartPtrCHeaders& headers);

private:
	SmartPtrIVariant findOptionalHeader(
		const std::string& key) const;

	SmartPtrIVariant findRequiredHeader(
		const std::string& key) const;

private:
	bool _isInitialized;
	IIntMessage::SmartPtrCHeaders _headers;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CCafMessageHeaders);
};

}

#endif // #ifndef CCafMessageHeaders_h_
