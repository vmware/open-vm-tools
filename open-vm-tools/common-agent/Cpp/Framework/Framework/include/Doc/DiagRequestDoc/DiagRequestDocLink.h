/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef DiagRequestDoc_Link_h_
#define DiagRequestDoc_Link_h_

#ifndef DIAGREQUESTDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define DIAGREQUESTDOC_LINKAGE __declspec(dllexport)
        #else
            #define DIAGREQUESTDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define DIAGREQUESTDOC_LINKAGE
    #endif
#endif

#include "Doc/DiagRequestDoc/DiagRequestDocInc.h"

#endif /* DiagRequestDoc_Link_h_ */
