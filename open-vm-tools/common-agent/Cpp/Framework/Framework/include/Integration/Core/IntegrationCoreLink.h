/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef INTEGRATIONCORELINK_H_
#define INTEGRATIONCORELINK_H_

#ifndef INTEGRATIONCORE_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define INTEGRATIONCORE_LINKAGE __declspec(dllexport)
        #else
            #define INTEGRATIONCORE_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define INTEGRATIONCORE_LINKAGE
    #endif
#endif

#endif /* INTEGRATIONCORELINK_H_ */
