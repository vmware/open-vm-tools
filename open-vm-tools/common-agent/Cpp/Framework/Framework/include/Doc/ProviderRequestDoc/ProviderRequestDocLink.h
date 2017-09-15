/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef ProviderRequestDoc_Link_h_
#define ProviderRequestDoc_Link_h_

#ifndef PROVIDERREQUESTDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define PROVIDERREQUESTDOC_LINKAGE __declspec(dllexport)
        #else
            #define PROVIDERREQUESTDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define PROVIDERREQUESTDOC_LINKAGE
    #endif
#endif

#include "Doc/ProviderRequestDoc/ProviderRequestDocInc.h"

#endif /* ProviderRequestDoc_Link_h_ */
