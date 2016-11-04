/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CProviderCdifFormatter_h_
#define CProviderCdifFormatter_h_

#include "IProviderResponse.h"
#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentNameCollectionDoc.h"
#include "Doc/ProviderResultsDoc/CRequestIdentifierDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"

namespace Caf {

/// Sends responses/errors back to the client.
class CProviderCdifFormatter : public IProviderResponse {
public:
	CProviderCdifFormatter();
	virtual ~CProviderCdifFormatter();

public:
	void initialize(
		const SmartPtrCRequestIdentifierDoc requestIdentifier,
		const SmartPtrCSchemaDoc schema,
		const std::string outputFilePath);

	void finished();

	std::string getOutputFilePath() const;

public: // IProviderResponse
	void addInstance(const SmartPtrCDataClassInstanceDoc instance);

	void addAttachment(const SmartPtrCAttachmentDoc attachment);

private:
	void saveProviderResponse();

	SmartPtrCAttachmentDoc createAttachment() const;

	SmartPtrCAttachmentCollectionDoc createAttachmentCollection() const;

	SmartPtrCAttachmentNameCollectionDoc createAttachmentNameCollection() const;

private:
	bool _isInitialized;

	SmartPtrCRequestIdentifierDoc _requestIdentifier;
	SmartPtrCSchemaDoc _schema;
	std::string _outputFilePath;

	std::deque<std::string> _defnObjCollection;
	std::deque<SmartPtrCAttachmentDoc> _attachmentCollectionInner;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CProviderCdifFormatter);
};

}

#endif // #ifndef CProviderCdifFormatter_h_
