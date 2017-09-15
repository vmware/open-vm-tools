/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef MgmtTypesDoc_Link_h_
#define MgmtTypesDoc_Link_h_

#ifndef MGMTTYPESDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define MGMTTYPESDOC_LINKAGE __declspec(dllexport)
        #else
            #define MGMTTYPESDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define MGMTTYPESDOC_LINKAGE
    #endif
#endif

#include "Doc/MgmtTypesDoc/MgmtTypesDocInc.h"

#endif /* MgmtTypesDoc_Link_h_ */
