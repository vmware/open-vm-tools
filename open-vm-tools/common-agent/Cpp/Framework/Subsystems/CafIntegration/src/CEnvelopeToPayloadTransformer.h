/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CEnvelopeToPayloadTransformer_h_
#define CEnvelopeToPayloadTransformer_h_

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
