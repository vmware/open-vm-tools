/*
 *  Created on: Jul 18, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_AMQPINTEGRATIONEXCEPTIONS_H_
#define AMQPINTEGRATIONCORE_AMQPINTEGRATIONEXCEPTIONS_H_

#include "amqpClient/api/AMQExceptions.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @brief Exceptions defined by this library
 */
namespace AmqpIntExceptions {

/** @brief A reply was required but was not produced */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ReplyRequiredException);

/** @brief Thrown by a ChannelResolver when it cannot resolve a channel name */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ChannelResolutionException);

/** @brief Thrown if an expression failed to return a result */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ExpressionResultNull);

/** @brief Thrown if an expression resolved to an incorrect result type */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ExpressionResultWrongType);

}}}

#endif /* AMQPINTEGRATIONCORE_AMQPINTEGRATIONEXCEPTIONS_H_ */
