/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (c) 2016 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef SchemaTypesDoc_Link_h_
#define SchemaTypesDoc_Link_h_

#ifndef SCHEMATYPESDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define SCHEMATYPESDOC_LINKAGE __declspec(dllexport)
        #else
            #define SCHEMATYPESDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define SCHEMATYPESDOC_LINKAGE
    #endif
#endif

#include "Doc/SchemaTypesDoc/SchemaTypesDocInc.h"

#endif /* SchemaTypesDoc_Link_h_ */
