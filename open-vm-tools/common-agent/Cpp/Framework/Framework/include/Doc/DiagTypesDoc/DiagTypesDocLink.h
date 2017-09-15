/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef DiagTypesDoc_Link_h_
#define DiagTypesDoc_Link_h_

#ifndef DIAGTYPESDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define DIAGTYPESDOC_LINKAGE __declspec(dllexport)
        #else
            #define DIAGTYPESDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define DIAGTYPESDOC_LINKAGE
    #endif
#endif


#endif /* DiagTypesDoc_Link_h_ */
