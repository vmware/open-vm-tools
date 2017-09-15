/*
 *  Created on: Jun 15, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_BINDINGINTERNAL_H_
#define AMQPINTEGRATIONCORE_BINDINGINTERNAL_H_


#include "ICafObject.h"

#include "amqpCore/Binding.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Interface allowing for internal management of binding integration objects
 */
struct __declspec(novtable) BindingInternal : public ICafObject {
	CAF_DECL_UUID("23D0079E-93F8-4C06-BD33-F0E795506FB2")

	/**
	 * @brief Sets the delegated Binding object
	 * @param binding the delegated binding
	 */
	virtual void setBindingInternal(SmartPtrBinding binding) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(BindingInternal);

}}
#endif /* AMQPINTEGRATIONCORE_BINDINGINTERNAL_H_ */
