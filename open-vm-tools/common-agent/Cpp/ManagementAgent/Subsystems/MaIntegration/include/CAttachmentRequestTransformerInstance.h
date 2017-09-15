/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAttachmentRequestTransformerInstance_h_
#define CAttachmentRequestTransformerInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CAttachmentRequestTransformerInstance :
	public TCafSubSystemObjectRoot<CAttachmentRequestTransformerInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer {
public:
	struct SExpandedFileAlias {
		std::string _filePath;
		std::string _encoding;
	};
	CAF_DECLARE_SMART_POINTER(SExpandedFileAlias);

public:
	CAttachmentRequestTransformerInstance();
	virtual ~CAttachmentRequestTransformerInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdAttachmentRequestTransformerInstance)

	CAF_BEGIN_INTERFACE_MAP(CAttachmentRequestTransformerInstance)
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
	std::string calcOutputDirPath(
			const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope) const;

	std::string calcFilePath(
			const UriUtils::SUriRecord& uriRecord) const;

	std::string calcRelPath(
			const std::string& filePath,
			const UriUtils::SUriRecord& uriRecord) const;

	void moveFile(
			const std::string& srcFilePath,
			const std::string& dstFilePath) const;

private:
	bool _isInitialized;
	std::string _id;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CAttachmentRequestTransformerInstance);
};

}

#endif // #ifndef CAttachmentRequestTransformerInstance_h_
