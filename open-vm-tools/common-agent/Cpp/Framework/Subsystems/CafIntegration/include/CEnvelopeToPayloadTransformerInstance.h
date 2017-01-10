/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CEnvelopeToPayloadTransformerInstance_h_
#define CEnvelopeToPayloadTransformerInstance_h_

#include "Common/IAppContext.h"
#include "CafIntegrationSubsys.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"

#include "Integration/IErrorProcessor.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CEnvelopeToPayloadTransformerInstance :
	public TCafSubSystemObjectRoot<CEnvelopeToPayloadTransformerInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer,
	public IErrorProcessor {
public:
	struct SExpandedFileAlias {
		std::string _filePath;
		std::string _encoding;
	};
	CAF_DECLARE_SMART_POINTER(SExpandedFileAlias);

public:
	CEnvelopeToPayloadTransformerInstance();
	virtual ~CEnvelopeToPayloadTransformerInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdEnvelopeToPayloadTransformerInstance)

	CAF_BEGIN_INTERFACE_MAP(CEnvelopeToPayloadTransformerInstance)
		CAF_INTERFACE_ENTRY(IIntegrationObject)
		CAF_INTERFACE_ENTRY(IIntegrationComponentInstance)
		CAF_INTERFACE_ENTRY(ITransformer)
		CAF_INTERFACE_ENTRY(IErrorProcessor)
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

public: // IErrorProcessor
	SmartPtrIIntMessage processErrorMessage(
		const SmartPtrIIntMessage& message);

private:
	SmartPtrCDynamicByteArray findPayload(
			const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection) const;

	std::deque<SmartPtrCAttachmentDoc> removePayload(
		const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection) const;

private:
	bool _isInitialized;
	std::string _id;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CEnvelopeToPayloadTransformerInstance);
};

}

#endif // #ifndef CEnvelopeToPayloadTransformerInstance_h_
