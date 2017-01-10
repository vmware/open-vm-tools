/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#include "amqpClient/AmqpClientLink.h"
#include <CommonDefines.h>


//extern "C" {
#include <amqp_tcp_socket.h>
#include <amqp_ssl_socket.h>
#include <amqp_framing.h>
//}

#if !defined(sun)
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif /* !sun */

#define AMQP_HANDLE_INVALID NULL;

// Forward-declare because the connection creates the channel
// and the channel hangs onto the connection.
namespace Caf { namespace AmqpClient {
CAF_DECLARE_CLASS_AND_SMART_POINTER(CAmqpConnection);
CAF_DECLARE_CLASS_AND_SMART_POINTER(CAmqpChannel);
}}

#include "AmqpCommon.h"
#include "amqpClient/CAmqpChannel.h"

#include "AmqpConnection.h"
#include "AmqpAuthPlain.h"
#include "AmqpChannel.h"
#include "AmqpUtil.h"
#include "amqpImpl/amqpImplInt.h"

/**
 * @defgroup AmqpApiImpl AMQP API Implementation
 * Documentation for the implementation of the AMQP API.
 * <p>
 * These classes, methods and constants cannot be used directly by application code.
 */

/**
 * @mainpage
 * Documentation of the CAF AMQP Client Library.
 * <p>
 * This library allows applications to interact with an AMQP broker as a
 * client using first-class C++ objects representing AMQP entities and operations.
 */
#endif /* STDAFX_H_ */
