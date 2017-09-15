/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
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


#endif /* PersistenceDoc_Link_h_ */
