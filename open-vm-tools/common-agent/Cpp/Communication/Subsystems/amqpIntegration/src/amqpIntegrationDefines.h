/*
 *  Created on: May 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONDEFINES_H_
#define AMQPINTEGRATIONDEFINES_H_

namespace Caf {

/** @brief CAF AMQP Integration */
namespace AmqpIntegration {
	/** @brief CachingConnectionFactory bean subsystem id */
	extern const char* _sObjIdAmqpCachingConnectionFactory;

	/** @brief CachingConnectionFactory bean subsystem id */
	extern const char* _sObjIdAmqpSecureCachingConnectionFactory;

	/** @brief Integration object aggregator bean subsystem id */
	extern const char* _sObjIdIntegrationObjects;
}}

#endif /* AMQPINTEGRATIONDEFINES_H_ */
