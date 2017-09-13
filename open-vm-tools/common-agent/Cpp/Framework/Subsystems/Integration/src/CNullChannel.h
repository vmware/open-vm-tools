/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CNullChannel_h_
#define CNullChannel_h_

namespace Caf {

/// Sends responses/errors back to the client.
class CNullChannel :
	public TCafSubSystemObjectRoot<CNullChannel>,
	public IBean,
	public IIntegrationComponent
{
public:
	CNullChannel();
	virtual ~CNullChannel();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdNullChannel)

	CAF_BEGIN_INTERFACE_MAP(CNullChannel)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IIntegrationComponent)
	CAF_END_INTERFACE_MAP()

public:
	void initialize();

public: // IBean
	void initializeBean(const IBean::Cargs& ctorArgs, const IBean::Cprops& properties);
	void terminateBean();

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
	CAF_CM_DECLARE_NOCOPY(CNullChannel);
};

}

#endif // #ifndef CNullChannel_h_
