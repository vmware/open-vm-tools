/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef ProviderResultsDoc_Link_h_
#define ProviderResultsDoc_Link_h_

#ifndef PROVIDERRESULTSDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define PROVIDERRESULTSDOC_LINKAGE __declspec(dllexport)
        #else
            #define PROVIDERRESULTSDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define PROVIDERRESULTSDOC_LINKAGE
    #endif
#endif


#endif /* ProviderResultsDoc_Link_h_ */
