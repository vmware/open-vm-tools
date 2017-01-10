/*
 *  Created on: Jul 18, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "AmqpIntegrationExceptions.h"

using namespace Caf::AmqpIntegration::AmqpIntExceptions;

AMQP_CM_DEFINE_EXCEPTION_CLASS(ReplyRequiredException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ChannelResolutionException);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ExpressionResultNull);
AMQP_CM_DEFINE_EXCEPTION_CLASS(ExpressionResultWrongType);
