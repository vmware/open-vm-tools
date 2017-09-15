/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef ResponseDoc_Link_h_
#define ResponseDoc_Link_h_

#ifndef RESPONSEDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define RESPONSEDOC_LINKAGE __declspec(dllexport)
        #else
            #define RESPONSEDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define RESPONSEDOC_LINKAGE
    #endif
#endif

#include "Doc/ResponseDoc/ResponseDocInc.h"

#endif /* ResponseDoc_Link_h_ */
