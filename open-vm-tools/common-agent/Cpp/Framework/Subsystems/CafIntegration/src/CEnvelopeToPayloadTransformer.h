/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CEnvelopeToPayloadTransformer_h_
#define CEnvelopeToPayloadTransformer_h_


#include "IBean.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IIntegrationComponent.h"

namespace Caf {

/// Sends responses/errors back to the client.
class CEnvelopeToPayloadTransformer :
	public TCafSubSystemObjectRoot<CEnvelopeToPayloadTransformer>,
	public IBean,
	public IIntegrationComponent {
public:
	CEnvelopeToPayloadTransformer();
	virtual ~CEnvelopeToPayloadTransformer();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdEnvelopeToPayloadTransformer)

	CAF_BEGIN_INTERFACE_MAP(CEnvelopeToPayloadTransformer)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IIntegrationComponent)
	CAF_END_INTERFACE_MAP()

public:
	virtual void initialize();

public: // IBean
	virtual void initializeBean(const IBean::Cargs& ctorArgs, const IBean::Cprops& properties);

	virtual void terminateBean();

public: // IIntegrationComponent
	bool isResponsible(
		const SmartPtrIDocument& configSection) const;

	SmartPtrIIntegrationObject createObject(
		const SmartPtrIDocument& configSection) const;

private:
	bool _isInitialized;
	IBean::Cargs _ctorArgs;
	IBean::Cprops _properties;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CEnvelopeToPayloadTransformer);
};

}

#endif // #ifndef CEnvelopeToPayloadTransformer_h_
