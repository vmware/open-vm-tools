/*
 *	 Author: bwilliams
 *  Created: July 3, 2015
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCmsMessageAttachments_h_
#define CCmsMessageAttachments_h_



#include "CCmsMessage.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"

namespace Caf {

class CCmsMessageAttachments {
public:
	CCmsMessageAttachments();
	virtual ~CCmsMessageAttachments();

public:
	void initialize(
			const SmartPtrCCmsMessage& cmsMessage);

public:
	void enforceSecurityOnAttachments(
			const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection,
			const bool isSigningEnforced,
			const bool isEncryptionEnforced) const;

	std::deque<SmartPtrCAttachmentDoc> encryptAndSignAttachments(
			const std::deque<SmartPtrCAttachmentDoc>& sourceAttachmentCollection) const;

	SmartPtrCAttachmentDoc encryptAndSignAttachment(
			const std::string& sourceAttachmentPath,
			const SmartPtrCAttachmentDoc& sourceAttachment,
			const std::map<std::string, std::string>& uriParameters = std::map<std::string, std::string>()) const;

	std::deque<SmartPtrCAttachmentDoc> decryptAndVerifyAttachments(
			const std::deque<SmartPtrCAttachmentDoc>& sourceAttachmentCollection) const;

	SmartPtrCAttachmentDoc decryptAndVerifyAttachment(
			const std::string& sourceAttachmentPath,
			const SmartPtrCAttachmentDoc& sourceAttachment,
			const std::map<std::string, std::string>& uriParameters) const;

private:
	void signAttachment(
			const std::string& sourceAttachmentPath,
			const SmartPtrCAttachmentDoc& sourceAttachment,
			const std::map<std::string, std::string>& uriParameters,
			std::string& destAttachmentPath,
			SmartPtrCAttachmentDoc& destAttachment) const;

	void verifyAttachment(
			const std::string& sourceAttachmentPath,
			const SmartPtrCAttachmentDoc& sourceAttachment,
			const std::map<std::string, std::string>& uriParameters,
			std::string& destAttachmentPath,
			SmartPtrCAttachmentDoc& destAttachment) const;

	void encryptAttachment(
			const std::string& sourceAttachmentPath,
			const SmartPtrCAttachmentDoc& sourceAttachment,
			const std::map<std::string, std::string>& uriParameters,
			std::string& destAttachmentPath,
			SmartPtrCAttachmentDoc& destAttachment) const;

	void decryptAttachment(
			const std::string& sourceAttachmentPath,
			const SmartPtrCAttachmentDoc& sourceAttachment,
			const std::map<std::string, std::string>& uriParameters,
			std::string& destAttachmentPath,
			SmartPtrCAttachmentDoc& destAttachment) const;

private:
	void removeStr(
			std::string& sourceStr,
			const std::string& strToRemove) const;

	void enforceSigning(
			const bool isSigningEnforced,
			const SmartPtrCAttachmentDoc& attachment) const;

	void enforceEncryption(
			const bool isEncryptionEnforced,
			const SmartPtrCAttachmentDoc& attachment) const;

private:
	bool _isInitialized;

	SmartPtrCCmsMessage _cmsMessage;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CCmsMessageAttachments);
};

CAF_DECLARE_SMART_POINTER(CCmsMessageAttachments);

}

#endif // #ifndef CCmsMessageAttachments_h_
