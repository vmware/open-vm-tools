/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_EXCHANGEINTERNAL_H_
#define AMQPINTEGRATIONCORE_EXCHANGEINTERNAL_H_


#include "ICafObject.h"

#include "amqpCore/Binding.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Interface allowing for internal management of exchange integration objects
 */
struct __declspec(novtable) ExchangeInternal : public ICafObject {
	CAF_DECL_UUID("5373AD13-2103-4FC4-A581-9D6D454F02A3")

	/**
	 * @brief Return the bindings defined as part of the exchange definition
	 * @return the collection of bindings
	 */
	virtual std::deque<SmartPtrBinding> getEmbeddedBindings() const = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ExchangeInternal);

}}

#endif
