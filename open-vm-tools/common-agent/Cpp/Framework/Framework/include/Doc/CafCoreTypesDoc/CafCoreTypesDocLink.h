/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
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


#endif /* CafCoreTypesDoc_Link_h_ */
