 /*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef AMQPMETHODIMPLINT_H_
#define AMQPMETHODIMPLINT_H_

using namespace Caf::AmqpClient;

#include "IMethod.h"
#include "TMethodImpl.h"
#include "IContentHeader.h"
#include "IServerMethod.h"

#include "AMQPImpl.h"
#include "FieldImpl.h"
#include "EnvelopeImpl.h"
#include "GetResponseImpl.h"

#include "BasicProperties.h"
#include "BasicAckMethod.h"
#include "BasicGetMethod.h"
#include "BasicGetOkMethod.h"
#include "BasicGetEmptyMethod.h"
#include "BasicPublishMethod.h"
#include "BasicConsumeMethod.h"
#include "BasicConsumeOkMethod.h"
#include "BasicDeliverMethod.h"
#include "BasicCancelMethod.h"
#include "BasicCancelOkMethod.h"
#include "BasicReturnMethod.h"
#include "BasicRecoverMethod.h"
#include "BasicRecoverOkMethod.h"
#include "BasicQosMethod.h"
#include "BasicQosOkMethod.h"
#include "BasicRejectMethod.h"

#include "ChannelOpenOkMethod.h"
#include "ChannelCloseMethod.h"
#include "ChannelCloseOkMethod.h"
#include "ChannelCloseOkFromServerMethod.h"

#include "ExchangeDeclareMethod.h"
#include "ExchangeDeclareOkMethod.h"
#include "ExchangeDeleteMethod.h"
#include "ExchangeDeleteOkMethod.h"

#include "QueueDeclareMethod.h"
#include "QueueDeclareOkMethod.h"
#include "QueueDeleteMethod.h"
#include "QueueDeleteOkMethod.h"
#include "QueuePurgeMethod.h"
#include "QueuePurgeOkMethod.h"
#include "QueueBindMethod.h"
#include "QueueBindOkMethod.h"
#include "QueueUnbindMethod.h"
#include "QueueUnbindOkMethod.h"

#endif
