/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#ifndef AMQPINTEGRATIONCORE_LINKAGE
#ifdef WIN32
#define AMQPINTEGRATIONCORE_LINKAGE __declspec(dllexport)
#else
#define AMQPINTEGRATIONCORE_LINKAGE
#endif
#endif

#include <CommonDefines.h>
#include <Integration.h>
#include "AmqpIntegrationCoreFunc.h"
#include "AmqpIntegrationCoreDefines.h"
#include "AmqpIntegrationExceptions.h"

#endif /* STDAFX_H_ */
