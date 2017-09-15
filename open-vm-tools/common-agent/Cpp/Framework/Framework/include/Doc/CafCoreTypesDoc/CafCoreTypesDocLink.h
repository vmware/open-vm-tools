/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CafCoreTypesDoc_Link_h_
#define CafCoreTypesDoc_Link_h_

#ifndef CAFCORETYPESDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define CAFCORETYPESDOC_LINKAGE __declspec(dllexport)
        #else
            #define CAFCORETYPESDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define CAFCORETYPESDOC_LINKAGE
    #endif
#endif

#include "Doc/CafCoreTypesDoc/CafCoreTypesDocInc.h"

#endif /* CafCoreTypesDoc_Link_h_ */
