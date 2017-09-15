 /*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPMETHODIMPLINT_H_
#define AMQPMETHODIMPLINT_H_

using namespace Caf::AmqpClient;

#include "TMethodImpl.h"

#include "AMQPImpl.h"

#include "amqpClient/amqpImpl/BasicProperties.h"
#include "BasicGetOkMethod.h"
#include "BasicGetEmptyMethod.h"
#include "BasicConsumeOkMethod.h"
#include "BasicDeliverMethod.h"
#include "BasicCancelOkMethod.h"
#include "BasicReturnMethod.h"
#include "BasicRecoverOkMethod.h"
#include "BasicQosOkMethod.h"

#include "ChannelOpenOkMethod.h"
#include "ChannelCloseMethod.h"
#include "ChannelCloseOkFromServerMethod.h"

#include "ExchangeDeclareOkMethod.h"
#include "ExchangeDeleteOkMethod.h"

#include "QueueDeclareOkMethod.h"
#include "QueueDeleteOkMethod.h"
#include "QueuePurgeOkMethod.h"
#include "QueueBindOkMethod.h"
#include "QueueUnbindOkMethod.h"

#endif
