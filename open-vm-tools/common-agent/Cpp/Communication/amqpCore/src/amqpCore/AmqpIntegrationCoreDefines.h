/*
 *  Created on: Jul 31, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCOREDEFINES_H_
#define AMQPINTEGRATIONCOREDEFINES_H_

namespace Caf { namespace AmqpIntegration {
/**
 * @brief Message acknowledgment mode flags
 */
typedef enum {
	/** @brief No acks will be sent. AMQP broker will assume all messages are acked. */
	ACKNOWLEDGEMODE_NONE,

	/** @brief The listener must acknowledge all messages by calling Channel.basicAck() */
	ACKNOWLEDGEMODE_MANUAL,

	/** @brief The container will acknowledge messages automatically. */
	ACKNOWLEDGEMODE_AUTO
} AcknowledgeMode;

}}

#endif
