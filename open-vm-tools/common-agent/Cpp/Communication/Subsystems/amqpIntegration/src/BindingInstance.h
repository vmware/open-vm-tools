/*
 *  Created on: Jun 15, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BINDINGINSTANCE_H_
#define BINDINGINSTANCE_H_

#include "Integration/IIntegrationObject.h"
#include "Integration/IDocument.h"
#include "amqpCore/Binding.h"
#include "amqpCore/BindingInternal.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObj
 * @brief An Integration Object implementing Caf::AmqpIntegration::Binding
 * <p>
 * Currently this object is only created indirectly through rabbit-binding
 * declarations in exchange declarations.
 */
class BindingInstance :
	public IIntegrationObject,
	public BindingInternal,
	public Binding {
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(BindingInternal)
		CAF_QI_ENTRY(Binding)
	CAF_END_QI()

public:
	BindingInstance();
	virtual ~BindingInstance();

public: // IIntegrationObject
	void initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection);

	std::string getId() const;

public: // BindingInternal
	void setBindingInternal(SmartPtrBinding binding);

public: // Binding
	std::string getQueue() const;
	std::string getExchange() const;
	std::string getRoutingKey() const;

private:
	std::string _id;
	SmartPtrBinding _binding;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BindingInstance);
};
CAF_DECLARE_SMART_QI_POINTER(BindingInstance);

}}

#endif /* BINDINGINSTANCE_H_ */
