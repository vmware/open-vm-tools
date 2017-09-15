/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef ProviderInfraDoc_Link_h_
#define ProviderInfraDoc_Link_h_

#ifndef PROVIDERINFRADOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define PROVIDERINFRADOC_LINKAGE __declspec(dllexport)
        #else
            #define PROVIDERINFRADOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define PROVIDERINFRADOC_LINKAGE
    #endif
#endif

#include "Doc/ProviderInfraDoc/ProviderInfraDocInc.h"

#endif /* ProviderInfraDoc_Link_h_ */
