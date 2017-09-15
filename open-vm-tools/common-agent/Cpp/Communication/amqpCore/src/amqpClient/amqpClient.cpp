/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "amqpClient/api/amqpClient.h"

namespace Caf {
namespace AmqpClient {
	const char* DEFAULT_USER = "guest";
	const char* DEFAULT_PASS ="guest";
	const char* DEFAULT_VHOST = "/";
	const char* DEFAULT_PROTOCOL = "amqp";
	const char* DEFAULT_HOST = "localhost";
}}

#ifdef WIN32
extern "C" BOOL APIENTRY DllMain(HINSTANCE hModule, uint32 dwReason, LPVOID)
{
    return TRUE;
}
#endif

/**
 * @todo Make number of frames processed per thread slice configurable
 * @todo Make # threads in the connection thrad pool configurable
 * @todo Make thread pool refresh rate configurable
 * @todo Create stats module so we can figure out how to turn performance
 */
class amqpClientTodo {
};
