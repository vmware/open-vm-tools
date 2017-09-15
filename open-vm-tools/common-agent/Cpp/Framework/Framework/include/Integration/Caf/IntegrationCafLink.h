/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef INTEGRATIONCAFLINK_H_
#define INTEGRATIONCAFLINK_H_

#ifndef INTEGRATIONCAF_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define INTEGRATIONCAF_LINKAGE __declspec(dllexport)
        #else
            #define INTEGRATIONCAF_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define INTEGRATIONCAF_LINKAGE
    #endif
#endif

#endif /* INTEGRATIONCAFLINK_H_ */
