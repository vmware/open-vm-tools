/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CVersionTransformerInstance_h_
#define CVersionTransformerInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CVersionTransformerInstance :
	public TCafSubSystemObjectRoot<CVersionTransformerInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer {
public:
	CVersionTransformerInstance();
	virtual ~CVersionTransformerInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdVersionTransformerInstance)

	CAF_BEGIN_INTERFACE_MAP(CVersionTransformerInstance)
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
	SmartPtrIIntMessage transformEnvelope(
			const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
			const SmartPtrIIntMessage& message) const;

	SmartPtrIIntMessage transformPayload(
			const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
			const SmartPtrIIntMessage& message) const;

	void parseVersion(
			const std::string& payloadType,
			const std::string& version,
			std::string& majorVersion,
			std::string& minorVersion) const;

private:
	bool _isInitialized;
	std::string _id;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CVersionTransformerInstance);
};

}

#endif // #ifndef CVersionTransformerInstance_h_
