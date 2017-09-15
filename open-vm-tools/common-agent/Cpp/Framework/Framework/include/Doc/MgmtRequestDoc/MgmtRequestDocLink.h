/*
 *  Author: bwilliams
 *  Created: February 25, 2016
 *
 *  Copyright (C) 2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef MgmtRequestDoc_Link_h_
#define MgmtRequestDoc_Link_h_

#ifndef MGMTREQUESTDOC_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define MGMTREQUESTDOC_LINKAGE __declspec(dllexport)
        #else
            #define MGMTREQUESTDOC_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define MGMTREQUESTDOC_LINKAGE
    #endif
#endif


#endif /* MgmtRequestDoc_Link_h_ */
