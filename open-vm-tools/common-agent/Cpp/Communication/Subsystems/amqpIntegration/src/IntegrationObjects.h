/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef INTEGRATIONOBJECTS_H_
#define INTEGRATIONOBJECTS_H_


#include "IBean.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IIntegrationComponent.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief The bean responsible for creating all of the AMQP integration objects.
 * <p>
 * All applications using AMQP integration must include this definition in the
 * application context:
 * <pre>
 * \<bean
 *    id="amqpIntegrationObjects"
 *    class="com.vmware.caf.comm.integration.objects" /\>
 * </pre>
 */
class IntegrationObjects :
	public TCafSubSystemObjectRoot<IntegrationObjects>,
	public IBean,
	public IIntegrationComponent {
public:
	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdIntegrationObjects)

	CAF_BEGIN_INTERFACE_MAP(IntegrationObjects)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IIntegrationComponent)
	CAF_END_INTERFACE_MAP()

public:
	IntegrationObjects();
	virtual ~IntegrationObjects();

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

#endif /* INTEGRATIONOBJECTS_H_ */
