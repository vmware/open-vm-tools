/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef PersistenceDoc_Link_h_
#define PersistenceDoc_Link_h_

#ifndef PERSISTENCEDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define PERSISTENCEDOC_LINKAGE __declspec(dllexport)
        #else
            #define PERSISTENCEDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define PERSISTENCEDOC_LINKAGE
    #endif
#endif

#include "Doc/PersistenceDoc/PersistenceDocInc.h"

#endif /* PersistenceDoc_Link_h_ */
