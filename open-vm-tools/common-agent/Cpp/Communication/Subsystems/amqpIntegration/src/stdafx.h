/*
 *  Created on: May 24, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#include <CommonDefines.h>
#include <Integration.h>
#include "../../../amqpCore/src/amqpCore/amqpIntegrationCoreLink.h"
#include "amqpIntegrationDefines.h"

/**
 * @defgroup IntObj Integration Objects
 * Documentation for the Integration Objects that can be declare in an
 * application context file.
 * <p>
 * The #Caf::AmqpIntegration::CachingConnectionFactoryObj bean has it's own bean definition in the
 * application context:
 * <pre>
 * \<bean
 * 	id="connectionFactory"
 * 	class="com.vmware.caf.comm.integration.amqp.caching.connection.factory"/\>
 * </pre>
 * <p>
 * All other integration objects are accessed through the IntegrationObjects bean:
 * <pre>
 * \<bean
 *    id="amqpIntegrationObjects"
 *    class="com.vmware.caf.comm.integration.objects" /\>
 * </pre>
 */

/**
 * @defgroup IntObjImpl Integration Object Implementation
 * Documentation for the implementation of the integration objects.
 * <p>
 * These classes and methods cannot be used directly by application code.
 */

/**
 * @mainpage
 * The CAF AMQP Integration Library provides integration components that enable
 * AMQP message channels, endpoints and gateways to be wired into integration-enabled
 * applications.
 */

#endif /* STDAFX_H_ */
