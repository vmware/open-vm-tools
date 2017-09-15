/*
 *  Created on: May 3, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "amqpClient/api/AMQExceptions.h"

using namespace Caf::AmqpClient::AmqpExceptions;

AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpTimeoutException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpNoMemoryException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpInvalidHandleException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpInvalidArgumentException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpWrongStateException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpTooManyChannelsException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpQueueFullException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpFrameTooLargeException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpIoErrorException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpProtocolErrorException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpUnimplementedException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(AmqpIoInterruptedException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(UnexpectedFrameException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(UnknownClassOrMethodException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ConnectionClosedException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ChannelClosedException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ConnectionUnexpectedCloseException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ConnectionClosedByIOException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ChannelClosedByServerException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ChannelClosedByShutdownException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ChannelClosedByUserException);
