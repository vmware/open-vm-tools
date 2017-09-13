/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CProtocolHeaderEnricher_h_
#define CProtocolHeaderEnricher_h_

namespace Caf {

/// Sends responses/errors back to the client.
class CProtocolHeaderEnricher :
	public TCafSubSystemObjectRoot<CProtocolHeaderEnricher>,
	public IBean,
	public IIntegrationComponent {
public:
	CProtocolHeaderEnricher();
	virtual ~CProtocolHeaderEnricher();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdCommIntegrationProtocolHeaderEnricher)

	CAF_BEGIN_INTERFACE_MAP(CProtocolHeaderEnricher)
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
	CAF_CM_DECLARE_NOCOPY(CProtocolHeaderEnricher);
};

}

#endif // #ifndef CProtocolHeaderEnricher_h_
