/*
 *  Created on: Nov 13, 2015
 *      Author: bwilliams
 *
 *  Copyright (c) 2015 VMware, Inc.  All rights reserved.
 *  -- VMware Confidential
 */

#ifndef _MaIntegration_IntegrationObjects_h_
#define _MaIntegration_IntegrationObjects_h_

namespace Caf { namespace MaIntegration {

class IntegrationObjects :
	public TCafSubSystemObjectRoot<IntegrationObjects>,
	public IBean,
	public IIntegrationComponent {
public:
	IntegrationObjects();
	virtual ~IntegrationObjects();

public:
	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdIntegrationObjects)

	CAF_BEGIN_INTERFACE_MAP(IntegrationObjects)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IIntegrationComponent)
	CAF_END_INTERFACE_MAP()

public: // IBean
	void initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties);

	void terminateBean();

public: // IIntegrationComponent
	bool isResponsible(
		const SmartPtrIDocument& configSection) const;

	SmartPtrIIntegrationObject createObject(
		const SmartPtrIDocument& configSection) const;

private:
	IBean::Cargs _ctorArgs;
	IBean::Cprops _properties;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(IntegrationObjects);
};

}}

#endif /* _MaIntegration_IntegrationObjects_h_ */
