/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCmsMessageTransformerInstance_h_
#define CCmsMessageTransformerInstance_h_

#include <openssl/ssl.h>

#include "CCmsMessageAttachments.h"
#include "Common/IAppContext.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"

#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CCmsMessageTransformerInstance :
	public TCafSubSystemObjectRoot<CCmsMessageTransformerInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer {
public:
	CCmsMessageTransformerInstance();
	virtual ~CCmsMessageTransformerInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdCommIntegrationCmsMessageTransformerInstance)

	CAF_BEGIN_INTERFACE_MAP(CCmsMessageTransformerInstance)
		CAF_INTERFACE_ENTRY(IIntegrationObject)
		CAF_INTERFACE_ENTRY(IIntegrationComponentInstance)
		CAF_INTERFACE_ENTRY(ITransformer)
	CAF_END_INTERFACE_MAP()

public: // IIntegrationObject
	void initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection);

	std::string getId() const;

public: // IIntegrationComponentInstance
	void wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver);

public: // ITransformer
	SmartPtrIIntMessage transformMessage(
			const SmartPtrIIntMessage& message);

private:
	SmartPtrIIntMessage createOutgoingPayload(
			const IIntMessage::SmartPtrCHeaders& headers,
			const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
			const SmartPtrCCmsMessageAttachments& cmsMessageAttachments) const;

	SmartPtrIIntMessage createIncomingPayload(
			const IIntMessage::SmartPtrCHeaders& headers,
			const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
			const SmartPtrCCmsMessageAttachments& cmsMessageAttachments) const;

private:
	bool _isInitialized;
	std::string _id;

	std::string _workingDirectory;
	bool _isSigningEnforced;
	bool _isEncryptionEnforced;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CCmsMessageTransformerInstance);
};

}

#endif // #ifndef CCmsMessageTransformerInstance_h_
