/*
 *  Created on: Aug 8, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CIntegrationObjectFactory_h
#define CIntegrationObjectFactory_h


#include "IBean.h"

#include "IntegrationSubsys.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IIntegrationComponent.h"

namespace Caf {

class CIntegrationObjectFactory :
	public TCafSubSystemObjectRoot<CIntegrationObjectFactory>,
	public IBean,
	public IIntegrationComponent {
public:
	CIntegrationObjectFactory();
	virtual ~CIntegrationObjectFactory();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdIntegrationObjectFactory)

	CAF_BEGIN_INTERFACE_MAP(CIntegrationObjectFactory)
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
	CAF_CM_DECLARE_NOCOPY(CIntegrationObjectFactory);
};

}

#endif /* CIntegrationObjectFactory_h */
