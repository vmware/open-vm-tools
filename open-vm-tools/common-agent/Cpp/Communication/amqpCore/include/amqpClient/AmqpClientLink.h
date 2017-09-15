/*
 *	 Author: bwilliams
 *  Created: 10/19/2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef AMQP_CLIENTLINK_H_
#define AMQP_CLIENTLINK_H_

#ifndef AMQPCLIENT_LINKAGE
    #ifdef WIN32
        #ifdef AMQP_CLIENT
            #define AMQPCLIENT_LINKAGE __declspec(dllexport)
        #else
            #define AMQPCLIENT_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define AMQPCLIENT_LINKAGE
    #endif
#endif

#endif /* AMQP_CLIENTLINK_H_ */
