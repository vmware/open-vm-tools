/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CCmsMessageTransformer_h_
#define CCmsMessageTransformer_h_

namespace Caf {

/// Sends responses/errors back to the client.
class CCmsMessageTransformer :
	public TCafSubSystemObjectRoot<CCmsMessageTransformer>,
	public IBean,
	public IIntegrationComponent {
public:
	CCmsMessageTransformer();
	virtual ~CCmsMessageTransformer();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdCommIntegrationCmsMessageTransformer)

	CAF_BEGIN_INTERFACE_MAP(CCmsMessageTransformer)
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
	CAF_CM_DECLARE_NOCOPY(CCmsMessageTransformer);
};

}

#endif // #ifndef CCmsMessageTransformer_h_
